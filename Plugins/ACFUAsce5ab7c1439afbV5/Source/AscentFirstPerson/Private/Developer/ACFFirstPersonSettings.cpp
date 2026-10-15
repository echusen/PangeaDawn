// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Developer/ACFFirstPersonSettings.h"

#define LOCTEXT_NAMESPACE "ACFFirstPersonSettings"

UACFFirstPersonSettings::UACFFirstPersonSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

FName UACFFirstPersonSettings::GetSectionName() const
{
	return TEXT("Ascent First Person Settings");
}

#if WITH_EDITOR
FText UACFFirstPersonSettings::GetSectionText() const
{
	return NSLOCTEXT("ACFFirstPerson", "ACFFirstPersonSettingsSection", "Ascent First Person Settings");
}

FText UACFFirstPersonSettings::GetSectionDescription() const
{
	return NSLOCTEXT("ACFFirstPerson", "ACFFirstPersonSettingsDescription", "Configure Curve Montage and overall settings for ACF First Person.");
}

void UACFFirstPersonSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE