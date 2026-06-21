
# ARCHITECTURE.md

## Project Overview

AshDraft is a large-scale battlefield action strategy game built on Unreal Engine 5 using a Lyra-style architecture.

The first PoC focuses on proving that the following systems can work together:

- One playable hero/general character
- One enemy general
- Ally and enemy soldiers
- Capturable bases
- 8-direction melee combat slots
- GAS-based combat
- Mass Entity-based large-scale soldiers
- Hierarchical AI
- AI LOD
- Time slicing
- Flow Field-style group movement
- Future QA bot and reinforcement learning integration

The goal is not to build a complete commercial game in the first phase. The goal is to validate the core technical architecture.

---

# 1. High-Level Architecture

AshDraft uses a hybrid architecture:

```text
Lyra-style Game Feature Plugin
        +
GAS-based hero/general combat
        +
Mass Entity data-oriented soldier simulation
        +
Hierarchical AI
        +
AI LOD and time-sliced updates
```
The first PoC uses one primary Game Feature Plugin:

Plugins/GameFeatures/AshDraftCore

Additional plugins should not be created until the project has a working vertical slice.


# 2. AshDraftCore Plugin Responsibility

AshDraftCore owns the first PoC gameplay implementation.

It contains:
- Playable hero
- Enemy general
- Soldier systems
- Base system
- Combat slot system
- GAS combat foundation
- Commander AI
- Squad AI
- Mass soldier simulation
- AI LOD
- Flow Field PoC
- PoC map and experience assets

# 3. Project Directory
I already inplace directory base structure

# 4. Lyra-Style Design

AshDraft follows Lyra-style design principles:

## 4.1 Game Feature Plugin Ownership

Game-specific systems are placed in a Game Feature Plugin instead of directly modifying Lyra core code.

This keeps the project modular and reduces upgrade risk.

## 4.2 Experience-Driven Setup

Game mode setup should be driven by Experience assets.

The PoC should have an experience such as:

```text
DA_Experience_AshDraft_PoC
```

The experience should define the initial gameplay setup, including PawnData, input, UI, and game rules.

## 4.3 PawnData-Driven Character Setup

Playable characters should be configured through PawnData-style assets where possible.

The hero should not be configured only through hardcoded C++ values.

## 4.4 Gameplay Tags

Gameplay Tags are the shared vocabulary for state, team, ability, and objective logic.

Example tags:

```text
Ash.State.Dead
Ash.State.Stunned
Ash.State.Attacking
Ash.State.Capturing

Ash.Team.Player
Ash.Team.Ally
Ash.Team.Enemy
Ash.Team.Neutral

Ash.Ability.Attack.Basic
Ash.Ability.Attack.Heavy
Ash.Ability.Dodge

Ash.Objective.Base.Capture
Ash.Objective.Base.Defend
```

## 4.5 Enhanced Input

Player input should use Enhanced Input.

Input actions and mapping contexts should live under:

```text
Content/Input/
```

---

# 5. Gameplay Ability System Architecture

Hero and general combat should use GAS.

## 5.1 GAS Responsibilities

GAS is responsible for:

* Basic attack
* Heavy attack
* Dodge
* Damage
* Buffs and debuffs
* Stun
* Invincibility
* Death state
* Ability activation rules

## 5.2 Attribute Set

Initial attributes:

```text
Health
MaxHealth
AttackPower
Defense
Stamina
MaxStamina
```

## 5.3 Gameplay Effects

Initial Gameplay Effects:

```text
GE_Damage_Physical
GE_Heal_BaseRecovery
GE_State_Stunned
GE_State_Invincible
```

## 5.4 Gameplay Abilities

Initial Gameplay Abilities:

```text
GA_Hero_BasicAttack
GA_Hero_HeavyAttack
GA_Hero_Dodge
GA_General_BasicAttack
```

## 5.5 Authority Rule

Even though the first PoC is single-player, combat should be structured so that future server authority is possible.

Damage, death, and base capture should not be designed as purely cosmetic local events.

---

# 6. Character Architecture

## 6.1 Playable Hero

The playable hero uses:

```text
ACharacter
Enhanced Input
GAS
Animation Blueprint
Camera Component
Character Movement Component
```

