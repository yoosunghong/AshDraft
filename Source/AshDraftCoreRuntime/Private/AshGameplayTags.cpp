// Copyright YoosungHong. All Rights Reserved.

#include "AshGameplayTags.h"

namespace AshGameplayTags
{
	// Team classification
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Player, "Ash.Team.Player", "Locally controlled player team.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Ally, "Ash.Team.Ally", "Friendly AI team allied with the player.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Enemy, "Ash.Team.Enemy", "Hostile AI team.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Neutral, "Ash.Team.Neutral", "Non-aligned team.");

	// Unit state
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "Ash.State.Dead", "Unit health reached zero.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "Ash.State.Stunned", "Unit cannot act due to stun.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "Ash.State.Attacking", "Unit is executing an attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Capturing, "Ash.State.Capturing", "Unit is capturing a base.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Commanding, "Ash.State.Commanding", "General is executing its commander order (operational layer).");

	// Abilities
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Basic, "Ash.Ability.Attack.Basic", "Basic attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "Ash.Ability.Attack.Heavy", "Heavy attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ash.Ability.Dodge", "Dodge / evasion ability.");

	// Objectives
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Base_Capture, "Ash.Objective.Base.Capture", "Objective: capture a base.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Base_Defend, "Ash.Objective.Base.Defend", "Objective: defend a base.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Engage, "Ash.Objective.Engage", "Sub-objective: engage a sensed enemy mid-order.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Objective_Assault, "Ash.Objective.Assault", "Sub-objective: assault an enemy stronghold in the path.");

	// GAS data tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Ash.Data.Damage", "Damage magnitude carried into the damage Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StunDuration, "Ash.Data.StunDuration", "Stun length (seconds) carried into the stun Gameplay Effect via SetByCaller.");

	// Animation gameplay events
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Melee, "Ash.Event.Hit.Melee", "Melee montage contact frame; triggers the attack damage sweep.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_Next, "Ash.Event.Combo.Next", "Repeat attack input forwarded to the active combo ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_WindowOpen, "Ash.Event.Combo.WindowOpen", "Combo cancel window opened (montage may be cancelled into the next hit).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_WindowClose, "Ash.Event.Combo.WindowClose", "Combo cancel window closed.");

	// Enhanced Input tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "Ash.InputTag.Move", "Move input (2D axis).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "Ash.InputTag.Look.Mouse", "Look input via mouse.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Stick, "Ash.InputTag.Look.Stick", "Look input via gamepad stick.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Basic, "Ash.InputTag.Attack.Basic", "Basic attack input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Heavy, "Ash.InputTag.Attack.Heavy", "Heavy attack input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Dodge, "Ash.InputTag.Dodge", "Dodge input.");
}
