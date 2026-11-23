// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIAgentSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AscentAIAgentSettings)

const UACFAIAgentDeveloperSettings* UACFAIAgentDeveloperSettings::Get()
{
	return GetDefault<UACFAIAgentDeveloperSettings>();
}

UACFAIAgentDeveloperSettings* UACFAIAgentDeveloperSettings::GetMutable()
{
	return GetMutableDefault<UACFAIAgentDeveloperSettings>();
}
