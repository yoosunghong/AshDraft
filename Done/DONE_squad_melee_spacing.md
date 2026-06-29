---
# DONE: Squad-vs-Squad Melee Spacing (1v1 / 1v2)

## Summary

Fixes the field report that, when two squads engage (e.g. a 1v1 between fireteams during a general duel),
**all ~10 soldiers pile onto a single point**. An engaged fireteam now fans its soldiers out abreast along
the contact front on its own side instead of collapsing every soldier onto the one shared contact point.
The squads stay clustered around the contact (squad-level grouping is preserved) but individual soldiers are
spaced apart, so a 1v1 / 1v2 clash reads as two spread ranks meeting rather than one ball.

## Files Changed

- `Public/Mass/AshMassFireteamProcessor.h` — new data-driven `EngageLineSpacing` (lateral spacing between
  engaged soldiers) and `EngageOwnSideOffset` (how far the line sits back on its own side).
- `Private/Mass/AshMassFireteamProcessor.cpp` — new `EngageLineSlot()` helper; the engaged-fireteam write
  now spreads soldiers along the contact front (keyed on `FireteamFragment::SlotIndex` / `FireteamSize`)
  instead of writing the single contact point to every soldier.

## Follow-up (second field report): generals isolated, still clustered, erratic motion

A PIE pass surfaced three more problems, all fixed here:

1. **Generals duel 1-on-1 in an empty clearing.** When a Battle forms, *every* fireteam was assigned a duel
   ring slot at `DuelRingRadius` (~14 m out) and the battle assignment overrode the general's own "rally on
   me" objective — so the troops left and the generals fought alone. Fix: `UAshGeneralConfig::GuardFireteamCount`
   (default 1) keeps each general's **closest fireteam(s) as a personal guard**, excluded from the ring in
   `UAshBattleSubsystem::Replan`. With no battle assignment they follow the general's combat objective and
   engage whatever is near it (the enemy's guards / the rival general), so there is a knot of soldiers
   fighting *around* the generals.
2. **Squads still clustered.** The engage-line spread used `normalize(enemyAvg − myAvg)` as its facing, which
   **degenerates to a constant once the two squads intermix** (their averages coincide), collapsing both
   lines back onto one point. Fix: the fireteam processor now low-pass-filters each engaged fireteam's facing
   (`SmoothedEngageFacing`, time constant `EngageFacingSmoothTime`) and **never updates it from a near-zero
   delta**, so the opposing lines stay stably separated even in a fully intermixed melee.
3. **Soldiers move erratically.** Same root cause — the per-frame facing/anchor was recomputed from noisy
   moving averages, so soldiers chased a jittering formation point. The smoothed, degenerate-guarded facing
   removes that jitter; motion is calm.

Added: `UAshGeneralConfig::GuardFireteamCount`; `UAshBattleSubsystem` guard exclusion;
`UAshMassFireteamProcessor::EngageFacingSmoothTime` + `SmoothedEngageFacing` (pass 2c) feeding the engaged
line facing.

## Implementation Details

- **Root cause.** When a fireteam reached melee contact (both the Global-Combat ring-slot branch and the
  out-of-battle midpoint branch), the processor set `bUseFormationV = false` and wrote
  `DesiredWorldPosition = Anchor` — the *same* contact point — for all five soldiers. With both fireteams
  doing this, all ten soldiers funneled to one spot; same-team separation (relaxed + clamped) could not
  overcome the convergence.
- **Fix.** Those two branches already set `bHasContactAnchor = true` (and only those branches do), so it is
  used as the "in melee contact" predicate at the final write. When true, each soldier's rest position is
  `EngageLineSlot(ContactPoint, Facing, SlotIndex, FireteamSize, EngageLineSpacing, EngageOwnSideOffset)`:
  - lateral offset `= (SlotIndex - (FireteamSize-1)/2) * EngageLineSpacing` along the front (perpendicular
    to `Facing`), centring the line on the contact point;
  - `- Facing * EngageOwnSideOffset` pulls the whole line back onto the fireteam's own side, so the two
    opposing ranks don't spawn perfectly interpenetrating and meet with a small closeable gap.
- **Why this is enough.** The per-soldier melee dissolve (behavior processor `AcquireCappedNearest`) still
  hands each soldier a distinct enemy, and when a soldier has a live target the movement processor's combat
  approach drives it to striking range. The engagement line is the *rest/fallback* shape (used between
  target assignments and the moment of contact); because **both** teams now start spread, the dissolve has
  spread targets to chase and the spacing persists. Same-team separation fine-tunes the final gaps; the
  Engaged leash (`MeleeChaseRadius`, anchored to the contact) keeps the brawl a local bubble.
- **Untouched paths.** The deploy/marching branch keeps its V (`bUseFormationV = true`, no contact anchor);
  the actor-target branch (general/hero) keeps the single-point anchor (soldiers combat-approach the actor);
  the no-contact formation branch keeps the squad-objective V. Only fireteam-vs-fireteam melee changed.

## Architecture Notes

- Hierarchical AI (ARCHITECTURE.md 8): the change lives at the fireteam/formation layer; soldiers keep
  simple local rules and the squad stays the clustering unit.
- Data-driven (17 / CLAUDE.md): the two spacing constants are `config, EditDefaultsOnly` UPROPERTYs on the
  processor (persisted to `DefaultGame.ini`), consistent with the existing `Fireteam*Spacing` tunables.
- No new dependencies, no Tick, no per-soldier pathfinding; pure formation math.

## Testing / Verification

- Build: LyraEditor Win64 Development — **Succeeded** (2026-06-29, editor closed).
- PENDING USER (editor + PIE): place an ally and an enemy general with troops; when squads clash 1v1 / 1v2,
  confirm the soldiers spread into spaced opposing ranks around the contact (no single-point pile-up) while
  the fireteams stay grouped. Tune `EngageLineSpacing` (wider/narrower ranks) and `EngageOwnSideOffset` (gap
  between the lines) under `[/Script/AshDraftCoreRuntime.AshMassFireteamProcessor]`. `Ash.Battle.Debug 1`
  shows the duel ring/contact slots.

## Known Issues

- The line is centred and symmetric per fireteam; in a 1v2 the two attacking fireteams approach the shared
  enemy from their own sides, so their lines sit on different bearings (they don't perfectly overlap) but are
  not explicitly de-conflicted against each other.
- The actor-target case (soldiers swarming a general/hero) still clusters at an attack-range arc; spreading
  that would want the combat-slot system and is out of scope for this request (squad-vs-squad).

## Next Steps

- Optional: feed `EngageLineSpacing` from the unit's body size (`UAshSoldierVisualConfig` capsule radius) so
  larger units space out more.
- Optional: a slight per-soldier jitter on the line for a less regimented look, if the ranks read too tidy.
