# Phase 30 — UI Widget Setup Guide (Player HUD + Unit Health Bars)

This guide walks through authoring the Widget Blueprints in the editor on top of the C++ widget base
classes added in this phase. **No C++ is needed** — every binding is by widget name, and every data
source is already wired in code.

> Build first. This phase adds new `UCLASS`/`USTRUCT`/`UPROPERTY`s, so **build with the editor closed**
> (see `build-command`), then reopen the editor. The widget base classes (`UAshHUDWidget`,
> `UAshUnitHealthBarWidget`, `UAshTargetInfoWidget`, `UAshUserWidget`) will then appear in the
> "Pick Parent Class" list.

---

## 0. What the code provides

| C++ class | Role | You author |
| --- | --- | --- |
| `UAshUserWidget` | Common base (abstract). | — |
| `UAshHUDWidget` | Player HUD root: health + mana bars, kill count, recently-struck panel. | `WBP_AshHUD` |
| `UAshUnitHealthBarWidget` | Over-head soldier/general health bar (cyan ally / red enemy). | `WBP_AshUnitHealthBar` |
| `UAshTargetInfoWidget` | Recently-struck-enemy panel (name / affiliation / health). | `WBP_AshTargetInfo` |
| `UAshCombatFeedSubsystem` | Sources kill count + last-struck unit (auto, world subsystem). | — |
| `UAshUIStatics` | `GetTeamDisplayText` / `GetTeamHealthColor` Blueprint helpers. | — |

The HUD reads the **hero's GAS attributes** for the bars (Health/MaxHealth and Stamina/MaxStamina —
**mana is the Stamina attribute**, the project's secondary pool; there is no separate Mana attribute).
Kill count and the struck-enemy panel are fed by `UAshCombatFeedSubsystem`, which records only the
**local player's** strikes from the two damage paths (hero melee on Mass soldiers, and GAS damage on
generals).

