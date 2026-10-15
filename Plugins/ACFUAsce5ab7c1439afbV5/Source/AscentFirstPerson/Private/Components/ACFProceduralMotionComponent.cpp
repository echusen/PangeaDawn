// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralMotionComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Presets/ACFProceduralPreset.h"

namespace ACFProceduralMotionComponent
{
	/** First preset data property on the actor, whatever the customer chose to call it. */
	static UACFProceduralPresetData* FindPresetDataOnOwner(const AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		for (TFieldIterator<FObjectPropertyBase> It(Owner->GetClass()); It; ++It)
		{
			const FObjectPropertyBase* Property = *It;
			if (!Property->PropertyClass || !Property->PropertyClass->IsChildOf(UACFProceduralPresetData::StaticClass()))
			{
				continue;
			}

			if (UACFProceduralPresetData* Value = Cast<UACFProceduralPresetData>(Property->GetObjectPropertyValue_InContainer(Owner)))
			{
				return Value;
			}
		}

		return nullptr;
	}
}

UACFProceduralMotionComponent::UACFProceduralMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UACFProceduralMotionComponent::OnRegister()
{
	Super::OnRegister();

	RefreshInheritedPresetData();
}

void UACFProceduralMotionComponent::RefreshInheritedPresetData()
{
	InheritedPresetData = bInheritPresetDataFromOwner
		? ACFProceduralMotionComponent::FindPresetDataOnOwner(GetOwner())
		: nullptr;
}

bool UACFProceduralMotionComponent::HasRequiredPreset() const
{
	const TSubclassOf<UACFProceduralPreset> PresetClass = GetPresetClass();
	if (!bRequirePreset || !PresetClass)
	{
		return true;
	}

	const UACFProceduralPresetData* EffectivePresetData = GetEffectivePresetData();
	return EffectivePresetData && EffectivePresetData->GetPresetByClass(PresetClass);
}

void UACFProceduralMotionComponent::UpdateOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input)
{
	check(IsInGameThread());

	CachedOps.Reset();

	AnimOwnerCharacter = nullptr;
	if (AnimInstance)
	{
		if (const USkeletalMeshComponent* AnimOwnerComp = AnimInstance->GetOwningComponent())
		{
			AnimOwnerCharacter = AnimOwnerComp->GetOwner<ACharacter>();
		}

		CachedAnimInstance = AnimInstance;
	}

	bAcceptingOutputs = bEnabled
		&& !(!bApplyInFirstPerson && Input.bFirstPerson)
		&& !(!bApplyInThirdPerson && !Input.bFirstPerson)
		&& HasRequiredPreset();

	if (!bAcceptingOutputs)
	{
		return;
	}

	ComputeOutputs(DeltaSeconds, AnimInstance, Input, CachedOps);
}

void UACFProceduralMotionComponent::BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex)
{
}

void UACFProceduralMotionComponent::UpdatePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context)
{
	if (!bAcceptingOutputs)
	{
		return;
	}

	ComputePoseOutputs(DeltaSeconds, Context, CachedOps);
}

void UACFProceduralMotionComponent::ComputePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context, TArray<FACFProceduralMotionOutput>& Outs)
{
}

float UACFProceduralMotionComponent::ReadCurve(const UAnimInstance* AnimInstance, const FName& CurveName, const float DefaultValue)
{
	float OutValue = DefaultValue;
	if (AnimInstance && !CurveName.IsNone())
	{
		AnimInstance->GetCurveValue(CurveName, OutValue);
	}
	return OutValue;
}

void UACFProceduralMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	BP_ComputeOutputs(DeltaSeconds, AnimInstance, Input, Outs);
}

void UACFProceduralMotionComponent::BP_ComputeOutputs_Implementation(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	FACFProceduralMotionOutput Single;
	if (ComputeOutput(DeltaSeconds, AnimInstance, Input, Single))
	{
		Outs.Add(MoveTemp(Single));
	}
}

bool UACFProceduralMotionComponent::ComputeOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out)
{
	return BP_ComputeOutput(DeltaSeconds, AnimInstance, Input, Out);
}