Example class:

```cpp
AAshHeroCharacter
```

The hero is high fidelity and should support precise combat, hit reaction, input, camera, and interaction.

## 6.2 Enemy General

The enemy general also uses `ACharacter`.

Example class:

```cpp
AAshEnemyGeneralCharacter
```

The enemy general is not a Mass Entity because it requires high-fidelity decision-making, animation, combat behavior, and direct interaction with the player.

General AI may use:

```text
AIController
Behavior Tree
Blackboard
Gameplay Tags
GAS
```

## 6.3 Soldiers

General soldiers should eventually use Mass Entity.

Nearby important soldiers may temporarily use Actor or Character proxies for visual fidelity.

---

# 7. ECS and Data-Oriented Architecture

AshDraft uses an ECS-style and data-oriented approach for large-scale soldier simulation.

The goal is to avoid creating hundreds or thousands of expensive independent `ACharacter` instances.

## 7.1 Entity

A soldier is represented as a Mass Entity.

A Mass Entity is an ID associated with fragments.

It should not contain complex object-oriented behavior by itself.

## 7.2 Components / Fragments

Soldier data should be stored in fragments.

Initial fragments:

```cpp
FAshTeamFragment
FAshHealthFragment
FAshMovementFragment
FAshCombatFragment
FAshSquadFragment
FAshLODFragment
```

Example conceptual data:

```cpp
struct FAshTeamFragment
{
    int32 TeamId;
};

struct FAshHealthFragment
{
    float CurrentHealth;
    float MaxHealth;
};

struct FAshMovementFragment
{
    FVector Position;
    FVector Velocity;
    float MoveSpeed;
};

struct FAshCombatFragment
{
    int32 TargetEntityId;
    float AttackRange;
    float AttackCooldown;
    float AttackPower;
};

struct FAshSquadFragment
{
    int32 SquadId;
    int32 OrderId;
};

struct FAshLODFragment
{
    int32 LODLevel;
    float UpdateInterval;
    float LastUpdateTime;
};
```

## 7.3 Systems / Processors

Behavior should be implemented through Mass Processors.

Initial processors:

```cpp
UAshMassLODProcessor
UAshMassMovementProcessor
UAshMassFormationProcessor
UAshMassCombatProcessor
UAshMassDeathProcessor
UAshMassRepresentationProcessor
```

## 7.4 Data-Oriented Rules

Follow these rules:

* Keep soldier data compact.
* Process soldiers in batches.
* Avoid per-soldier Actor Tick.
* Avoid per-soldier Behavior Trees.
* Avoid individual pathfinding for every soldier.
* Prefer shared targets and shared movement fields.
* Prefer simple local rules for individual soldiers.
* Store tunable values in Data Assets or config structures.

## 7.5 Why This Matters

AshDraft needs hundreds or thousands of soldiers on the battlefield.

Traditional Actor-heavy design creates high Game Thread cost.

Mass Entity and data-oriented design allow:

* Better cache locality
* Batch processing
* Lower per-unit overhead
* Easier LOD control
* Cleaner large-scale simulation

````

```md
# 8. Hierarchical AI Architecture

AshDraft AI is not designed as a fully independent brain for every soldier.

The AI hierarchy is:

```text
Battle Director / Commander AI
        ↓
Army / Team Controller
        ↓
Squad AI
        ↓
Soldier Local Rules
````

## 8.1 Commander AI

The Commander decides strategic objectives.

Responsibilities:

* Analyze base ownership
* Decide attack or defense objectives
* Assign squad orders
* React to base capture events
* React to general death events

Example orders:

```text
Order.AttackBase
Order.DefendBase
Order.CapturePoint
Order.ReinforceHero
Order.Retreat
```

## 8.2 Squad AI

Squad AI controls groups of soldiers.

Responsibilities:

* Maintain squad objective
* Track average squad position
* Track alive unit count
* Maintain formation
* Handle group movement
* Request new orders when needed

## 8.3 Soldier AI

Soldier AI is intentionally simple.

Responsibilities:

