// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassMovementProcessor.h"

#include "AI/AshSquadSubsystem.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/AshFlowFieldSubsystem.h"
#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"

namespace
{
	/** Resolves the nearest base not owned by SoldierTeam (enemy or neutral). Returns false if none. */
	bool FindNearestCapturableBase(const UWorld* World, EAshTeamId SoldierTeam, const FVector& From, FVector& OutLocation)
	{
		const UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr;
		if (!BaseSubsystem)
		{
			return false;
		}

		float BestDistSq = TNumericLimits<float>::Max();
		bool bFound = false;
		for (const AAshBaseActor* Base : BaseSubsystem->GetAllBases())
		{
			if (!Base || Base->GetOwningTeam() == SoldierTeam)
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(From, Base->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutLocation = Base->GetActorLocation();
				bFound = true;
			}
		}
		return bFound;
	}

	/** Hashes a world position to a 2D grid cell for neighbour lookups. */
	FORCEINLINE FIntPoint PositionToCell(const FVector& Position, float CellSize)
	{
		return FIntPoint(FMath::FloorToInt(Position.X / CellSize), FMath::FloorToInt(Position.Y / CellSize));
	}
}

UAshMassMovementProcessor::UAshMassMovementProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Reads the squad subsystem and base actors on the game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshPlayerDisplacementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshFormationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshSquadFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	// ReadWrite: the movement processor drifts FAshSoldierStateFragment::SlotAngle each frame so a
	// surrounding (waiting) soldier orbits its target (Phase 28).
	EntityQuery.AddRequirement<FAshSoldierStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	// ReadWrite: the movement processor owns the stun countdown + decaying knockback slide (Phase 32).
	EntityQuery.AddRequirement<FAshStunFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const UWorld* World = Context.GetWorld();
	const UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
	UAshFlowFieldSubsystem* FlowField = (GroupNavMode == EAshGroupNavMode::FlowField && World)
		? World->GetSubsystem<UAshFlowFieldSubsystem>() : nullptr;
	const float ArrivalToleranceSq = ArrivalTolerance * ArrivalTolerance;
	const APawn* PlayerPawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	FVector PlayerVelocity = PlayerPawn ? PlayerPawn->GetVelocity() : FVector::ZeroVector;
	PlayerVelocity.Z = 0.f;
	const bool bPlayerCanPush = PlayerPawn && PlayerPushRadius > 0.f && PlayerPushSpeed > 0.f
		&& PlayerVelocity.SizeSquared2D() > FMath::Square(10.f);

	// Per-unit-type tunable resolvers: use the entity's behavior config when present, else this
	// processor's fallbacks (so a soldier with no config still behaves sensibly).
	const auto SepRadiusOf   = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->SeparationRadius      : SeparationRadius; };
	const auto SepStrengthOf = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->SeparationStrength    : SeparationStrength; };
	const auto SepRelaxOf    = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->SeparationRelaxation  : SeparationRelaxation; };
	const auto MaxPushOf     = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->MaxPushPerNeighbor     : MaxPushPerNeighbor; };
	const auto AnchorOf      = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->CombatAnchorResistance : CombatAnchorResistance; };
	const auto TurnRateOf    = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->TurnRateDegPerSec      : FacingTurnRateDegPerSec; };
	const auto SlowdownOf    = [this](const UAshSoldierBehaviorConfig* C){ return C ? C->ArrivalSlowdownRadius  : ArrivalSlowdownRadius; };
	// Surround ring (Phase 28): how far the waiting ring sits beyond the striker standoff, and how fast
	// waiting soldiers orbit. Fallbacks mirror UAshSoldierBehaviorConfig's defaults.
	const auto SurroundGapOf      = [](const UAshSoldierBehaviorConfig* C){ return C ? C->SurroundRingGap         : 150.f; };
	const auto SurroundOrbitOf    = [](const UAshSoldierBehaviorConfig* C){ return C ? C->SurroundOrbitSpeed      : 35.f; };
	// Opposing anti-stack (Phase 26). Default 0 = the old hard standoff (opposing lines never overlap).
	const auto EnemySepRadiusOf   = [](const UAshSoldierBehaviorConfig* C){ return C ? C->EnemySeparationRadius   : 0.f; };
	const auto EnemySepStrengthOf = [](const UAshSoldierBehaviorConfig* C){ return C ? C->EnemySeparationStrength : 0.f; };

	// Size the avoidance grid to the largest separation radius in play so a 3x3 neighbourhood always
	// covers any soldier's personal space, even with mixed unit types.
	float MaxSepRadius = 0.f;
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const UAshSoldierBehaviorConfig* B = BehaviorList[It].Behavior;
			// Cover both same-team and (opt-in) enemy anti-stack radii so the 3x3 sweep never misses one.
			MaxSepRadius = FMath::Max3(MaxSepRadius, SepRadiusOf(B), EnemySepRadiusOf(B));
		}
	});

	const bool bAvoidance = MaxSepRadius > 0.f;
	const float CellSize = FMath::Max(1.f, MaxSepRadius);

	// Inter-soldier avoidance: hash every soldier's *current* position AND team into a uniform grid
	// once, so the steering pass finds crowding neighbours in O(1). Team is stored because same-team
	// separation (spacing within a formation) and enemy separation (a weak anti-stack so opposing lines
	// can jumble without perfectly overlapping, Phase 26) are applied with different radii/strengths.
	// Positions are this frame's pre-integration values, so the pass is order-independent.
	struct FNeighbour { FVector Pos; EAshTeamId Team; };
	TMap<FIntPoint, TArray<FNeighbour>> SpatialGrid;
	if (bAvoidance)
	{
		EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
			const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
			for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
			{
				const FVector& P = MovementList[It].Position;
				SpatialGrid.FindOrAdd(PositionToCell(P, CellSize)).Add({ P, TeamList[It].TeamId });
			}
		});
	}

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FAshMovementFragment> MovementList = ChunkContext.GetMutableFragmentView<FAshMovementFragment>();
		const TArrayView<FAshPlayerDisplacementFragment> DisplacementList = ChunkContext.GetMutableFragmentView<FAshPlayerDisplacementFragment>();
		const TConstArrayView<FAshFormationFragment> FormationList = ChunkContext.GetFragmentView<FAshFormationFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshCombatFragment> CombatList = ChunkContext.GetFragmentView<FAshCombatFragment>();
		const TConstArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetFragmentView<FAshCombatTargetFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetMutableFragmentView<FAshSoldierStateFragment>();
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();
		const TArrayView<FAshStunFragment> StunList = ChunkContext.GetMutableFragmentView<FAshStunFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshMovementFragment& Movement = MovementList[It];
			FAshPlayerDisplacementFragment& Displacement = DisplacementList[It];
			const FAshFormationFragment& Formation = FormationList[It];
			const FAshCombatFragment& Combat = CombatList[It];
			const FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];
			const FAshSquadFragment& Squad = SquadList[It];
			FAshSoldierStateFragment& StateFrag = StateList[It];
			const EAshSoldierState State = StateFrag.State;
			const UAshSoldierBehaviorConfig* Cfg = BehaviorList[It].Behavior;
			const bool bAlive = HealthList[It].CurrentHealth > 0.f;

			if (!bAlive)
			{
				Movement.Velocity = FVector::ZeroVector;
				continue;
			}

			// Hit-reaction stun (Phase 32): a struck soldier neither steers nor strikes (combat processor
			// gates separately) while stunned — it only slides under its decaying knockback. The shove eases
			// to zero over the stun window (scaled by remaining/duration) so it reads as a brief shove, not a
			// teleport. Separation/objective steering resumes the moment the stun ends.
			FAshStunFragment& Stun = StunList[It];
			if (Stun.StunTimeRemaining > 0.f)
			{
				Stun.StunTimeRemaining = FMath::Max(0.f, Stun.StunTimeRemaining - DeltaTime);
				const float Alpha = (Stun.StunDuration > KINDA_SMALL_NUMBER)
					? FMath::Clamp(Stun.StunTimeRemaining / Stun.StunDuration, 0.f, 1.f) : 0.f;
				Movement.Velocity = Stun.KnockbackVelocity * Alpha;
				Movement.Position += Movement.Velocity * DeltaTime;
				continue;
			}

			const float Now = World ? World->GetTimeSeconds() : 0.f;
			if (!Displacement.bReturningToBase && Now - Displacement.LastPushedTime > PlayerPushBaseHoldSeconds)
			{
				Displacement.BasePosition = Movement.Position;
			}

			if (!Displacement.bReturningToBase
				&& FVector::DistSquared2D(Movement.Position, Displacement.BasePosition) > FMath::Square(MaxPlayerForcedDisplacement))
			{
				Displacement.bReturningToBase = true;
			}

			// 1. Resolve a destination, in priority order. bGroupObjective marks a shared goal
			//    (squad objective / target base) eligible for flow-field steering; a combat
			//    target is chased directly regardless of nav mode. TargetPos is also captured for
			//    target-aware facing below.
			FVector Destination = Movement.Position;
			bool bHasDestination = false;
			bool bGroupObjective = false;
			float StopDistance = ArrivalTolerance;
			FVector TargetPos = FVector::ZeroVector;
			bool bHasTargetPos = false;

			if (Displacement.bReturningToBase)
			{
				Destination = Displacement.BasePosition;
				bHasDestination = true;
				StopDistance = FMath::Max(ArrivalTolerance, 25.f);
			}
			else if (CombatTarget.TargetType == EAshCombatTargetType::Actor && CombatTarget.ActorTarget)
			{
				Destination = CombatTarget.ActorTarget->GetActorLocation();
				TargetPos = Destination;
				bHasTargetPos = true;
				bHasDestination = true;
				StopDistance = FMath::Max(ArrivalTolerance, Combat.AttackRange * 0.9f);
			}
			else if (Combat.Target.IsSet() && EntityManager.IsEntityValid(Combat.Target))
			{
				if (const FAshMovementFragment* TargetMovement = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(Combat.Target))
				{
					Destination = TargetMovement->Position;
					TargetPos = TargetMovement->Position;
					bHasTargetPos = true;
					bHasDestination = true;
					// Stop just inside attack range so the combat processor can strike.
					StopDistance = FMath::Max(ArrivalTolerance, Combat.AttackRange * 0.9f);
				}
			}

			// Target-loss hold (Phase 26): an Engage/Attack soldier whose target died has no resolved
			// target this frame. Don't fall back to the squad/base objective (which would march it out of
			// the melee); let it hold at the contact (formation desired = the contact anchor) so the
			// behavior processor can hand it a fresh nearby enemy on its next AI-LOD tick.
			const bool bHoldNoTarget = !bHasTargetPos && !Displacement.bReturningToBase
				&& (State == EAshSoldierState::Attack || State == EAshSoldierState::Engage
					|| State == EAshSoldierState::Surround);

			if (!bHasDestination && Formation.bHasDesiredPosition)
			{
				Destination = Formation.DesiredWorldPosition;
				bHasDestination = true;
				StopDistance = FormationStopDistance;
			}

			if (!bHoldNoTarget && !bHasDestination && SquadSubsystem)
			{
				FVector SquadObjective;
				float SquadFormationRadius = 0.f;
				if (SquadSubsystem->GetSquadObjective(Squad.SquadId, SquadObjective, SquadFormationRadius))
				{
					Destination = SquadObjective;
					// A general-led squad publishes a FormationRadius: members stop in a ring at that
					// radius around the general's point instead of all converging on it (Phase 22).
					// Legacy commander-driven squads leave the radius 0 and keep the arrival tolerance.
					if (SquadFormationRadius > 0.f) { StopDistance = FMath::Max(StopDistance, SquadFormationRadius); }
					bHasDestination = true;
					bGroupObjective = true;
				}
			}

			if (!bHoldNoTarget && !bHasDestination)
			{
				FVector BaseLocation;
				if (FindNearestCapturableBase(World, TeamList[It].TeamId, Movement.Position, BaseLocation))
				{
					Destination = BaseLocation;
					bHasDestination = true;
					bGroupObjective = true;
				}
			}

			// 2. Steering velocity (pre-separation). An Attack-state soldier holds its ground (anchored)
			//    so it does not creep forward while striking — the front line stays put (Phase 20). When
			//    advancing on a *group* objective the speed eases in within ArrivalSlowdownRadius so the
			//    soldier settles onto the goal instead of ramming it at full speed and rebounding — the
			//    ram-and-rebound was a main cause of crowd shaking (Phase 20.1). AdvanceDir is captured
			//    for the distributed-deployment test below.
			FVector SteerVelocity = FVector::ZeroVector;
			FVector AdvanceDir = FVector::ZeroVector;
			bool bAdvancing = false;

			// 2a. Combat approach (close to striking range, then hold — no backpedal). A soldier with a
			//     combat target steers in until it is just inside attack range, then holds its ground; it
			//     never moves backward. Same-team separation seats the final spacing, and facing (step 5)
			//     keeps it pointed at the enemy while it holds and strikes.
			const bool bCombatApproach = bHasTargetPos && !Displacement.bReturningToBase;
			if (bCombatApproach)
			{
				const float AttackRange = Combat.AttackRange;
				const float StandoffDist = FMath::Max(
					Cfg ? Cfg->MinCombatSpacing : 110.f,
					AttackRange * (Cfg ? Cfg->AttackStandoffScale : 0.85f));

				if (State == EAshSoldierState::Surround)
				{
					// Outer (waiting) ring: orbit slowly around the target at a radius past strike range, so a
					// few soldiers fight at the inner standoff while the surplus circle and menace (Phase 28 —
					// the Musou crowd). The slot bearing drifts each frame so the whole ring rotates; the
					// soldier tracks its moving slot without overshoot (the speed is capped to land exactly on
					// it), which reads as a smooth circle-around rather than a queue.
					StateFrag.SlotAngle += FMath::DegreesToRadians(SurroundOrbitOf(Cfg)) * DeltaTime;
					const float OuterRadius = FMath::Max(StandoffDist + SurroundGapOf(Cfg), AttackRange * 1.15f);
					const FVector SlotPos = TargetPos
						+ FVector(FMath::Cos(StateFrag.SlotAngle), FMath::Sin(StateFrag.SlotAngle), 0.f) * OuterRadius;
					FVector ToSlot = SlotPos - Movement.Position;
					ToSlot.Z = 0.f;
					const float Dist = ToSlot.Size();
					if (Dist > KINDA_SMALL_NUMBER)
					{
						const FVector Dir = ToSlot / Dist;
						const float Speed = FMath::Min(Movement.MoveSpeed, Dist / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER));
						SteerVelocity = Dir * Speed;
						AdvanceDir = Dir;
						bAdvancing = true;
					}
				}
				else
				{
					FVector ToTarget = TargetPos - Movement.Position;
					ToTarget.Z = 0.f;
					const float Dist = ToTarget.Size();
					if (Dist > StandoffDist && Dist > KINDA_SMALL_NUMBER)
					{
						const FVector Dir = ToTarget / Dist;
						SteerVelocity = Dir * Movement.MoveSpeed; // close in to strike
						AdvanceDir = Dir;
						bAdvancing = true;
					}
					// else: within striking standoff — hold; same-team separation seats spacing (no backstep).
				}
			}
			else if ((Displacement.bReturningToBase || State != EAshSoldierState::Attack) && bHasDestination)
			{
				const FVector ToDest = Destination - Movement.Position;
				const float DistSq = ToDest.SizeSquared2D();
				if (DistSq > FMath::Square(StopDistance) && DistSq > ArrivalToleranceSq)
				{
					// Flow-field direction for shared objectives (Phase 14); otherwise straight line.
					FVector Dir;
					if (bGroupObjective && FlowField)
					{
						FVector FlowDir;
						Dir = FlowField->GetFlowDirection(Destination, Movement.Position, FlowDir)
							? FlowDir
							: ToDest.GetSafeNormal2D();
					}
					else
					{
						Dir = ToDest.GetSafeNormal2D();
					}

					float Speed = Displacement.bReturningToBase ? ForcedReturnSpeed : Movement.MoveSpeed;
					if (bGroupObjective)
					{
						const float Slowdown = SlowdownOf(Cfg);
						if (Slowdown > 0.f)
						{
							Speed *= FMath::Clamp(FMath::Sqrt(DistSq) / Slowdown, 0.f, 1.f);
						}
					}
					SteerVelocity = Dir * Speed;
					AdvanceDir = Dir;
					bAdvancing = true;
				}
				else if (Displacement.bReturningToBase)
				{
					Displacement.bReturningToBase = false;
					Displacement.BasePosition = Movement.Position;
				}
			}

			// 3. Separation. One 3x3 neighbour sweep gathers the same-team spread-apart push (with the
			//    distributed-deployment "blocked ahead" test) AND, when enabled, a weak ENEMY anti-stack
			//    push so opposing lines can interpenetrate and jumble without bodies perfectly overlapping
			//    (Phase 26). Same-team stability measures (clamp, relaxation, combat-anchor) are unchanged.
			FVector Push = FVector::ZeroVector;
			FVector EnemyPush = FVector::ZeroVector;
			bool bBlockedAhead = false;
			if (bAvoidance)
			{
				const float SepRadius = SepRadiusOf(Cfg);
				const float EnemySepRadius = EnemySepRadiusOf(Cfg);
				if (SepRadius > 0.f || EnemySepRadius > 0.f)
				{
					const FIntPoint Cell = PositionToCell(Movement.Position, CellSize);
					const float RadiusSq = SepRadius * SepRadius;
					const float EnemyRadiusSq = EnemySepRadius * EnemySepRadius;
					const float MaxPush = MaxPushOf(Cfg);
					const EAshTeamId SelfTeam = TeamList[It].TeamId;
					const float SelfDistToDestSq = (bAdvancing && bGroupObjective)
						? FVector::DistSquared2D(Movement.Position, Destination) : 0.f;
					for (int32 dx = -1; dx <= 1; ++dx)
					{
						for (int32 dy = -1; dy <= 1; ++dy)
						{
							const TArray<FNeighbour>* Bucket = SpatialGrid.Find(FIntPoint(Cell.X + dx, Cell.Y + dy));
							if (!Bucket)
							{
								continue;
							}
							for (const FNeighbour& N : *Bucket)
							{
								FVector Off = Movement.Position - N.Pos;
								Off.Z = 0.f;
								const float DistSq = Off.SizeSquared();
								if (DistSq <= KINDA_SMALL_NUMBER)
								{
									continue;
								}
								if (N.Team == SelfTeam)
								{
									if (SepRadius > 0.f && DistSq < RadiusSq)
									{
										const float Dist = FMath::Sqrt(DistSq);
										const float Contribution = FMath::Min(MaxPush, 1.f - Dist / SepRadius);
										Push += (Off / Dist) * Contribution;

										// Distributed deployment: a squad-mate ahead of us and closer to the
										// group objective means we've reached the formation edge — stop pressing in.
										if (bAdvancing && bGroupObjective
											&& FVector::DotProduct(-Off, AdvanceDir) > 0.f
											&& FVector::DistSquared2D(N.Pos, Destination) < SelfDistToDestSq)
										{
											bBlockedAhead = true;
										}
									}
								}
								else if (EnemySepRadius > 0.f && DistSq < EnemyRadiusSq)
								{
									// Weak opposing anti-stack: lets lines blend/jumble without overlapping.
									const float Dist = FMath::Sqrt(DistSq);
									EnemyPush += (Off / Dist) * (1.f - Dist / EnemySepRadius);
								}
							}
						}
					}
				}
			}

			// 4. Combine. A blocked soldier abandons its forward press so separation can seat it at
			//    spacing behind the rank ahead; otherwise it keeps its (eased) steer velocity. Enemy
			//    anti-stack is NOT anchor-resisted (even an attacker yields a little so bodies slip past),
			//    while same-team anchoring still holds the line. Combined velocity is clamped to MoveSpeed.
			Movement.Velocity = bBlockedAhead ? FVector::ZeroVector : SteerVelocity;
			if (!Push.IsNearlyZero() || !EnemyPush.IsNearlyZero())
			{
				const float Relax = FMath::Clamp(SepRelaxOf(Cfg), 0.f, 1.f);
				if (!Push.IsNearlyZero())
				{
					const float AnchorScale = (State == EAshSoldierState::Attack)
						? (1.f - FMath::Clamp(AnchorOf(Cfg), 0.f, 1.f))
						: 1.f;
					Movement.Velocity += Push * (Movement.MoveSpeed * SepStrengthOf(Cfg) * Relax * AnchorScale);
				}
				if (!EnemyPush.IsNearlyZero())
				{
					Movement.Velocity += EnemyPush * (Movement.MoveSpeed * EnemySepStrengthOf(Cfg) * Relax);
				}
				const float MaxSpeed = Movement.MoveSpeed;
				if (MaxSpeed > 0.f && Movement.Velocity.SizeSquared() > FMath::Square(MaxSpeed))
				{
					Movement.Velocity = Movement.Velocity.GetSafeNormal() * MaxSpeed;
				}
			}

			Movement.Position += Movement.Velocity * DeltaTime;

			if (bPlayerCanPush && !Displacement.bReturningToBase)
			{
				FVector FromPlayer = Movement.Position - PlayerLocation;
				FromPlayer.Z = 0.f;
				const float DistSq = FromPlayer.SizeSquared();
				if (DistSq < FMath::Square(PlayerPushRadius))
				{
					const float Dist = FMath::Sqrt(FMath::Max(DistSq, 1.f));
					const FVector RadialDir = FromPlayer / Dist;
					const FVector PlayerDir = PlayerVelocity.GetSafeNormal2D();
					const FVector PushDir = (RadialDir + PlayerDir * 0.35f).GetSafeNormal2D();
					const float PushAlpha = 1.f - (Dist / PlayerPushRadius);
					Movement.Position += PushDir * (PlayerPushSpeed * PushAlpha * DeltaTime);
					Displacement.LastPushedTime = Now;

					if (FVector::DistSquared2D(Movement.Position, Displacement.BasePosition) > FMath::Square(MaxPlayerForcedDisplacement))
					{
						Displacement.bReturningToBase = true;
					}
				}
			}

			// 5. Target-aware interpolated facing (Phase 20). While engaging/attacking, face the
			//    target so a stopped soldier still looks at its enemy (fixes "attacks facing the wrong
			//    way"); otherwise face travel. The yaw is interpolated at the unit's turn rate so the
			//    body never snaps and separation pushes don't whip the facing around.
			float DesiredYaw = Movement.FacingYaw;
			bool bHaveDesired = false;
			// Attack/Engage (closing strikers) and Surround (orbiting waiters) all face the target so they
			// menace it while strafing the ring — the body looks at the enemy even though it sidesteps (Phase 28).
			if ((State == EAshSoldierState::Attack || State == EAshSoldierState::Engage
				|| State == EAshSoldierState::Surround) && bHasTargetPos)
			{
				const FVector ToTarget = TargetPos - Movement.Position;
				if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
				{
					DesiredYaw = ToTarget.Rotation().Yaw;
					bHaveDesired = true;
				}
			}
			else
			{
				const FVector PlanarVel(Movement.Velocity.X, Movement.Velocity.Y, 0.f);
				if (PlanarVel.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					DesiredYaw = PlanarVel.Rotation().Yaw;
					bHaveDesired = true;
				}
			}

			if (bHaveDesired)
			{
				const float TurnRate = TurnRateOf(Cfg);
				Movement.FacingYaw = (TurnRate > 0.f)
					? FMath::FixedTurn(Movement.FacingYaw, DesiredYaw, TurnRate * DeltaTime)
					: DesiredYaw;
			}
		}
	});

	// Visualise the shared flow field once per frame (Phase 14 debug). Gated so it costs
	// nothing in Direct mode or when disabled.
	if (bDrawFlowFieldDebug && FlowField)
	{
		FlowField->DrawDebug(World);
	}
}
