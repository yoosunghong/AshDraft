# DONE: UI (Widget Blueprints) — Player HUD + Unit Health Bars

## Summary

Implemented the game's UI as Widget Blueprints driven by C++ base classes (Phase 30, ARCHITECTURE.md 16).
A common abstract base (`UAshUserWidget`) underpins three concrete widget classes the designer derives WBPs
from and binds elements to in the Widget Blueprint editor:

1. **Player HUD** (`UAshHUDWidget`): health bar with a mana bar below (bottom-centre), kill count
   (bottom-right), and a recently-struck-enemy panel — name, affiliation, health (top-left). The health
   and mana bars **animate down smoothly** (per-frame interpolation), with an optional trailing "chip" bar
   showing the amount just lost.
2. **Unit health UI** (`UAshUnitHealthBarWidget`): an over-head bar for soldiers/generals — **cyan** for
   the player's side, **red** for the enemy — shown together with the mesh while the unit is rendered
   (it rides the existing representation LOD: a proxy only exists when promoted near the player).

All widgets are observation-only: they read gameplay state through GAS attribute-change delegates and a new
combat-feed subsystem, and never mutate gameplay (ARCHITECTURE.md 16 / 18.4).

## Files Changed

New:
- `Public/UI/AshUITypes.h` + `Private/UI/AshUITypes.cpp` — `FAshStruckUnitInfo` (struck-unit view struct)
  and `UAshUIStatics` (`GetTeamDisplayText`, `GetTeamHealthColor` — cyan/red/grey).
