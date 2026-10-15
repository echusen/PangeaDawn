// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralAdditiveMotionComponent.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "ACFFirstPersonLog.h"
#include "Presets/ACFProceduralPreset.h"
#include "Presets/ACFProceduralPresetAdditiveMotion.h"
#include "Developer/ACFFirstPersonSettings.h"

namespace
{
	/** Blend-in rate from the cached pose into a freshly started curve montage. */
	constexpr float CurveMontageBlendSpeed = 12.0f;
}

float FACFProceduralCurveMontage::GetLength() const
{
	float RotMin = 0.0f, RotMax = 0.0f, LocMin = 0.0f, LocMax = 0.0f;

	if (::IsValid(TargetCurveMontageRot))
	{
		TargetCurveMontageRot->GetTimeRange(RotMin, RotMax);
	}

	if (::IsValid(TargetCurveMontageLoc))
	{
		TargetCurveMontageLoc->GetTimeRange(LocMin, LocMax);
	}

	return FMath::Max(RotMax, LocMax);
}

bool FACFProceduralCurveMontage::IsPlaying() const
{
	return CurveMontagePlayback < GetLength();
}

UACFProceduralAdditiveMotionComponent::UACFProceduralAdditiveMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 3;
}

void UACFProceduralAdditiveMotionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SkipOwnerParameters;
	SkipOwnerParameters.bIsPushBased = true;
	SkipOwnerParameters.Condition = COND_SkipOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(UACFProceduralAdditiveMotionComponent, ProceduralCurveMontageTag, SkipOwnerParameters);
}

void UACFProceduralAdditiveMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	ProceduralCurveMontage.CurveMontageBlend = 1.f;
	ProceduralCurveMontage.CurveMontageCache = ProceduralCurveMontage.CurveMontageOut = FTransform::Identity;
}

void UACFProceduralAdditiveMotionComponent::PlayProceduralCurveMontage()
{
	if (!ProceduralCurveMontageTag.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FACFProceduralCurveMontageData* DataPtr = UACFFirstPersonSettings::Get()->CurveMontageData.Find(ProceduralCurveMontageTag);
	if (!DataPtr)
	{
		return;
	}

	if (!::IsValid(DataPtr->CurveMontageLoc) || !::IsValid(DataPtr->CurveMontageRot))
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("Curve montage %s is missing its location or rotation curve."),
			*ProceduralCurveMontageTag.ToString());
		return;
	}

	ProceduralCurveMontage.TargetCurveMontageLoc = DataPtr->CurveMontageLoc;
	ProceduralCurveMontage.TargetCurveMontageRot = DataPtr->CurveMontageRot;

	// Copy the settings by value: the timer outlives this scope and the settings map can be re-entered.
	FTimerDelegate Delegate = FTimerDelegate::CreateWeakLambda(this, [this, Data = *DataPtr]()
	{
		ProceduralCurveMontage.CurveMontageRotScale = Data.CurveMontageRotScale;
		ProceduralCurveMontage.CurveMontageLocScale = Data.CurveMontageLocScale;
		ProceduralCurveMontage.CurveMontageBlend = 0.f;
		ProceduralCurveMontage.CurveMontageCache = ProceduralCurveMontage.CurveMontageOut;

		ProceduralCurveMontage.CurveMontagePlayRate = Data.PlayRate;
		ProceduralCurveMontage.CurveMontagePlayback = 0.0f;
	});

	World->GetTimerManager().ClearTimer(TimerHandle_ProceduralCurveMontage);

	if (DataPtr->Delay <= 0.f)
	{
		Delegate.ExecuteIfBound();
	}
	else
	{
		World->GetTimerManager().SetTimer(TimerHandle_ProceduralCurveMontage, Delegate, DataPtr->Delay, false);
	}
}

void UACFProceduralAdditiveMotionComponent::SetProceduralCurveMontageTag(FGameplayTag InTag)
{
	if (const FACFProceduralCurveMontageData* CurrentData = UACFFirstPersonSettings::Get()->CurveMontageData.Find(ProceduralCurveMontageTag))
	{
		if (!InTag.MatchesTagExact(ProceduralCurveMontageTag) && !CurrentData->bIsCancellable)
		{
			return;
		}
	}

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerSetProceduralCurveMontageTag(InTag);
	}

	ProceduralCurveMontageTag = InTag;

	if (GetOwnerRole() >= ROLE_Authority)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UACFProceduralAdditiveMotionComponent, ProceduralCurveMontageTag, this);
	}

	PlayProceduralCurveMontage();
}

