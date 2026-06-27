# Commanding an Army of Thousands in Unreal Engine 5: Mass Entity + AI LOD

### How I gave every soldier on a battlefield a brain — without giving any of them a `Tick()`

---

If you've ever tried to put a few thousand AI soldiers on screen in Unreal Engine, you already know the wall you hit. The first hundred actors are fine. The first five hundred start to sweat. Somewhere past a thousand `ACharacter`s — each running its own Behavior Tree, its own movement component, its own `Tick()` — the frame budget detonates and your "epic battle" becomes a slideshow.

I'm building **AshDraft**, a large-scale battlefield strategy game on top of Unreal's Lyra framework. The core fantasy is simple: *armies*, not squads. That means the engineering problem is never "how do I make one smart soldier" — it's "how do I make ten thousand cheap soldiers that *look* smart together."

This post is about the two systems that make that possible:

1. **Mass Entity** — representing soldiers as data, not actors.
2. **AI LOD** — paying for intelligence only where the player can actually see it.

Let me walk through how they fit together, and the design decisions I'd make again.

---

## Part 1: The Actor doesn't scale — so stop using one

The instinct every Unreal dev has is to make a soldier a `Character`. It's comfortable: you get a skeletal mesh, a movement component, a controller, a Behavior Tree. It's also a luxury sedan when you need ten thousand bicycles.

A single `ACharacter` drags along a `USkeletalMeshComponent`, a `UCharacterMovementComponent`, collision, replication scaffolding, and an `AIController` ticking a Behavior Tree. Multiply that by an army and you're paying for ten thousand independent "brains," ten thousand transform hierarchies, and ten thousand `Tick()` calls — most of them for soldiers the player can't even see.

So in AshDraft, a soldier **is not an actor**. It's a Mass Entity: an ID associated with a handful of compact, plain-old-data fragments.

```cpp
// A soldier is just these structs, packed in cache-friendly arrays.
struct FAshTeamFragment      { EAshTeamId TeamId; };
struct FAshHealthFragment    { float CurrentHealth, MaxHealth; };
struct FAshMovementFragment  { FVector Position, Velocity; float MoveSpeed, FacingYaw; };
struct FAshCombatFragment    { FMassEntityHandle Target; float AttackRange, AttackCooldown, AttackPower; };
struct FAshSquadFragment     { int32 SquadId, OrderId; };
struct FAshLODFragment       { int32 LODLevel; float UpdateInterval, LastUpdateTime; };
```

No methods. No vtable. No `Tick()`. Just data laid out contiguously in memory, which is exactly what a modern CPU wants to chew through.

The behavior doesn't live *in* the soldier — it lives in **processors** that sweep over the whole population at once:

```
SquadTracking → Behavior → Movement → Ground → Combat → Representation
```

This is the mental flip that makes everything else work. Instead of asking ten thousand objects *"what do you want to do this frame?"*, one processor asks *"of all soldiers, which ones are in combat?"* and handles them in a single tight loop over packed arrays. This is data-oriented design, and it's the difference between an `O(actors)` pile of virtual calls and an `O(entities)` linear sweep that the cache loves.

> **The rule I held to:** no soldier gets an independent brain, an Actor, or a `Tick()`. Behavior lives in processors over fragments — full stop.

---

## Part 2: But the player still wants to *see* soldiers

Data in arrays is great for the CPU and useless for the player. Nobody wants to watch a battle of colored debug points.

The trick is decoupling the **simulation** from the **representation**. The Mass simulation is always authoritative — it owns every soldier's position, health, and combat state, whether or not anything is rendered. On top of that sits a thin, pooled "proxy" actor that only exists for the soldiers near the camera.

Here's the part I'm happiest with: **one generic proxy Blueprint, dressed at runtime from data.**

```
DA_<UnitType> (UAshMassSoldierConfig)   → stats + a Visual reference
   └─ Visual ─► DA_<UnitType>_Visual (UAshSoldierVisualConfig)
                  ├─ SkeletalMesh, AnimBP, mesh transform
                  └─ AttackMontage, HitReactMontage
```

The proxy Blueprint ships **empty** — no mesh, no anim. At runtime, the representation processor calls `ConfigureVisual()` on each promoted proxy and dresses it: skeletal mesh, Animation Blueprint, montages, transform offset. It's idempotent — it only re-dresses when a *pooled* proxy is handed to a different unit type, so swapping infantry for archers costs nothing per frame.

The consequence is the part designers love: **adding a new unit type — infantry, archer, cavalry — is pure data authoring.** No new Blueprint, no code, no recompile. You duplicate a data asset, assign a mesh and montages, and point a spawner at it.

Combat animations stay data-oriented too. When a strike lands, the combat processor doesn't reach into a mesh — it sets a one-shot flag on a fragment:

```cpp
struct FAshCombatEventFragment {
    bool bAttackedThisTick = false;   // → proxy plays attack montage
    bool bWasHitThisTick   = false;   // → proxy plays hit-react montage
};
```

The representation processor consumes those flags, plays the montage on the proxy, and clears them the same frame. So an event animates exactly once, and an off-screen soldier never replays a stale swing. The simulation flags *what happened*; the view decides *whether anyone's around to see it*.

### The proxy cap: a perf-vs-UX dial

The pool of proxies is deliberately **bounded**. There's a hard ceiling — `MaxActiveProxies` (default 100) — and once it's hit, the pool simply refuses to dress any more soldiers:

```cpp
// Respect the active cap (bounded Actor count).
if (ActiveProxies.Num() >= MaxActiveProxies)
{
    return nullptr; // this soldier stays a simulated entity with no visible body
}
```

This is the safety valve that keeps the *render* cost bounded the same way LOD keeps the *think* cost bounded. With the cap on, you can pile a huge crowd right next to the camera and the frame rate holds, because the number of animating skeletons can never exceed the cap no matter how many soldiers swarm in.

But it's a genuine tradeoff, not a free win. Cap it too low and you get the classic **popping artifact**: a soldier is fully simulated and very much alive, but has no body to show, so units visibly wink in and out as proxies are recycled at the boundary. That's a real UX cost.

So I treat the cap as a **dial, and one I deliberately removed for performance comparison.** Turning it off (uncapped) is the honest baseline: it shows the *true* unbounded cost of representing everything near the camera, and it eliminates popping entirely. Turning it back on trades that perfect fidelity for a stable, predictable frame budget. Knowing the shape of both ends of that dial — and where the player actually starts noticing the pop — is what lets me pick the cap value with data instead of a guess.

---

## Part 3: AI LOD — the idea that ties it all together

Here's the core insight of the whole project:

> **A soldier 200 meters away, off-screen, behind a hill, does not deserve the same CPU budget as the soldier swinging a sword in the player's face.**

Graphics programmers have used Level of Detail forever — distant meshes drop polygons. **AI LOD** applies the same economics to *thinking*. The further a soldier is from the player, the less often it updates, and the cruder its behavior.

A dedicated processor classifies every soldier each frame by distance from the player pawn, into four rings:

```cpp
int32 UAshMassLODProcessor::ComputeLODLevel(float Distance) const
{
    if (Distance <= LOD0MaxDistance) { return 0; } // near, full fidelity
    if (Distance <= LOD1MaxDistance) { return 1; }
    if (Distance <= LOD2MaxDistance) { return 2; }
    return 3;                                       // far, abstract & cheap
}
```

Each ring carries an **update interval** — how often the expensive systems (combat, behavior) are allowed to touch that soldier:

```cpp
LODUpdateIntervals[0] = 0.05f; // near: ~20× per second
LODUpdateIntervals[1] = 0.15f;
LODUpdateIntervals[2] = 0.5f;
LODUpdateIntervals[3] = 1.0f;  // far: once per second
```

A soldier in the front row re-evaluates its target twenty times a second. A soldier in the back ranks does it once a second — and the player never notices, because that soldier is a handful of pixels in a churning mass. The combat and behavior processors simply skip an entity until `WorldTime - LastUpdateTime >= UpdateInterval`. Same code, a fraction of the cost.

This is the **first** of two distinct throttles, and the one that's genuinely *LOD-based*: how often a soldier is allowed to **think** scales directly with its LOD ring. The second throttle, below, is different — it throttles how often a soldier is **re-classified**, and it's deliberately *LOD-independent*. Keeping the two separate in your head matters; it's easy to say "time slicing by LOD" and mean two unrelated things.

### Time slicing: don't even *classify* everyone every frame

There's a subtler cost: re-computing distance and LOD for the entire population every single frame is itself work. So the LOD pass is **time-sliced** — the army is split into N batches, and only one batch is re-classified per frame. Note that this split is a flat `EntityIndex % BatchCount` — it is *not* keyed off LOD level. A near soldier and a far soldier can land in the same batch; the batching only decides *whose distance gets recomputed this frame*, while the per-LOD interval above decides *how often each soldier acts*:

```cpp
const int32 BatchCount  = NumTimeSliceBatches;        // e.g. 4
const int32 ActiveBatch = FrameCounter % BatchCount;
// ...
if ((EntityIndex % BatchCount) == ActiveBatch)
{
    // recompute this soldier's LOD this frame; the other ¾ keep last frame's
}
```

With four batches, each soldier's LOD ring refreshes every four frames instead of every frame — invisible at distance, and it quarters the classification cost. A soldier doesn't need to know it crossed a LOD boundary the *instant* it happened; "within a few frames" is indistinguishable to the player and four times cheaper.

