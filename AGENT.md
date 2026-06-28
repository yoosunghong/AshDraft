## Purpose

This document defines the working rules for coding agents contributing to the AshDraft project.

AshDraft is a Lyra-based Unreal Engine 5 project focused on building a first playable PoC for a large-scale battlefield action strategy game. The project emphasizes modular gameplay architecture, GAS-based combat, Mass Entity soldiers, hierarchical AI, data-oriented design, and future extensibility for multiplayer and reinforcement-learning-based QA bots.

## Project Directory
C:\Users\Foryoucom\Documents\GitHub\AshDraft\Plugins\GameFeatures\AshDraftCore

## Required Reading Before Work

Before making any code, asset, or configuration change, the coding agent must read the following documents:

1. `PLAN.md`
   - Understand the current phase.
   - Identify the next unchecked task.
   - Do not work on tasks outside the active phase unless explicitly requested.

2. `ARCHITECTURE.md`
   - Understand the current architecture, project rules, naming conventions, and system boundaries.
   - Follow the documented patterns before introducing new systems.

3. Relevant files in `Done/`
   - Review previous completed work when modifying or extending an existing feature.
   - Avoid re-implementing already completed systems.

## Main Working Root

### For every task:

1. Read PLAN.md.
2. Select the next unchecked item or the item explicitly requested by the user.
3. Read ARCHITECTURE.md.
4. Inspect existing code before creating new classes.
5. Prefer data-driven design over hardcoded values.
6. Avoid unnecessary Actor Tick.
7. Update PLAN.md.
8. If the task is complete, create a corresponding DONE_xxxx.md file inside Done/.


### PLAN.md Update Rule

After completing a task, update the relevant checkbox in PLAN.md:

- [x] Implement basic hero movement

If a task is partially complete, do not check it. Instead, add a short note below the task:

- [ ] Implement basic hero movement
  - Partial: Character class created, input binding still missing.

### Done Document Rule

When a task or phase item is completed, create a document in:

Done/

The filename must follow this format:

DONE_<feature_name>.md

Examples:

Done/DONE_project_structure.md
Done/DONE_game_feature_plugin_setup.md
Done/DONE_combat_slot_component.md
Done/DONE_base_capture_system.md
Done/DONE_mass_soldier_fragments.md

Each DONE_xxxx.md file must include:
---
# DONE: <Feature Name>

## Summary

Briefly describe what was implemented.

## Files Changed

List the important files created or modified.

## Implementation Details

Explain the main technical decisions.

## Architecture Notes

Explain how the implementation follows the current architecture.

## Testing / Verification

Describe how the feature was tested or how it should be tested.

## Known Issues

List limitations, unfinished details, or future improvements.

## Next Steps

Suggest the next related task.
---

## Coding Rules

### Avoid Actor Tick by default.

Preferred alternatives:

- GAS events
- Gameplay Tags
- Delegates
- Timers
- Mass Processors
- Subsystems
- AI update intervals
- Time slicing

Tick may be used only when there is a clear reason.

### Dependency Rule

Do not create unnecessary hard dependencies.

Rules:

- UI must not directly control AI.
- AI must not directly depend on UI.
- Mass Entity logic must not strongly reference Player Character.
- Use Subsystems for global coordination.
- Use Gameplay Tags for state and classification.
- Use Data Assets for tunable values.
- Avoid hardcoded gameplay numbers.

## Architecture Rules
Lyra-Style Rules

Follow Lyra-style patterns:

- Game Feature Plugin-based feature ownership
- Experience-driven game setup
- PawnData-driven character configuration
- GAS-based abilities and effects
- Enhanced Input
- Gameplay Tags
- Modular components
- Data Asset-driven tuning

## ECS / Data-Oriented Rules

For large-scale soldiers, prefer data-oriented Mass Entity design.

General soldiers should be represented as Mass Entities when possible.

Use fragments for data:

```
FAshTeamFragment
FAshHealthFragment
FAshMovementFragment
FAshCombatFragment
FAshSquadFragment
FAshLODFragment
```

Use processors for behavior:

```
UAshMassLODProcessor
UAshMassMovementProcessor
UAshMassFormationProcessor
UAshMassCombatProcessor
UAshMassDeathProcessor
UAshMassRepresentationProcessor
```

Avoid giving each soldier a complex independent brain.


## AI Rules

Use hierarchical AI:

Battle Director / Commander AI
        ↓
Squad AI
        ↓
Soldier Local Rules

General rule:

- Commanders make strategic decisions.
- Squads manage group-level movement and objectives.
- Soldiers execute simple local rules.
- Nearby important units may use Actor or Character proxies.
- Far units should be simplified.

## Combat Rules

Use GAS for hero and general combat.

The 8-direction combat slot system should be implemented as a reusable component:

UAshCombatSlotComponent

It should support:

- Slot reservation
- Slot occupation
- Slot release
- Best available slot query
- Slot world location calculation

## Commit Message Rule

Use this format:

type(scope): summary

Examples:

feat(hero): add basic movement character

Valid types:
- feat
- fix
- refactor
- perf
- docs
- test
- chore
- art
- proto

## Completion Criteria

A task is complete only when:

- The implementation exists.
- The code follows project naming and folder rules.
- The implementation is consistent with ARCHITECTURE.md.
- PLAN.md is updated.
- A DONE_xxxx.md file is created in Done/.
- Any known limitations are documented.

## Do Not

Do not:

- Create unnecessary plugins.
- Modify Lyra core code unless explicitly requested.
- Put major game logic in the root project Content/ folder.
- Add complex AI to every soldier Actor.
- Use Actor Tick for large-scale soldier logic.
- Hardcode values that should be Data Assets.
- Skip PLAN.md updates.
- Mark tasks complete without creating a Done/DONE_xxxx.md file.


## Important
Unreal-mcp exists. It allows you to control the editor.

If the mcp tool call fails three or more times, write a guide requesting the user to take direct control of the editor.