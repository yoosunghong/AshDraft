# DONE: Victory, Defeat, and Match Flow (Phase 16)

## Summary

Implemented the PoC match loop (ARCHITECTURE.md 15/16): a world-scoped state machine that
starts on world BeginPlay, watches the three Phase 16 end conditions, and broadcasts a terminal
result that UI/telemetry observe. The loop transitions `NotStarted → InProgress → Victory/Defeat`:

- **Victory** — the player side (Player/Ally) owns *every* registered base.
- **Defeat** — the playable hero dies.
- **Defeat** — a player-side *main* base is captured by the enemy.

It is fully event-driven (no Actor Tick, no polling): it binds once to
`UAshBaseSubsystem::OnAnyBaseOwnershipChanged` and to the hero's new `OnHeroDied` delegate. A
code-only on-screen "VICTORY"/"DEFEAT" banner makes the loop verifiable in PIE before any UMG
widget exists; the real result widget binds `OnMatchEnded`.

## Files Changed

Created:
- `Source/AshDraftCoreRuntime/Public/Match/AshMatchTypes.h` — `EAshMatchState`,
  `EAshMatchResult`, `EAshMatchEndReason`.
- `Source/AshDraftCoreRuntime/Public/Match/AshMatchSubsystem.h`
- `Source/AshDraftCoreRuntime/Private/Match/AshMatchSubsystem.cpp` —
  `UAshMatchSubsystem : UWorldSubsystem` (state machine, condition evaluation, delegates, hero-bind
  retry, on-screen result fallback).

Modified:
- `Source/AshDraftCoreRuntime/Public/Character/AshHeroCharacter.h`
- `Source/AshDraftCoreRuntime/Private/Character/AshHeroCharacter.cpp` — added `FAshOnHeroDied`
  delegate, `IsDead()`, health-change binding, and `HandleDeath()` (mirrors
  `AAshEnemyGeneralCharacter`). The hero previously had **no** death handling at all.
- `Source/AshDraftCoreRuntime/Public/Base/AshBaseActor.h` — added `bIsMainBase`
  (EditAnywhere) + `IsMainBase()`.
- `Docs/PLAN.md`, `Docs/Plan/Phase16.md` — checklist + notes.

## Implementation Details

- **Match state machine** lives in `UAshMatchSubsystem`. `StartMatch()` runs in
  `OnWorldBeginPlay` (after all actor BeginPlay, so bases are registered), sets `InProgress`,
  broadcasts `OnMatchStarted`, and evaluates once. `EndMatch(Result, Reason)` is idempotent
  (terminal states are sticky), logs `LogAshMatch`, broadcasts `OnMatchEnded`, and prints the
  banner.
- **Base conditions** (`EvaluateBaseConditions`) run on every base flip: scan all bases; if a
  main base is Enemy-owned → defeat (`MainBaseLost`); if every base is player-side → victory
  (`AllBasesCaptured`). Guarded against the zero-base case.
- **Player death**: the hero now drives death through the GAS Health attribute (event-driven,
  ARCHITECTURE.md 15) and broadcasts `OnHeroDied`; the subsystem ends the match as `PlayerDeath`.
  Because the experience pipeline may spawn the hero *after* world BeginPlay, `TryBindToHero`
  retries on a short timer until the hero exists.
- **UI seam** (ARCHITECTURE.md 16): the subsystem exposes `OnMatchStarted` / `OnMatchEnded` /
  `OnMatchStateChanged` as `BlueprintAssignable`. UI reads results through them; the subsystem
  never owns UI. The on-screen banner is only a verification fallback.

## Architecture Notes

- Event-driven, no Tick (ARCHITECTURE.md 18.3); reacts to base/hero events rather than polling.
- Authority-friendly (5.5/15): match end is an authoritative, event-driven decision, not a
  cosmetic local effect.
- Low coupling (18.4): soft subsystem lookups + weak hero pointer; no hard UI/AI dependency. The
  "main base" designation is data on the placed base instance, not hardcoded.

## Testing / Verification

- **Build verified**: `LyraEditor Win64 Development` — **Succeeded** (2026-06-22), incremental
  compile + link of `UnrealEditor-AshDraftCoreRuntime.dll`, no errors.
- PIE verification is PENDING USER and documented in
  `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §16, including a diagnostic layer-by-layer
  checklist (subsystem init → match start log → each end condition → banner/delegate) so a failure
  localizes to a specific layer.

## Known Issues

- Result UI is a debug banner only; the real UMG widget is editor work (bind `OnMatchEnded`).
- Single-player PoC: hero death is terminal (no respawn flow yet); `HandleDeath` is the seam for it.
- Victory requires ≥1 base; a degenerate map with only player-side bases wins instantly (correct
  by the rule, but worth knowing).
- If telemetry's subsystem binds *after* an instant match-end (degenerate map), it could miss that
  one result; irrelevant on the real Phase 15 map where the match ends much later.

## Next Steps

- Phase 15 map: flag the ally + enemy home bases `bIsMainBase=true`; author a real result widget.
- Phase 17 already consumes `OnMatchEnded` for the match-result log.