* Move toward squad objective
* Maintain rough formation
* Detect nearby enemy
* Attack when in range
* Avoid excessive overlap
* Die when health reaches zero

Soldiers must not perform expensive strategic decision-making.

---

# 9. AI LOD and Time Slicing

AshDraft only simulates nearby and important units with high fidelity.

## 9.1 LOD Levels

```text
LOD 0: Near player, high-fidelity combat
LOD 1: Mid-range, simplified movement and combat
LOD 2: Far battlefield, squad-level simulation
LOD 3: Very far or offscreen, abstract simulation
```

## 9.2 LOD Factors

LOD should consider:

```text
DistanceToPlayer
CameraVisibility
ImportanceScore
NearBase
NearHero
RecentlyDamaged
CurrentObjective
```

## 9.3 Update Frequency

Example update rates:

```text
Hero: every frame
Enemy General: 30-60 Hz
Nearby Soldiers: 10-20 Hz
Mid-range Soldiers: 5-10 Hz
Far Squads: 1-2 Hz
Abstract Battle Simulation: <= 1 Hz
```

## 9.4 Time Slicing

Large soldier updates should be distributed across frames.

Example:

```text
Frame 1: Soldier Batch 0
Frame 2: Soldier Batch 1
Frame 3: Soldier Batch 2
Frame 4: Soldier Batch 3
```

This avoids updating every unit every frame.

---

# 10. Combat Slot Architecture

Important combat targets such as the player and enemy generals use 8-direction combat slots.

Example component:

```cpp
UAshCombatSlotComponent
```

## 10.1 Purpose

The combat slot system exists to:

* Prevent soldiers from overlapping around a target
* Improve melee readability
* Limit the number of active attackers
* Create a readable surrounding effect
* Support musou-style combat pacing

## 10.2 Slot States

```text
Empty
Reserved
Occupied
Attacking
Cooldown
```

## 10.3 Component Responsibilities

```text
RequestSlot()
ReleaseSlot()
GetSlotWorldLocation()
FindBestAvailableSlot()
```

This component should not directly own complex soldier AI. It only manages slot state around a target.

---

# 11. Base / Stronghold Architecture

Bases are battlefield objectives.

Example class:

```cpp
AAshBaseActor
```

## 11.1 Base Responsibilities

A base owns:

```text
Team ownership
Durability
Contested state
Defender count
Spawn points
Capture events
Recovery behavior
```

## 11.2 Capture Rule

The first PoC uses a durability-based capture model:

* Bases have durability.
* Defender deaths reduce durability.
* When durability reaches zero, ownership changes.
* After ownership changes, new defenders spawn after a delay.
* If no enemy is inside the base, durability can recover.
* If both teams are present, the base is contested and does not recover.
* Ownership change events notify AI systems.

---

# 12. Movement Architecture

Different unit types use different movement approaches.

| Unit Type            | Movement                                 |
| -------------------- | ---------------------------------------- |
| Player               | Direct input                             |
| Enemy General        | NavMesh                                  |
| Nearby Actor Soldier | NavMesh or steering                      |
| Mass Soldier         | Flow Field or shared objective direction |
| Far Squad            | Abstract base graph movement             |

## 12.1 Flow Field PoC

Flow Field movement is used to reduce individual pathfinding cost.

Concept:

```text
Target Base -> Grid Cost -> Best Direction Per Cell -> Soldier Movement
```

Flow Field movement should first be implemented as a simple PoC and compared with direct movement.

---

# 13. Hybrid Mass / Actor Representation

AshDraft uses Mass Entity for most soldiers.

However, nearby soldiers may need higher fidelity.

## 13.1 Promotion

A Mass soldier may be promoted to an Actor proxy when:

```text
Near player
Visible to camera
Near hero/general
Recently damaged
In combat slot range
```

## 13.2 Demotion

An Actor proxy may be demoted back to Mass when:

```text
Far from player
Not visible
Not recently damaged
Not important to current combat
```

## 13.3 State Transfer

Promotion and demotion must preserve important state:

```text
Team
Health
Position
Velocity
SquadId
LODLevel
Combat state
```

---

# 14. QA Bot and RL Extension Interface

The first PoC does not implement full reinforcement learning.

