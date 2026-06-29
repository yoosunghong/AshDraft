// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassFireteamProcessor.h"

#include "AI/AshBattleSubsystem.h"
#include "AI/AshBattleTypes.h"
#include "AI/AshEngagementMatcher.h"
#include "AI/AshFireteamSubsystem.h"
#include "AI/AshGeneralSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "Character/AshGeneralCharacter.h"
#include "Character/AshHeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	struct FAshRuntimeFireteam
	{
		int32 FireteamId = INDEX_NONE;
		int32 SquadId = INDEX_NONE;
		int32 FireteamIndexInSquad = 0;
		EAshTeamId TeamId = EAshTeamId::Neutral;
		FVector AveragePosition = FVector::ZeroVector;
		FVector LeaderPosition = FVector::ZeroVector;
		FMassEntityHandle LeaderEntity;
		int32 AliveCount = 0;
		int32 AssignedEnemyFireteamId = INDEX_NONE;
		TWeakObjectPtr<AActor> AssignedActorTarget;

		// Global Combat assignment (Phase 24). Authoritative over the proximity fallback below.
		bool bBattleAssigned = false;
		FVector BattleRingSlot = FVector::ZeroVector;

		// Stable, low-pass-filtered facing toward the enemy while engaged (Phase 27): keeps the
		// melee spread line from collapsing/jittering once the squads intermix. Set in pass 2c.
		FVector EngageFacing = FVector::ForwardVector;
	};

	FVector PlanarSafeNormal(const FVector& V, const FVector& Fallback = FVector::ForwardVector)
	{
		FVector P(V.X, V.Y, 0.f);
		return P.Normalize() ? P : Fallback;
	}

	FVector RotatePlanarOffset(const FVector& LocalOffset, const FVector& FacingDir)
	{
		const FRotator FacingRot = FacingDir.Rotation();
		return FRotationMatrix(FacingRot).TransformVector(LocalOffset);
	}

	/**
	 * Per-soldier rest position for an *engaged* fireteam: a line-abreast slot along the contact front,
	 * on the fireteam's own side of the shared contact point (Phase 27). Soldiers fan out laterally by
	 * SlotIndex (centred), spaced LineSpacing apart, and the whole line sits OwnSideOffset back toward
	 * its own side (-Facing). This replaces collapsing all soldiers onto the single contact point — the
	 * cause of a 1v1 squad clash piling 10 bodies on one spot. Facing is a planar unit vector toward the
	 * enemy; its perpendicular is the line direction. Same-team separation fine-tunes the final spacing.
	 */
	FVector EngageLineSlot(const FVector& ContactPoint, const FVector& Facing, int32 SlotIndex,
		int32 FireteamSize, float LineSpacing, float OwnSideOffset)
	{
		const int32 Size = FMath::Max(1, FireteamSize);
		const int32 Slot = FMath::Clamp(SlotIndex, 0, Size - 1);
		const FVector Right(-Facing.Y, Facing.X, 0.f);
		const float Lateral = (static_cast<float>(Slot) - 0.5f * static_cast<float>(Size - 1)) * LineSpacing;
		return ContactPoint - Facing * OwnSideOffset + Right * Lateral;
	}

	/**
	 * Out-of-battle fireteam matchmaking (the fallback used when no Global Combat plan covers a fireteam).
	 * Pairs hostile fireteams via the shared AshEngagement matcher (greedy 1:1 by proximity + capped
	 * surplus doubling + hysteresis), then any fireteam with no enemy squad in range falls back to the
	 * nearest hostile actor (rival general or the player hero). This is the *only* path that targets an
	 * actor, so generals/the hero stop being a crowd magnet.
	 */
	void MatchmakeFallback(
		TMap<int32, FAshRuntimeFireteam>& Fireteams,
		const UWorld* World,
		const UAshGeneralSubsystem* GeneralSubsystem,
		float EngageRadius,
		float ActorEngageRadius,
		TMap<int32, int32>& LastAssignment)
	{
		// Two-sided FMatchUnit list over the non-battle fireteams (side 0 = a reference team, side 1 = hostile).
		EAshTeamId RefTeam = EAshTeamId::Neutral;
		TArray<AshEngagement::FMatchUnit> Units;
		for (const TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			const FAshRuntimeFireteam& FT = Pair.Value;
			if (FT.AliveCount <= 0 || FT.bBattleAssigned)
			{
				continue;
			}
			if (RefTeam == EAshTeamId::Neutral)
			{
				RefTeam = FT.TeamId;
			}
			int32 Side;
			if (FT.TeamId == RefTeam)
			{
				Side = 0;
			}
			else if (UAshTeamStatics::AreTeamsHostile(RefTeam, FT.TeamId))
			{
				Side = 1;
			}
			else
			{
				continue;
			}
			Units.Add({ FT.FireteamId, FT.AveragePosition, Side });
		}

		TMap<int32, int32> OutAssignment;
		if (Units.Num() >= 2)
		{
			AshEngagement::FPairingParams Params;
			Params.MaxPairDist = EngageRadius;
			Params.MaxAttackers = 2;
			Params.StickinessBonus = EngageRadius * 0.1f;
			TArray<AshEngagement::FMatchPairing> Pairings;
			AshEngagement::BalancedPairing(Units, Params, LastAssignment, Pairings, OutAssignment);

			for (const TPair<int32, int32>& A : OutAssignment)
			{
				if (FAshRuntimeFireteam* FT = Fireteams.Find(A.Key))
				{
					FT->AssignedEnemyFireteamId = A.Value;
				}
			}
		}
		LastAssignment = MoveTemp(OutAssignment);

		// Any fireteam still without an enemy squad: target the nearest hostile actor (general or hero).
		const float ActorRadiusSq = FMath::Square(ActorEngageRadius);
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
		const AAshHeroCharacter* Hero = Cast<AAshHeroCharacter>(PlayerPawn);
		const bool bHeroTargetable = PlayerPawn && (!Hero || !Hero->IsDead());

		for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			FAshRuntimeFireteam& FT = Pair.Value;
			if (FT.AliveCount <= 0 || FT.bBattleAssigned || FT.AssignedEnemyFireteamId != INDEX_NONE)
			{
				continue;
			}

			float BestDistSq = ActorRadiusSq;
			if (GeneralSubsystem)
			{
				for (int32 GeneralId : GeneralSubsystem->GetAllGeneralIds())
				{
					const FAshGeneralState GeneralState = GeneralSubsystem->GetGeneralState(GeneralId);
					AAshGeneralCharacter* General = GeneralState.General.Get();
					if (!General || General->IsDead() || !UAshTeamStatics::AreTeamsHostile(FT.TeamId, GeneralState.TeamId))
					{
						continue;
					}
					const float DistSq = FVector::DistSquared2D(FT.AveragePosition, General->GetActorLocation());
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						FT.AssignedActorTarget = General;
					}
				}
			}

			if (bHeroTargetable && UAshTeamStatics::AreTeamsHostile(FT.TeamId, UAshTeamStatics::GetActorTeam(PlayerPawn)))
			{
				const float DistSq = FVector::DistSquared2D(FT.AveragePosition, PlayerPawn->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					FT.AssignedActorTarget = PlayerPawn;
				}
			}
		}
	}
}

