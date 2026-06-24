# DONE: Editor-Configurable Victory / Defeat Conditions

## Summary

Win/lose conditions are now chosen in the editor per level instead of being hardcoded in
`UAshMatchSubsystem`. A `UAshMatchRulesConfig` data asset toggles each victory condition (all bases
captured, enemy generals eliminated) and each defeat condition (hero death, main base lost), plus an
optional time limit with a designer-chosen outcome (survive-to-win or time-out-to-lose). An
`AAshMatchRulesActor` placed in the level carries the asset; the match subsystem discovers it and
gates every end check on its flags. Conditions stay event-driven and GAS-backed (hero / general
death flow from the GAS attribute set).

## Files Changed

- `Public/Match/AshMatchTypes.h` — new end reasons `EnemyGeneralsEliminated`, `TimeLimitReached`;
  new `EAshTimeLimitOutcome` enum.
- `Public/Match/AshMatchRulesConfig.h` — new data asset of condition toggles + time limit.
- `Public/Match/AshMatchRulesActor.h` / `Private/Match/AshMatchRulesActor.cpp` — placeable holder for
  the rules asset.
- `Public/Match/AshMatchSubsystem.h` / `Private/Match/AshMatchSubsystem.cpp` — resolves the level's
  rules, gates all conditions on them, binds enemy generals (`OnGeneralDied`), runs the time-limit
  timer, and exposes the two new end reasons.

## Implementation Details

- **Discovery.** On `OnWorldBeginPlay`, `ResolveRules()` finds the first `AAshMatchRulesActor` via
  `TActorIterator` and caches its `UAshMatchRulesConfig`. With no actor/asset present, the rule
  accessors return the Phase 16 defaults, so existing levels behave exactly as before.
- **Gating.** `EvaluateBaseConditions()` checks the main-base-lost / all-bases-captured flags (and
  early-outs if neither is active); `HandleHeroDied` checks the hero-death flag.
- **Enemy generals (GAS-driven).** `BindToEnemyGenerals()` subscribes to each general's
  `OnGeneralDied` (raised from the GAS health attribute hitting zero) and counts the living ones;
  `HandleGeneralDied` decrements and, if the rule is on and the count hits zero, declares victory.
  The condition self-disables when the level has no generals.
- **Time limit.** `StartMatch()` arms a single world timer when the outcome is non-None and the
  duration is positive; `HandleTimeLimitReached()` ends the match with the configured result.
- All bindings are cleaned up in `Deinitialize()` (base delegate, hero, generals, both timers).

## Architecture Notes

- Data-driven tuning via a Data Asset, mirroring the project's `UAshMassSoldierConfig` +
  fallback-defaults pattern (CLAUDE.md "use Data Assets for tunable values").
- Event-driven, not polled (ARCHITECTURE.md 18.3): base flips, hero/general death delegates, one
  timer. The subsystem owns no UI/AI (ARCHITECTURE.md 16 / 18.4); the on-screen banner fallback is
  unchanged.
- Authoritative match-end decision preserved (ARCHITECTURE.md 5.5/15), ready for future server auth.

## Testing / Verification

- **Not yet compiled here** — built via LyraEditor (Win64 Development) on the user's machine;
  source-only, new files auto-compiled by the module.
- PENDING USER (editor + PIE):
  1. Create a `DA_MatchRules` (`UAshMatchRulesConfig`) and select conditions; drop an
     `AshMatchRulesActor` in the level and point it at the asset.
  2. PIE-verify each selected condition: capture all bases (victory), kill the enemy general(s)
     (victory), hero death (defeat), enemy captures a main base (defeat), and the time limit
     expiring (victory or defeat per the chosen outcome).
  3. Confirm a level with no rules actor still wins on all-bases-captured and loses on hero
     death / main base lost.

## Known Issues

- `BindToEnemyGenerals()` runs once at `OnWorldBeginPlay`; generals spawned later by the experience
  pipeline (rather than placed in the level) aren't counted. A retry/registration hook (like the
  hero-bind timer) is a clean follow-up if generals become experience-spawned.
- Only one `AAshMatchRulesActor` per level is honored (first found wins).
- No result UMG widget yet (still the Phase 16 on-screen banner fallback).

## Next Steps

- Add a real result widget bound to `OnMatchEnded` that reads the new end reasons; if needed, give
  generals a registration path so experience-spawned generals are counted.
