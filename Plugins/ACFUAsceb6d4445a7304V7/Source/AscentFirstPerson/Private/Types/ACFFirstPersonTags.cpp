// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Types/ACFFirstPersonTags.h"

namespace ACFFirstPersonTags
{
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_State_Idle, "FirstPerson.Locomotion.State.Idle");
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_State_Walk, "FirstPerson.Locomotion.State.Walk");
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_State_Run, "FirstPerson.Locomotion.State.Run");
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_State_Sprint, "FirstPerson.Locomotion.State.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_State_InAir, "FirstPerson.Locomotion.State.InAir");

	UE_DEFINE_GAMEPLAY_TAG(Locomotion_Event_Jump, "FirstPerson.Locomotion.Event.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Locomotion_Event_Land, "FirstPerson.Locomotion.Event.Land");
}