However, it should prepare a future QA bot interface.

## 14.1 Observation

Example observation:

```json
{
  "player_health": 85,
  "player_position": [120.0, 40.0, 0.0],
  "nearby_enemy_count": 12,
  "owned_bases": 1,
  "enemy_bases": 2,
  "current_objective": "CaptureCentralBase"
}
```

## 14.2 Action

Example action:

```json
{
  "move": [1.0, 0.0],
  "camera_yaw": 15.0,
  "attack": true,
  "dodge": false,
  "skill_1": false
}
```

## 14.3 Interface Shape

Future interface:

```text
Reset()
Step(Action)
GetObservation()
GetReward()
IsDone()
```

## 14.4 Logging

The PoC should eventually log:

```text
Combat events
Damage events
Base ownership changes
Player death
General death
Victory / defeat
Frame performance
Soldier counts
Active proxy counts
```

---

# 15. Multiplayer Extension Principles

The first PoC is single-player.

However, systems should avoid blocking future multiplayer.

Rules:

* Design important gameplay events with server authority in mind.
* Damage should be applicable from an authoritative context.
* Death should be event-driven.
* Base ownership should not be purely cosmetic.
* Gameplay Tags should represent network-relevant state cleanly.
* Do not assume every soldier will be individually replicated.
* Future multiplayer should replicate high-importance soldiers and squad/base-level state rather than every far soldier.

---

# 16. UI Architecture

UI should observe gameplay state but should not own gameplay logic.

Rules:

* UI must not directly control AI.
* UI must not directly mutate base ownership.
* UI should read state through components, subsystems, or view models.
* Base capture UI should subscribe to base state changes.
* Squad health UI should read squad state, not individual soldier Actors when possible.

---

# 17. Data Asset Rules

Use Data Assets for tunable values.

Examples:

```text
Hero stats
General stats
Soldier archetypes
Base settings
Squad settings
AI LOD settings
Flow Field settings
Combat slot settings
Ability sets
```

Avoid hardcoding values such as:

```text
Attack damage
Base durability
Capture radius
Slot radius
Soldier movement speed
LOD distance thresholds
```

---

# 18. Coding Standards

## 18.1 Naming

All AshDraft-specific classes use the `Ash` prefix.

Examples:

```cpp
AAshHeroCharacter
AAshEnemyGeneralCharacter
AAshBaseActor
UAshCombatSlotComponent
UAshCommanderSubsystem
UAshMassMovementProcessor
FAshTeamFragment
FAshHealthFragment
EAshTeamId
```

## 18.2 One Class Per File

Use one `.h` and `.cpp` pair per class.

## 18.3 Tick Policy

Actor Tick is disabled by default.

Use Tick only when necessary and document why.

Preferred alternatives:

```text
Mass Processor
Timer
Delegate
Gameplay Event
Ability Task
Subsystem update
Time-sliced batch update
```

## 18.4 Dependency Policy

Avoid circular dependencies.

Rules:

* UI does not directly control AI.
* AI does not directly depend on UI.
* Mass processors do not strongly reference hero character directly.
* Use subsystem queries or weak references where necessary.
* Prefer tags, IDs, and data handles over hard object references.

---

# 19. Current PoC Priority

The current priority is vertical functionality, not perfect abstraction.

Build in this order:

```text
Project foundation
Experience and input
Hero movement
GAS combat
Enemy general
Combat slots
Base system
Soldier prototype
Mass soldier foundation
Hierarchical AI
AI LOD
Flow Field PoC
QA Bot interface
Performance validation
```

Do not split the project into multiple Game Feature Plugins until the first PoC is playable.

---

# 20. Architecture Summary

AshDraft's first PoC is a hybrid Unreal Engine architecture:

```text
High-fidelity ACharacter hero/general
        +
GAS combat
        +
Mass Entity soldiers
        +
ECS/data-oriented processing
        +
Hierarchical AI
        +
AI LOD and time slicing
        +
Lyra-style Game Feature structure
```

The main principle is:

```text
Important things are simulated precisely.
Large-scale things are simulated cheaply.
Strategic decisions are centralized.
Soldier behavior is data-oriented and simple.
```
