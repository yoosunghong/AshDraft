// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * AshDraft native Gameplay Tags.
 *
 * These declarations expose the project's shared gameplay vocabulary to C++ so
 * code can reference tags without string literals. The tags are also registered
 * with the GameplayTag manager at startup, making them visible to Blueprints,
 * Data Assets, and the editor tag picker.
 *
 * Naming follows ARCHITECTURE.md section 4.4 (all tags rooted under "Ash.").
 */
namespace AshGameplayTags
{
	// Team classification (ARCHITECTURE 4.4)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Ally);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Enemy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Neutral);

	// Unit state
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Capturing);

	// Abilities
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Basic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Heavy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);

	// Objectives
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Base_Capture);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Objective_Base_Defend);

	// GAS data tags. Data.Damage carries the SetByCaller / computed damage magnitude
	// passed from an attack ability into the damage Gameplay Effect (ARCHITECTURE 5.3).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	// Gameplay events sent from animation. Event.Hit.Melee marks the contact frame
	// of a melee attack montage (via UAshAnimNotify_MeleeHit) so the ability runs
	// its damage sweep at the right moment instead of on activation frame 0.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Melee);

	// Event.Combo.Next is forwarded by the ASC when the attack input is pressed
	// again while the attack ability is already active, so the active combo ability
	// can advance to its next montage (input buffering).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_Next);

	// Combo cancel window, driven by UAshAnimNotifyState_ComboWindow on each attack
	// montage. While open, a repeat attack press cancels the current montage and
	// chains the next hit immediately (removes the recovery "after delay").
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_WindowOpen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_WindowClose);

	// Enhanced Input tags. These are matched against UAshInputConfig entries to
	// route Input Actions to gameplay code / abilities (Lyra-style input).
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Stick);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Basic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Attack_Heavy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Dodge);
}
