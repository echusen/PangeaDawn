// Copyright Recursoft LLC. All Rights Reserved.

#pragma once

#include "SMInputTypes.generated.h"

UENUM()
namespace ESMStateMachineInput
{
	enum Type : uint8
	{
		Disabled,
		/** Use the controller assigned to the context if one is available. */
		UseContextController,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}

UENUM()
namespace ESMNodeInput
{
	enum Type : uint8
	{
		Disabled,
		/** All input values are determined by the owning state machine. */
		UseOwningStateMachine,
		/** Use the controller assigned to the context if one is available. */
		UseContextController,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}