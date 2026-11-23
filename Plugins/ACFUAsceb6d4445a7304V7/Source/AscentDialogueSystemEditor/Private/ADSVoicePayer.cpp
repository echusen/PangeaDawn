// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSVoicePayer.h"

namespace
{
	TSharedPtr<IADSVoicePayer>& RegisteredPayer()
	{
		static TSharedPtr<IADSVoicePayer> Payer;
		return Payer;
	}
}

void ADSVoicePayers::Register(const TSharedRef<IADSVoicePayer>& Payer)
{
	RegisteredPayer() = Payer;
}

void ADSVoicePayers::Unregister(const TSharedRef<IADSVoicePayer>& Payer)
{
	if (RegisteredPayer() == Payer)
	{
		RegisteredPayer().Reset();
	}
}

TSharedPtr<IADSVoicePayer> ADSVoicePayers::GetRegistered()
{
	return RegisteredPayer();
}

TSharedPtr<IADSVoicePayer> ADSVoicePayers::GetPaying()
{
	const TSharedPtr<IADSVoicePayer>& Payer = RegisteredPayer();
	return (Payer.IsValid() && Payer->IsPaying()) ? Payer : nullptr;
}