### All of it is data, not magic numbers

Every threshold above — the ring distances, the per-ring cadence, the batch count — lives on an editable data asset, not baked into the processor:

```cpp
UCLASS(BlueprintType)
class UAshMassLODConfig : public UDataAsset
{
    float LOD0MaxDistance = 2000.f;
    float LOD1MaxDistance = 6000.f;
    float LOD2MaxDistance = 15000.f;
    TArray<float> LODUpdateIntervals = { 0.05f, 0.15f, 0.5f, 1.0f };
    int32 NumTimeSliceBatches = 4;
};
```

The processor loads this once and falls back to sane defaults when nothing's assigned. Why bother? Because I want to tune LOD aggressiveness — tighten the rings, slow the far ranks, change the batch count — *without recompiling*. It also means an automated perf-sweep (eventually a reinforcement-learning QA bot) can search the parameter space directly. There's even a master toggle: flip LOD off and **every** soldier snaps to full fidelity, which is exactly the baseline you want when measuring what LOD actually bought you.

---

## Part 4: Local rules, not a thousand strategists

The last piece is *what* a soldier thinks about — and the answer is "as little as possible."

AshDraft uses hierarchical AI:

```
Battle Director / Commander  →  Squad  →  Soldier (local rules only)
```

Commanders make strategy. Squads own group movement and objectives. And a soldier? A soldier runs a tiny data-oriented state machine — three states — over its fragments:

```cpp
enum class EAshSoldierState : uint8 {
    FollowSquad, // no local enemy: advance on the squad's shared objective
    Engage,      // enemy sensed nearby but out of reach: close in, facing it
    Attack,      // enemy in range: hold position and strike on cooldown
};
```

Critically, a soldier **never runs a map-wide target search.** It senses only hostiles inside a small radius using a coarse spatial hash, and it's *leashed* — it'll engage anyone who crosses its path during the march, but if it strays too far chasing a straggler, it drops the target and rejoins the advance. The leash is measured from where the soldier first made contact, so combat triggers on contact mid-march without ever degenerating into ten thousand soldiers independently pathfinding across the whole map.

This is the AAA-correct version of a "state tree" at army scale. Unreal's actual `StateTree` is wonderful — and I deliberately reserve it for the *handful* of complex Commander and Squad agents, where a real visual state machine earns its keep. The soldiers, who outnumber the commanders thousands to one, get the cheap data-oriented FSM. Spend complexity where it's rare; stay flat where it's massive.

It also forced some unglamorous-but-essential polish, all done as steering forces rather than physics (physics collision is the wrong tool for a crowd this size):

- **Facing** is decoupled from velocity, so a stopped soldier in melee still faces its enemy.
- **Separation** only pushes *same-team* crowders, with clamped, relaxed forces so a cluster settles instead of vibrating.
- **Arrival ease-in** slows soldiers as they near a shared objective, so ranks fan out into a stable formation instead of ramming the exact goal point and rebounding forever.
- **Ground conforming** line-traces soldiers onto the terrain — but only for the visible LOD-0 ranks, because a debug point on a far hill doesn't care about its Z.

That last bullet is AI LOD leaking, beautifully, into every system: even *standing on the ground correctly* is a privilege reserved for soldiers close enough to matter.

---

## What I'd tell my past self

If you're staring down the same problem — hundreds or thousands of agents in Unreal — here's the compressed version:

1. **Stop reaching for `ACharacter`.** Represent the crowd as data (Mass Entity / ECS). Behavior goes in processors that sweep the population, not in per-object `Tick()`.
2. **Separate simulation from representation.** The sim is authoritative and always runs; the expensive visuals are a thin, pooled view layered only on what's near the camera.
3. **LOD your AI like you LOD your meshes.** Distance buys you the right to update less often and think more crudely. Time-slice even the classification.
4. **Push every threshold into data assets.** Distances, cadences, batch counts, sense radii, leashes — all tunable, none recompiled. Your future self (and your perf bots) will thank you.
5. **Keep the soldiers dumb and the commanders smart.** Hierarchy means you spend intelligence where it's rare and stay flat where it's massive.

The result is an army where the soldier in your face animates, faces you, and reacts every frame — and the soldier on the far hill costs almost nothing — and the player can't tell where the line is. That seam, invisible and deliberate, is the whole trick.

---

*AshDraft is an in-development Lyra-based UE5 project. The systems above are a working proof-of-concept — the numbers shown are current tuning defaults, not shipped benchmarks. Next up: promoting the emergent formation packing into authored formation slots, and wiring an RL-based QA bot to sweep the LOD parameter space automatically.*

*Thanks for reading. If you're working on large-scale crowds or Mass Entity in UE5, I'd love to compare notes.*
