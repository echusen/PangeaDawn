// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 


#include "ACFEditorSettings.h"

#include "ACFEditorSubsystem.h"

void UACFEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	GEditor->GetEditorSubsystem<UACFEditorSubsystem>()->RefreshAll();
}
