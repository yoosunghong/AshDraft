# DONE: Base / Stronghold System (Phase 7)

## Summary

Implemented the first capturable battlefield base (ARCHITECTURE.md 11): `AAshBaseActor`
with a durability-based capture model. A base owns a team, a durability pool, and a
spherical capture volume. While a hostile team holds the volume uncontested its
durability drains; at zero, ownership flips to that team and durability resets. With
both sides present the base is contested and frozen; with no attackers it recovers
after a delay. Ownership/durability/contested changes are event-driven (delegates +
Blueprint hooks), and a world subsystem re-broadcasts ownership changes so AI/UI react
without coupling to individual bases. Tuning is data-driven; there is no Actor Tick.

## Files Changed

C++ (`Source/AshDraftCoreRuntime/`):
- `Public/Base/AshBaseActor.h` + `Private/Base/AshBaseActor.cpp` (new)
- `Public/Base/AshBaseConfig.h` (new — data-driven tuning data asset)
- `Public/Base/AshBaseSubsystem.h` + `Private/Base/AshBaseSubsystem.cpp` (new — registry + AI notification hub)
- `Public/Teams/AshTeamAgentInterface.h` (new — shared team-identity contract)
- `Public/Teams/AshTeamStatics.h` + `Private/Teams/AshTeamStatics.cpp` (new — GetActorTeam / AreTeamsHostile)
- Implemented `IAshTeamAgentInterface` on `AAshHeroCharacter`, `AAshEnemyGeneralCharacter`,
  `AAshGASTargetDummy` (each returns its existing `TeamId`).

No `Build.cs` change required (SphereComponent / WorldSubsystem live in `Engine`).

## Implementation Details

- **Capture model (11.2), presence-driven for the PoC:** soldiers do not exist yet
  (Phase 8), so capture is driven by team-tagged *pawns* inside the capture sphere.
  `RecountOccupants` polls `USphereComponent::GetOverlappingActors(APawn)` and tallies
  Player/Ally/Enemy via `UAshTeamStatics::GetActorTeam`. Player+Ally are one side, Enemy
  the other. Relative to the current owner it derives DefenderCount, AttackerCount, and
  `CapturingTeam` (the specific team that would take ownership).
- **State loop is timer-driven, not Tick (18.3):** `UpdateCaptureState` runs on a
  repeating timer at `Config.UpdateInterval`. Uncontested attackers → drain at
  `CaptureRatePerSecond`; no attackers → recover at `RecoveryRatePerSecond` after
  `RecoveryDelay` (owned bases only; Neutral does not self-recover); both sides present →
  contested freeze.
- **Ownership flip:** `ChangeDurability` clamps, fires `OnDurabilityChanged`
  (+ `K2_OnDurabilityChanged`), and when durability hits zero calls `SetOwningTeam`,
  which resets durability to full for the new owner, broadcasts `OnOwnershipChanged`
  (+ `K2_OnOwnershipChanged`), and routes through the subsystem.
- **Forward hooks for Phase 8:** `NotifyDefenderDied(Team)` (defender death drains
  `DurabilityPerDefenderDeath`, re-counting first so a death mid-assault can flip the
  base) and `ApplyCaptureDamage(FromTeam, Amount)` let soldiers/the general drive
  durability directly later without changing this class.
- **Data-driven (17):** `UAshBaseConfig` supplies max durability, capture radius,
  drain/recovery rates, recovery/respawn delays, update interval. Inline `Fallback*`
  values keep a base functional with no config assigned.
- **Team interface (18.4):** `IAshTeamAgentInterface` + `UAshTeamStatics` centralize
  "what team is this actor?" and "are these teams hostile?", replacing per-class casts.

## Architecture Notes

- Event-driven and decoupled (16 / 18.4): UI binds to the base's delegates / BP hooks
  and never mutates ownership; the commander AI subscribes once to
  `UAshBaseSubsystem::OnAnyBaseOwnershipChanged` rather than to each base.
- Authority-friendly (15): ownership is real gameplay state with explicit transition
  events, not a cosmetic local effect.
- No hardcoded gameplay numbers; no Actor Tick.

## Testing / Verification

Testable now without soldiers: place an Enemy-owned `AAshBaseActor` (or a Blueprint
child) in the map, enable `bShowDebug`, and walk the Player hero into the capture
sphere. Expected: AttackerCount rises, durability drains while uncontested, and at zero
the base flips to Player (on-screen debug + `LogAshBase` line). Bringing the enemy
general into the sphere should flip it to CONTESTED and freeze durability; leaving it
empty should recover an owned base after `RecoveryDelay`.

**Pending a user C++ build + PIE run** to confirm on screen — this environment cannot
run UBT/PIE.

## Known Issues

- Capture is presence-driven (any team pawn), not yet defender-death-driven; the
  `NotifyDefenderDied` / `ApplyCaptureDamage` hooks exist but are unused until Phase 8
  soldiers call them.
- `DefenderRespawnDelay` is a config value with no spawn behavior yet (Phase 8).
- No spawn points / base mesh on the C++ actor (functional sphere only); a Blueprint
  child adds visuals and a capture-progress widget bound to the delegates.
- Occupancy is polled each update interval (cheap for a few bases); switching to
  begin/end-overlap events is a later optimization if base counts grow.

## Next Steps

- Phase 8: Basic Soldier Prototype — Actor soldiers that seek combat slots on the
  hero/general and drive base durability via `NotifyDefenderDied` / `ApplyCaptureDamage`.