**Binding rule:** name an inner widget exactly like the C++ property and it binds automatically
(`meta=(BindWidgetOptional)` — so every element is optional; omit any you don't want). Names below.

---

## 1. `WBP_AshTargetInfo` — recently-struck-enemy panel

1. Content Browser → right-click → **User Interface → Widget Blueprint**.
2. In "Pick Parent Class" (or **File → Reparent Blueprint** afterwards), choose **AshTargetInfoWidget**.
3. Name it `WBP_AshTargetInfo`. Suggested location: `Content/UI/`.
4. In the Designer, add a horizontal/vertical box with:
   - a **TextBlock** named exactly **`UnitNameText`**,
   - a **TextBlock** named exactly **`AffiliationText`**,
   - a **ProgressBar** named exactly **`HealthBar`**.
5. (Optional) On the widget's **Class Defaults**: `bTintHealthBarByTeam` / `bTintAffiliationByTeam`
   recolour the bar/label by team (cyan friendly, red enemy). Leave on for the requested look.

No graph work is required — `SetTargetInfo` (called by the HUD) fills all three.

---

## 2. `WBP_AshHUD` — player HUD root

1. New Widget Blueprint, parent class **AshHUDWidget**, name `WBP_AshHUD`.
2. Lay out a full-screen **Canvas Panel** with these named widgets (all optional; add what you want):

   **Bottom-centre (health over mana):**
   - **ProgressBar** `HealthBar` — the live health fill.
   - **ProgressBar** `HealthBarTrail` *(optional)* — place it **behind** `HealthBar`, same size/position,
     tinted a "lost health" colour (e.g. white/dark-red). It lags behind to show the chunk just lost.
   - **ProgressBar** `ManaBar` — below the health bar.
   - **ProgressBar** `ManaBarTrail` *(optional)* — behind `ManaBar`.

   **Bottom-right:**
   - **TextBlock** `KillCountText`.

   **Top-left:**
   - A **`WBP_AshTargetInfo`** instance named `RecentTargetPanel` (add your `WBP_AshTargetInfo` from the
     palette → it appears under "User Created"; rename the instance to `RecentTargetPanel`).

3. **Class Defaults** (tunables, optional):
   - `Bar Interp Speed` (default 9) — how snappy the front bars chase the real value.
   - `Trail Interp Speed` (default 2.5) — lower = the lost-health chip lingers longer.
   - `Kill Count Format` (default `Kills: {0}`) — `{0}` is the number.
   - `Hide Target Panel Until First Strike` (default true).

4. Polish hooks (optional, in the widget Graph): override **On Kill Count Changed** and
   **On Unit Struck** events for pop/fade animations. The data is already applied before they fire.

> The bars animate themselves — the HUD interpolates each bar's percent every frame, so health/mana
> glide down smoothly on damage. You do **not** need UMG animation tracks for the smooth-decrease
> requirement (you may still add a UMG anim on the polish hooks for extra flair).

### Showing the HUD

Add it to the screen once the player exists. Easiest options:
- In your player **HUD/PlayerController Blueprint** (or the hero BP's BeginPlay): **Create Widget**
  (`WBP_AshHUD`, owning player = player 0) → **Add to Viewport**.
- Or via the Lyra experience UI layer if you have one wired.

The HUD self-binds to the hero and the combat feed; if it is created before the pawn is possessed it
retries automatically, so timing is not critical.

---

## 3. `WBP_AshUnitHealthBar` — over-head unit bar

1. New Widget Blueprint, parent class **AshUnitHealthBarWidget**, name `WBP_AshUnitHealthBar`.
2. Add a **ProgressBar** named exactly **`HealthBar`**. Keep the widget small (e.g. 120×14) and
   transparent background.
3. That's it — `SetTeam` colours it (cyan for Player/Ally, red for Enemy) and `SetHealth` fills it,
   both driven by the proxy each frame.

### Attach it to soldiers

Note: `UnitNameText` is optional and remains collapsed until code supplies a non-empty name. Soldier
proxies use the native `AshSoldierHealthBarWidget` by default, a compact 30x6 bar that is separate from
the larger `WBP_AshUnitHealthBar` used by generals. Soldier health bars are enemy-only: Player/Ally
soldier proxies hide their `HealthBarWidget`.

The soldier proxy (`B_Soldier_Proxy`) already has a **`HealthBarWidget`** component (screen-space,
positioned above the head). Assign the class:

1. Open **`B_Soldier_Proxy`** (your `AAshSoldierProxyActor` Blueprint).
2. Select the **HealthBarWidget** component.
3. Leave **Widget Class** empty or set it to **AshSoldierHealthBarWidget**. Do not assign
   `WBP_AshUnitHealthBar` here; that widget is intentionally larger and is for generals.

Because a proxy only exists while its entity is **promoted near the player** (representation LOD), the
bar is shown only "together with the mesh when it is rendered" — far soldiers cost nothing. To change
which LOD shows proxies/bars, edit `UAshMassRepresentationProcessor::PromoteAtOrBelowLOD`
(`DefaultGame.ini`, default 0; raise to 1 to show at LOD ≤ 1).

### Generals (optional)

Generals are full Characters. To give a general an over-head bar, add a **WidgetComponent** to the
general Blueprint, set its Widget Class to `WBP_AshUnitHealthBar`, and on the general BP's
`OnHealthChanged`/tick push values: `Get User Widget Object → Cast to AshUnitHealthBarWidget →
SetTeam(GetTeamId)` once and `SetHealth(GetHealth / GetMaxHealth)` on health change.

---

## 4. Names & affiliations in the struck-enemy panel

- **Soldiers:** set **`DisplayName`** on each `DA_*_Visual` (`UAshSoldierVisualConfig`) — e.g.
  "Infantry". Empty falls back to the affiliation label.
- **Generals / hero:** set **`DisplayName`** on the character Blueprint (Ash|UI category). Empty falls
  back to the affiliation label.
- **Affiliation** text/colour comes from the unit's team automatically.

---

## 5. PIE verification checklist

- [ ] HUD appears: health bar (bottom-centre) with mana below, kill count (bottom-right).
- [ ] Take damage → health bar **glides** down smoothly; the trail bar (if added) lags then catches up.
- [ ] Spend stamina (dodge/heavy, when wired) → mana bar animates the same way.
- [ ] Strike an enemy → top-left panel shows its name, "Enemy", and a red health bar that drops per hit.
- [ ] Kill enemies (soldiers and a general) → kill count increments.
- [ ] Near soldiers show an over-head bar **only while their mesh is rendered**; ally bars are cyan,
      enemy bars red.
- [ ] AI-vs-AI damage does **not** change the player's kill count or struck-enemy panel.