void UACFProceduralAdditiveMotionComponent::ServerSetProceduralCurveMontageTag_Implementation(FGameplayTag InTag)
{
	SetProceduralCurveMontageTag(InTag);
}

void UACFProceduralAdditiveMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetAdditiveMotion* Preset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetAdditiveMotion>(GetEffectivePresetData());
	if (!Preset)
	{
		return;
	}

	const float MaskAlpha = 1.0f - FMath::Clamp(ReadCurve(AnimInstance, Preset->MaskAdditiveCurve), 0.0f, 1.0f);

	// Baked curve motion.
	{
		const FRotator Rotation(
			ReadCurve(AnimInstance, Preset->PitchAxisCurve),
			ReadCurve(AnimInstance, Preset->YawAxisCurve),
			ReadCurve(AnimInstance, Preset->RollAxisCurve));

		const FVector Translation(
			ReadCurve(AnimInstance, Preset->XAxisCurve),
			ReadCurve(AnimInstance, Preset->YAxisCurve),
			ReadCurve(AnimInstance, Preset->ZAxisCurve));

		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = FTransform(Rotation, Translation);
		Out.Weight = FMath::Lerp(Preset->AdditiveScale, Preset->AimingAdditiveScale, Input.AimingState) * MaskAlpha;
		Outs.Add(Out);
	}

	// Curve montage.
	{
		RefreshProceduralCurveMontage(DeltaSeconds);

		FACFProceduralMotionOutput Out;
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = ProceduralCurveMontage.CurveMontageOut;
		Out.Weight = MaskAlpha;
		Outs.Add(Out);
	}
}

void UACFProceduralAdditiveMotionComponent::RefreshProceduralCurveMontage(float DeltaSeconds)
{
	bIsProceduralCurveMontagePlaying = ProceduralCurveMontage.IsPlaying();

	if (!bIsProceduralCurveMontagePlaying)
	{
		if (ProceduralCurveMontageTag.IsValid())
		{
			ProceduralCurveMontageTag = FGameplayTag::EmptyTag;

			if (GetOwnerRole() >= ROLE_Authority)
			{
				MARK_PROPERTY_DIRTY_FROM_NAME(UACFProceduralAdditiveMotionComponent, ProceduralCurveMontageTag, this);
			}
		}

		return;
	}

	if (!::IsValid(ProceduralCurveMontage.TargetCurveMontageLoc) || !::IsValid(ProceduralCurveMontage.TargetCurveMontageRot))
	{
		return;
	}

	ProceduralCurveMontage.CurveMontagePlayback += DeltaSeconds * ProceduralCurveMontage.CurveMontagePlayRate;

	const float Playback = ProceduralCurveMontage.CurveMontagePlayback;
	const FVector Rotation = ProceduralCurveMontage.TargetCurveMontageRot->GetVectorValue(Playback) * ProceduralCurveMontage.CurveMontageRotScale;
	const FVector Translation = ProceduralCurveMontage.TargetCurveMontageLoc->GetVectorValue(Playback) * ProceduralCurveMontage.CurveMontageLocScale;

	const FTransform Current(FRotator(Rotation.Y, Rotation.Z, Rotation.X).Quaternion(), Translation);

	ProceduralCurveMontage.CurveMontageBlend = FMath::Clamp(ProceduralCurveMontage.CurveMontageBlend + DeltaSeconds * CurveMontageBlendSpeed, 0.f, 1.f);

	ProceduralCurveMontage.CurveMontageOut = UKismetMathLibrary::TLerp(ProceduralCurveMontage.CurveMontageCache, Current, ProceduralCurveMontage.CurveMontageBlend);
}

void UACFProceduralAdditiveMotionComponent::OnRep_ProceduralCurveMontageTag()
{
	PlayProceduralCurveMontage();
}

TSubclassOf<UACFProceduralPreset> UACFProceduralAdditiveMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetAdditiveMotion::StaticClass();
}
