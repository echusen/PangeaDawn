// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "Presets/ACFProceduralPresetAimBounceMotion.h"

UACFProceduralPresetAimBounceMotion::UACFProceduralPresetAimBounceMotion()
{
	if (FRichCurve* RichCurve = AimingBounceCurve.GetRichCurve())
	{
		RichCurve->SetKeyInterpMode(RichCurve->AddKey(0.0f, 0.0f), RCIM_Cubic, true);
		RichCurve->SetKeyInterpMode(RichCurve->AddKey(0.7f, 1.0f), RCIM_Cubic, true);
		RichCurve->SetKeyInterpMode(RichCurve->AddKey(1.0f, 0.0f), RCIM_Cubic, true);
	}
}
