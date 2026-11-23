// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Tags the plugin itself needs to reference from C++.
 *
 * Native rather than ini-declared so they are valid from CDO construction onward — the locomotion preset
 * builds its default states in its constructor, which runs long before an ini tag table exists.
 */
namespace ACFFirstPersonTags
{
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_State_Idle);
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_State_Walk);
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_State_Run);
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_State_Sprint);
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_State_InAir);

	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_Event_Jump);
	ASCENTFIRSTPERSON_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locomotion_Event_Land);
}
