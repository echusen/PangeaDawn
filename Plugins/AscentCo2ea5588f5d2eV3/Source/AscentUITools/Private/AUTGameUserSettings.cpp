// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AUTGameUserSettings.h"

void UAUTGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    YSensitivity = 1.f;
    XSensitivity = 1.f;
    bInvertY = false;
    bToggleSprint = true;
}

FVector2D UAUTGameUserSettings::GetAxisSensitivity() const
{
    return FVector2D(XSensitivity, YSensitivity);
}

void UAUTGameUserSettings::SetAxisSensitivity(FVector2D axisSensitivity)
{
    XSensitivity = axisSensitivity.X;
    YSensitivity = axisSensitivity.Y;
}

void UAUTGameUserSettings::SetYAxisInverted(bool inYinverted)
{
    bInvertY = inYinverted;
}

void UAUTGameUserSettings::SetToggleSprint(bool inToggleSprint)
{
    bToggleSprint = inToggleSprint;
}

void UAUTGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
    Super::ApplySettings(bCheckForCommandLineOverrides);
    BPApplySettings();
    OnGameSettingsApplied.Broadcast();
}

void UAUTGameUserSettings::BPApplySettings_Implementation()
{
}
