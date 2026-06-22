# Project Integration — AshDraftCore

This repository is the **AshDraftCore Game Feature Plugin**, extracted to be
self-contained. It is *not* a full Unreal project — it is meant to be dropped
into a Lyra-based project. This document lists everything outside the plugin
folder that the plugin needs in order to run, so you can reconstitute the
project on any machine.

## 1. Prerequisites

- An Unreal Engine **Lyra** sample project, cloned/downloaded separately
  (matching the engine version this plugin targets — UE 5.8 / Mass).
- The base Lyra content and base plugins (they ship with the Lyra sample;
  this repo does **not** vendor them).

## 2. Place the plugin

Clone this repo into the Lyra project so its folder lands at:

```
<LyraProject>/Plugins/GameFeatures/AshDraftCore/
```

The repo root (the folder containing `AshDraftCore.uplugin`) must be the
`AshDraftCore` directory itself.

> `AshDraftCore.uplugin` is `EnabledByDefault: true` with
> `BuiltInInitialFeatureState: Active`, so as a Game Feature Plugin it is
> discovered and activated automatically — **no entry in `AshDraft.uproject`
> is required.** The engine plugins it depends on (`GameplayAbilities`,
> `MassGameplay`) are already enabled in a stock Lyra project.

## 3. Apply the project-level config

Append the following block to the Lyra project's
`Config/DefaultGame.ini`. It configures the Mass representation processor
(and pins the proxy class, which otherwise resets on editor restart):

```ini
[/Script/AshDraftCoreRuntime.AshMassRepresentationProcessor]
PromoteAtOrBelowLOD=0
MaxActiveProxies=100
ProxyClass=/AshDraftCore/Game/Mass/BP_SoldierProxy.BP_SoldierProxy_C
```

> This is the only project-level config delta. If you later add gameplay
> tags, input mappings, or asset-manager scan paths at the project level
> rather than inside the plugin's own `Config/`, record them here too.

## 4. Vendored art is already included

The `Content/Data/Hero/ParagonSunWukong/` pack (Paragon: Sun Wukong) is
committed directly in this repo as plain Git objects — you do **not** need to
re-import it from Fab. It is large (~2.3 GB), so the initial clone will be
correspondingly large.

The base Lyra sample content (root `Content/`) is **not** vendored here; it
comes from the Lyra project itself.

## 5. Build

From the Lyra project root:

```
# Regenerate project files (right-click the .uproject → Generate, or):
<Engine>/Build/BatchFiles/Build.bat AshDraftEditor Win64 Development -Project="<...>/AshDraft.uproject"
```

Then open the project; the AshDraftCore game feature activates on load.
