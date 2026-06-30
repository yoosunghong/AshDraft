// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AshDraftCoreRuntime : ModuleRules
{
	public AshDraftCoreRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"EnhancedInput",
				"AIModule",
				"NavigationSystem",
				// Mass Entity foundation for data-oriented soldiers (Phase 9+).
				// MassCore provides FMassEntityHandle (Mass/EntityHandle.h) in UE5.8.
				"MassCore",
				"MassEntity",
                "MassCommon",
                "MassSpawner",
				// StateTree-driven General operational AI (Phase 22). StateTreeModule provides the
				// node base structs (FStateTreeTaskBase / Condition / Evaluator); GameplayStateTreeModule
				// provides UStateTreeAIComponent + UStateTreeAIComponentSchema (AIController-bound).
				"StateTreeModule",
				"GameplayStateTreeModule",
				// Telemetry / QA-bot observation + log export to JSON (Phase 17).
				"Json",
				"JsonUtilities",
				// UMG widgets for the player HUD + over-head unit health bars (Phase 30).
				// UUserWidget / UProgressBar / UTextBlock and the proxy's UWidgetComponent.
				"UMG",
				"DeveloperSettings",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
