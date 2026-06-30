# DONE: Hero Archetype & Progression Data

## Summary

Adds a data-driven hero archetype system (`UAshHeroConfig`) that separates **what kind of hero** (archetype definition) from **how strong this particular instance is** (player progression bonuses). The same hero can appear as a player-controlled character with upgrades or as an AI-controlled enemy general using only the archetype's base stats — with no stat duplication across blueprints.

## Files Changed

| File | Change |
|---|---|
| `Public/Hero/AshHeroConfig.h` | New `UAshHeroConfig : UPrimaryDataAsset` |
| `Public/Hero/AshHeroProgressionData.h` | New `FAshHeroStatBonuses` struct |
| `Public/Character/AshHeroCharacter.h` | Added `HeroConfig`, `StatBonuses`, `GetHeroConfig()`, `ApplyStatBonuses()` |
| `Private/Character/AshHeroCharacter.cpp` | Updated `InitializeAttributes()`, added `ApplyStatBonuses()` |
| `Public/Character/AshGeneralCharacter.h` | Added optional `HeroConfig` |
| `Private/Character/AshGeneralCharacter.cpp` | Updated `InitializeAttributes()` to use HeroConfig if set |

## Implementation Details

### UAshHeroConfig (DA_Hero_*)
One asset per hero archetype. Contains:
- `HeroName` / `Portrait` — identity for hero-select UI
- `BaseMaxHealth` / `BaseAttackPower` / `BaseDefense` / `BaseMaxStamina` — "AI-tier" floor values
- `AICharacterClass` — informational: which `AAshGeneralCharacter` subclass represents this hero as an enemy

### FAshHeroStatBonuses
A `USTRUCT` holding flat additive upgrades. Editable on the hero Blueprint/instance; intended to be loaded from a `USaveGame` in the shipping game. Fields mirror the four base stats (bonus per stat).

### Stat initialization flow
```
Player hero:   HeroConfig.Base* + StatBonuses.Bonus*  →  SetNumericAttributeBase()
AI enemy hero: HeroConfig.Base* only                  →  SetNumericAttributeBase()
Legacy (no config): Initial* floats (unchanged)       →  SetNumericAttributeBase()
```

All values enter GAS as **base values**, so `GameplayEffect` modifiers (buffs / debuffs / abilities) apply additively on top via the normal GAS modifier stack — fully compatible.

### ApplyStatBonuses()
Allows replacing bonuses mid-game (save-game load path). Re-seeds GAS base values immediately and preserves current health proportionally (e.g. at 60% health before upgrade → still 60% after).

## Architecture Notes

- No `Tick` introduced. All attribute changes go through `SetNumericAttributeBase` — the correct GAS API.
- `AAshGeneralCharacter.HeroConfig` is purely optional; existing generals with only `Initial*` floats continue to work without any changes to their blueprints.
- `UAshHeroConfig` is a `UPrimaryDataAsset` so it participates in the asset manager's primary asset scanning.
- The `AICharacterClass` field is informational for designers and spawn systems; nothing auto-spawns from it.

## Testing / Verification

1. Build with the editor closed (new `USTRUCT`/`UPROPERTY`s require full reflection rebuild).
2. Create `DA_Hero_Knight` (or your hero name) from `UAshHeroConfig`.
3. Assign to the hero Blueprint's `HeroConfig`. Set `StatBonuses.BonusMaxHealth = 50` to simulate one upgrade.
4. Assign the same `DA_Hero_Knight` to the enemy general Blueprint's `HeroConfig` (leave `StatBonuses` at zero).
5. PIE: hero should have `BaseMaxHealth + 50` health; enemy general should have `BaseMaxHealth` only.
6. Apply a damage GE to both — verify it reduces health correctly (GAS modifier layering intact).
7. Call `ApplyStatBonuses` mid-game from Blueprint and verify health updates proportionally without death.

## Known Issues

- `FAshHeroStatBonuses` is not yet connected to a save game — values must be set manually on the BP instance. A `UAshHeroProgressionSubsystem` or `USaveGame` integration is the natural next step.
- Ability unlocks (skill trees) are not modeled — only flat stats. Ability-level GAS mechanics can extend this later.

## Next Steps

- Author `DA_Hero_*` assets per hero type; assign to hero and general BPs.
- Implement `USaveGame` for `FAshHeroStatBonuses` and call `ApplyStatBonuses()` on possession.
- Consider adding per-hero ability unlock lists to `UAshHeroConfig` for the skill-tree feature.