UAshMassFireteamProcessor::UAshMassFireteamProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassSoldierBehaviorProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassMovementProcessor"));
	bRequiresGameThreadExecution = true;
}

void UAshMassFireteamProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshSquadFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshFireteamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshFormationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatTargetFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassFireteamProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	UAshFireteamSubsystem* FireteamSubsystem = World ? World->GetSubsystem<UAshFireteamSubsystem>() : nullptr;
	const UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
	const UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr;
	const UAshBattleSubsystem* BattleSubsystem = World ? World->GetSubsystem<UAshBattleSubsystem>() : nullptr;

	// --- Pass 1: decompose entities into runtime fireteams (average position, leader, alive count). ---
	TMap<int32, FAshRuntimeFireteam> Fireteams;
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshFireteamFragment> FireteamList = ChunkContext.GetFragmentView<FAshFireteamFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth <= 0.f || FireteamList[It].FireteamId == INDEX_NONE)
			{
				continue;
			}

			const FAshFireteamFragment& FireteamFrag = FireteamList[It];
			FAshRuntimeFireteam& Runtime = Fireteams.FindOrAdd(FireteamFrag.FireteamId);
			Runtime.FireteamId = FireteamFrag.FireteamId;
			Runtime.SquadId = SquadList[It].SquadId;
			Runtime.FireteamIndexInSquad = FireteamFrag.FireteamIndexInSquad;
			Runtime.TeamId = TeamList[It].TeamId;
			Runtime.AveragePosition += MovementList[It].Position;
			Runtime.AliveCount++;

			if (FireteamFrag.Role == EAshFireteamRole::Leader)
			{
				Runtime.LeaderPosition = MovementList[It].Position;
				Runtime.LeaderEntity = ChunkContext.GetEntity(It);
			}
		}
	});

	for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
	{
		FAshRuntimeFireteam& Fireteam = Pair.Value;
		if (Fireteam.AliveCount > 0)
		{
			Fireteam.AveragePosition /= static_cast<float>(Fireteam.AliveCount);
			if (!Fireteam.LeaderEntity.IsSet())
			{
				Fireteam.LeaderPosition = Fireteam.AveragePosition;
			}
		}
	}

	// --- Pass 2a: stamp the committed Global Combat plan (Phase 24). Battle-assigned fireteams are
	// authoritative; the proximity fallback only fills the gaps for fireteams not part of a battle. ---
	if (BattleSubsystem && BattleSubsystem->IsInCombat())
	{
		for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			FAshEngagementAssignment Assignment;
			if (Pair.Value.AliveCount > 0 && BattleSubsystem->GetAssignment(Pair.Key, Assignment) && Assignment.bValid)
			{
				Pair.Value.bBattleAssigned = true;
				Pair.Value.AssignedEnemyFireteamId = Assignment.EnemyFireteamId;
				Pair.Value.BattleRingSlot = Assignment.RingSlot;
			}
		}
	}

	// --- Pass 2b: out-of-battle proximity matchmaking via the shared matcher (+ actor fallback). ---
	MatchmakeFallback(Fireteams, World, GeneralSubsystem, FireteamEngageRadius, GeneralActorEngageRadius,
		LastFallbackAssignment);

	// --- Pass 2c: stable per-fireteam engage facing (Phase 27). The melee spread line faces the enemy;
	// the raw avg-to-avg facing degenerates and jitters once the squads intermix, so it is low-pass
	// filtered here and never updated from a near-zero (intermixed) delta. Keeps opposing lines stably
	// separated and the soldiers' motion calm. ---
	{
		const float Dt = Context.GetDeltaTimeSeconds();
		const float SmoothTime = FMath::Max(0.f, EngageFacingSmoothTime);
		const float Alpha = (SmoothTime > KINDA_SMALL_NUMBER) ? FMath::Clamp(Dt / SmoothTime, 0.f, 1.f) : 1.f;
		TSet<int32> SeenFireteams;
		for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			FAshRuntimeFireteam& FT = Pair.Value;
			SeenFireteams.Add(FT.FireteamId);

			// Point this fireteam faces while engaged: the enemy squad if matched, else its battle slot.
			const FAshRuntimeFireteam* Enemy = Fireteams.Find(FT.AssignedEnemyFireteamId);
			FVector Target = FVector::ZeroVector;
			bool bHasTarget = false;
			if (Enemy) { Target = Enemy->AveragePosition; bHasTarget = true; }
			else if (FT.bBattleAssigned) { Target = FT.BattleRingSlot; bHasTarget = true; }

			FVector& Smoothed = SmoothedEngageFacing.FindOrAdd(FT.FireteamId, FVector::ZeroVector);
			if (bHasTarget)
			{
				FVector Delta = Target - FT.AveragePosition;
				Delta.Z = 0.f;
				if (Delta.SizeSquared() > FMath::Square(50.f)) // ignore degenerate (intermixed) deltas
				{
					const FVector Raw = Delta.GetUnsafeNormal();
					Smoothed = Smoothed.IsNearlyZero() ? Raw : FMath::Lerp(Smoothed, Raw, Alpha).GetSafeNormal();
				}
			}
			FT.EngageFacing = Smoothed.IsNearlyZero() ? FVector::ForwardVector : Smoothed;
		}
		// Prune filter state for fireteams that no longer exist (bounded growth across battles).
		for (auto It = SmoothedEngageFacing.CreateIterator(); It; ++It)
		{
			if (!SeenFireteams.Contains(It.Key())) { It.RemoveCurrent(); }
		}
	}

	// --- Pass 3: publish the decomposition for the Battle director / debug / UI. ---
	if (FireteamSubsystem)
	{
		FireteamSubsystem->BeginFireteamUpdate();
		for (const TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			const FAshRuntimeFireteam& Fireteam = Pair.Value;
			FAshFireteamState State;
			State.FireteamId = Fireteam.FireteamId;
			State.SquadId = Fireteam.SquadId;
			State.FireteamIndexInSquad = Fireteam.FireteamIndexInSquad;
			State.TeamId = Fireteam.TeamId;
			State.AveragePosition = Fireteam.AveragePosition;
			State.LeaderPosition = Fireteam.LeaderPosition;
			State.AliveCount = Fireteam.AliveCount;
			State.AssignedEnemyFireteamId = Fireteam.AssignedEnemyFireteamId;
			FireteamSubsystem->UpsertFireteam(State);
		}
	}

	// --- Pass 4: write each soldier's formation destination + fireteam state. On contact the fireteam
	// hands soldier-vs-soldier targeting to the melee dissolve (the behavior processor): it sets the
	// contact anchor + Engaged state but no per-slot target, so soldiers pair off individually. Only
	// actor targets (general/hero) are set here, since soldiers can't sense actors via the Mass grid. ---
	const int32 PerRow = FMath::Max(1, FireteamsPerRow);
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshFireteamFragment> FireteamList = ChunkContext.GetFragmentView<FAshFireteamFragment>();
		const TArrayView<FAshFormationFragment> FormationList = ChunkContext.GetMutableFragmentView<FAshFormationFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetMutableFragmentView<FAshCombatTargetFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshFormationFragment& Formation = FormationList[It];
			FAshCombatFragment& Combat = CombatList[It];
			FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];

			// Clears only the higher-level (actor) assignment. The per-soldier melee target (Combat.Target)
			// is OWNED by the behavior processor's dissolve — the fireteam must NOT wipe it each frame, or
			// the soldier loses its opponent every tick. Only suppress it where the soldier must not fight.
			auto SetCombatTargetNone = [&]()
			{
				CombatTarget.TargetType = EAshCombatTargetType::None;
				CombatTarget.MassTarget.Reset();
				CombatTarget.ActorTarget = nullptr;
			};
			auto SuppressMeleeTarget = [&]()
			{
				Combat.Target.Reset();
				SetCombatTargetNone();
			};

			if (HealthList[It].CurrentHealth <= 0.f)
			{
				Formation.bHasDesiredPosition = false;
				Formation.bHasContactAnchor = false;
				SuppressMeleeTarget();
				continue;
			}

			const FAshFireteamFragment& FireteamFrag = FireteamList[It];
			const FAshRuntimeFireteam* Fireteam = Fireteams.Find(FireteamFrag.FireteamId);
			if (!Fireteam)
			{
				continue;
			}

			FVector Anchor = Fireteam->LeaderPosition;
			FVector Facing = FVector::ForwardVector;
			EAshFireteamState FireteamState = EAshFireteamState::Standby;
			bool bUseFormationV = true;     // apply the per-soldier V offset (off once dissolved into melee)
			FVector ContactAnchor = FVector::ZeroVector;
			bool bHasContactAnchor = false;

			const FAshRuntimeFireteam* EnemyFireteam = Fireteams.Find(Fireteam->AssignedEnemyFireteamId);
			const AActor* ActorTarget = Fireteam->AssignedActorTarget.Get();

			if (Fireteam->bBattleAssigned)
			{
				// Global Combat: march to the committed ring slot (Deploying suppresses local sensing so the
				// squad ignores everything en route), then hand off to the melee dissolve at the slot.
				const FVector Slot = Fireteam->BattleRingSlot;
				const float DistToSlotSq = FVector::DistSquared2D(Fireteam->AveragePosition, Slot);
				const float DistToEnemySq = EnemyFireteam
					? static_cast<float>(FVector::DistSquared2D(Fireteam->AveragePosition, EnemyFireteam->AveragePosition))
					: TNumericLimits<float>::Max();
				const bool bArrived = DistToSlotSq <= FMath::Square(BattleSlotArrivalDistance)
					|| DistToEnemySq <= FMath::Square(BattleEngageProximity);

				if (bArrived)
				{
					// Engaged: contact is the shared slot; soldiers dissolve into individual duels there.
					Anchor = Slot;
					Facing = Fireteam->EngageFacing; // stable smoothed facing (Phase 27)
					FireteamState = EAshFireteamState::Engaged;
					bUseFormationV = false;
					ContactAnchor = Slot;
					bHasContactAnchor = true;
					SetCombatTargetNone(); // melee dissolve picks the individual target at the slot
				}
				else
				{
					Anchor = Slot;
					Facing = PlanarSafeNormal(Slot - Fireteam->AveragePosition, FVector::ForwardVector);
					FireteamState = EAshFireteamState::Deploying;
					SuppressMeleeTarget(); // ignore everything en route
				}
			}
			else if (EnemyFireteam)
			{
				// Out-of-battle contact: clash at the midpoint and dissolve into individual melee there.
				const FVector Midpoint = (Fireteam->AveragePosition + EnemyFireteam->AveragePosition) * 0.5f;
				Anchor = Midpoint;
				Facing = Fireteam->EngageFacing; // stable smoothed facing (Phase 27)
				FireteamState = EAshFireteamState::Engaged;
				bUseFormationV = false;
				ContactAnchor = Midpoint;
				bHasContactAnchor = true;
				SetCombatTargetNone(); // melee dissolve picks individual targets at the midpoint
			}
			else if (ActorTarget)
			{
				// Actor target (rival general / hero): soldiers approach + strike it directly (combat layer).
				const FVector TargetPos = ActorTarget->GetActorLocation();
				Anchor = TargetPos;
				Facing = PlanarSafeNormal(TargetPos - Fireteam->AveragePosition, FVector::ForwardVector);
				FireteamState = EAshFireteamState::Engaged;
				bUseFormationV = false;
				Combat.Target.Reset();
				CombatTarget.TargetType = EAshCombatTargetType::Actor;
				CombatTarget.MassTarget.Reset();
				CombatTarget.ActorTarget = const_cast<AActor*>(ActorTarget);
			}
			else
			{
				// No contact: arrange the fireteam around the squad objective in formation.
				FVector SquadObjective;
				float SquadFormationRadius = 0.f;
				FVector SquadFacing = FVector::ForwardVector;
				if (SquadSubsystem && SquadSubsystem->GetSquadObjective(SquadList[It].SquadId, SquadObjective, SquadFormationRadius, SquadFacing))
				{
					// Stable, general-published orientation (not derived from our own moving position) so the
					// formation settles instead of orbiting the objective forever (Phase 27).
					Facing = PlanarSafeNormal(SquadFacing, FVector::ForwardVector);
					const int32 Column = (Fireteam->FireteamIndexInSquad % PerRow) - (PerRow / 2);
					const int32 Row = Fireteam->FireteamIndexInSquad / PerRow;
					const FVector TeamOffset(-Row * FireteamRowSpacing, Column * FireteamSpacing, 0.f);
					Anchor = SquadObjective + RotatePlanarOffset(TeamOffset, Facing);
					FireteamState = FVector::DistSquared2D(Fireteam->AveragePosition, Anchor) > FMath::Square(FireteamStandbyDistance)
						? EAshFireteamState::Moving
						: EAshFireteamState::Standby;
				}
				// Leave Combat.Target to the behavior processor (a marching soldier may still skirmish a
				// stray enemy it senses); only clear the higher-level actor assignment.
				SetCombatTargetNone();
			}

			Formation.FireteamState = FireteamState;
			if (bHasContactAnchor)
			{
				// Engaged in melee at a shared contact point (fireteam-vs-fireteam): fan the soldiers out
				// abreast on our own side of the contact instead of stacking all of them on the single
				// point - the fix for a 1v1 squad clash piling 10 bodies onto one spot (Phase 27). The
				// fireteam stays clustered around the anchor; the melee dissolve then hands each soldier a
				// distinct enemy, and same-team separation fine-tunes the spacing.
				Formation.DesiredWorldPosition = EngageLineSlot(Anchor, Facing, FireteamFrag.SlotIndex,
					FireteamFrag.FireteamSize, EngageLineSpacing, EngageOwnSideOffset);
			}
			else
			{
				Formation.DesiredWorldPosition = bUseFormationV
					? Anchor + RotatePlanarOffset(Formation.LocalOffset, Facing)
					: Anchor;
			}
			Formation.bHasDesiredPosition = true;
			Formation.ContactAnchor = ContactAnchor;
			Formation.bHasContactAnchor = bHasContactAnchor;
		}
	});
}
