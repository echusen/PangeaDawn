// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantPanelFactory.h"

FACFAssistantPanelFactory& GetACFAssistantPanelFactory()
{
	static FACFAssistantPanelFactory Factory;
	return Factory;
}