- `Public/UI/AshCombatFeedSubsystem.h` + `.cpp` — world subsystem tracking the player's kill count and
  last-struck unit; `OnPlayerKillCountChanged` / `OnUnitStruck` delegates; `ReportPlayerStrike` (records
  only the local player's strikes).
- `Public/UI/AshUserWidget.h` + `.cpp` — abstract project base (`GetLocalHero`).
- `Public/UI/AshHUDWidget.h` + `.cpp` — the player HUD.
- `Public/UI/AshUnitHealthBarWidget.h` + `.cpp` — the over-head unit bar.
- `Public/UI/AshTargetInfoWidget.h` + `.cpp` — the recently-struck-enemy panel.
- `Docs/Guides/Phase30_UI_Widget_Setup_Guide.md` — editor authoring guide.

Modified:
- `AshDraftCoreRuntime.Build.cs` — added `UMG`.
- `Public/Teams/AshTeamAgentInterface.h` — added `GetAshDisplayName()` (default empty).
- `Public/Character/AshHeroCharacter.h`, `Public/Character/AshGeneralCharacter.h` — `DisplayName` +
  `GetAshDisplayName` override.
- `Public/Mass/AshSoldierVisualConfig.h` — `DisplayName` (unit-type name).
- `Public/Mass/AshSoldierProxyActor.h` + `Private/Mass/AshSoldierProxyActor.cpp` — added the screen-space
  `HealthBarWidget` `UWidgetComponent`, pushed team/health to it in `SyncFromEntity`, and reported the
  player's strikes from `ReceiveMeleeHit`.
- `Private/AbilitySystem/AshAttributeSet.cpp` — reported the player's strikes on ASC-owning victims
  (generals) from the existing damage choke point.

## Implementation Details

- **Mana = Stamina.** The attribute set has Health/MaxHealth and Stamina/MaxStamina (ARCHITECTURE.md 5.2);
  there is no separate Mana attribute, so the HUD's "mana" bar reflects the Stamina pool. Documented in the
  HUD header and the guide.
- **Smooth bars.** The HUD stores per-bar *target* (live) and *displayed* fractions; `NativeTick`
  `FInterpTo`s the front bar quickly and the optional trail bar slowly toward the target, then `SetPercent`.
  This is view interpolation in widget tick, not gameplay Actor Tick (ARCHITECTURE.md 18.3). The HUD seeds
  the displayed values to the targets on bind so bars start filled.
- **Binding model.** Every bound element is `meta=(BindWidgetOptional)`, so the WBP author names inner
  widgets to bind them and may omit/restyle any. The HUD binds all four GAS attributes to one handler that
  recomputes the health/mana target fractions; it retries the hero bind from tick until the pawn is possessed.
- **Combat feed.** `UAshCombatFeedSubsystem::ReportPlayerStrike` is called from the two places that already
  know instigator + victim + new health: the soldier proxy's actor-space melee hit (Mass soldiers) and the
  GAS attribute-set damage path (generals). It filters to the local player's pawn, updates the last-struck
  snapshot, and bumps the kill count when the blow is lethal — so the HUD shows the player's own combat.
- **Over-head bars + LOD.** The bar is a screen-space `UWidgetComponent` on the proxy. Proxies exist only
  while their entity is promoted (LOD ≤ `PromoteAtOrBelowLOD`), so bars appear with the rendered mesh and
  far units pay nothing. The widget *class* is assigned on `B_Soldier_Proxy` (data-driven; not authored in
  C++). `SetTeam` colours it cyan/red; `SetHealth` fills it each `SyncFromEntity`.

## Architecture Notes

- ARCHITECTURE.md 16 (UI): UI observes state through subsystems/components/delegates and owns no gameplay
  logic; dependency is one-way (gameplay → feed → UI), never UI → gameplay (18.4).
- ARCHITECTURE.md 17 (data assets): unit display names live on data assets / character Blueprints; HUD
  interp speeds and the kill-count format are `EditAnywhere` on the HUD widget.
- Lyra-style: features owned by the AshDraftCore plugin; widgets are Blueprint-authored over C++ bases.

## Testing / Verification

**WBP editor authoring completed 2026-06-30 via VibeUE MCP.** Three Widget Blueprints authored and saved:

| Asset | Path | Parent class | Named children |
|---|---|---|---|
| `WBP_AshUnitHealthBar` | `/AshDraftCore/Game/UI/Unit/` | `AshUnitHealthBarWidget` | `HealthBar` (ProgressBar, fill-anchored, transparent background) |
| `WBP_AshTargetInfo` | `/AshDraftCore/Game/UI/` | `AshTargetInfoWidget` | VerticalBox → `UnitNameText`, `AffiliationText`, `HealthBar` |
| `WBP_AshHUD` | `/AshDraftCore/Game/UI/HUD/` | `AshHUDWidget` | `HealthBarTrail` (z=0), `HealthBar` (z=1), `ManaBarTrail` (z=2), `ManaBar` (z=3), `KillCountText` (z=4), `RecentTargetPanel`/WBP_AshTargetInfo (z=5); trail bars tinted dark-red / dark-blue |

`BP_SoldierProxy.HealthBarWidget.widget_class` assigned to `WBP_AshUnitHealthBar` via Python CDO edit.

**Remaining user PIE tasks** (guide §5):
- Add `WBP_AshHUD` to the viewport — Create Widget (class `WBP_AshHUD`, owning player = 0) → Add to Viewport in the hero Blueprint or PlayerController BeginPlay.
- Set `DisplayName` on `DA_*_Visual` assets, the general Blueprint, and the hero Blueprint.
- PIE-verify: HUD bars glide on damage, struck-enemy panel populates, kill count increments, over-head bars show cyan/red with the rendered mesh, AI-vs-AI damage does not affect the player's HUD.

## Known Issues

- The recently-struck panel has no built-in auto-hide timer; it stays on the last struck unit. Add a fade
  via the `OnUnitStruck` Blueprint hook if a timeout is wanted.
- The over-head bar is wired for Mass soldiers via the proxy; generals need a `UWidgetComponent` added on
  the general Blueprint and a small SetTeam/SetHealth push (documented in the guide §3).
- "Mana" maps to the Stamina attribute (no dedicated Mana attribute in the PoC).

## Next Steps

- Author the WBPs and verify in PIE (the pending-user work above).
- Optionally add a match result/victory-defeat widget bound to `UAshMatchSubsystem::OnMatchEnded`
  (still pending from Phase 16).
