// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

/**
 * Factory set by AscentACFAssistant so AscentAIAgent can mount the native Slate panel
 * without a circular module dependency.
 */
DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FACFAssistantPanelFactory);

ASCENTAIAGENT_API FACFAssistantPanelFactory& GetACFAssistantPanelFactory();
