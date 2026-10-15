// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Utility/ACFFirstPersonFunctionLibrary.h"

#include "ACFFirstPersonLog.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#if WITH_EDITOR
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ACFFirstPersonFunctionLibrary)

#define LOCTEXT_NAMESPACE "ACFFirstPersonFunctionLibrary"

namespace
{
	void LogMissingBoneWarning(const USkeleton* Skeleton, const FName& BoneName, const FString& FunctionName)
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: Bone %s does not exist on the %s skeleton!"),
			*FunctionName,
			*BoneName.ToString(),
			*Skeleton->GetName());
	}
}

FRotator UACFFirstPersonFunctionLibrary::GetCameraRotation(const APawn* OwningPawn)
{
	if (!IsValid(OwningPawn))
	{
		return FRotator::ZeroRotator;
	}

	constexpr float MaxPitchDegrees = 90.0f;

	const APlayerController* PC = OwningPawn->GetController<APlayerController>();

	const bool bUseViewRotation = PC && PC->IsLocalController();
	const FRotator CameraWorldRot = bUseViewRotation
		? OwningPawn->GetViewRotation()
		: OwningPawn->GetBaseAimRotation();

	FRotator CameraClamped = CameraWorldRot;
	CameraClamped.Pitch = FMath::ClampAngle(CameraClamped.Pitch, -MaxPitchDegrees, MaxPitchDegrees);

	return CameraClamped;
}

FRotator UACFFirstPersonFunctionLibrary::GetCameraRelativeRotation(const APawn* OwningPawn)
{
	if (!IsValid(OwningPawn))
	{
		return FRotator::ZeroRotator;
	}

	return GetCameraRotation(OwningPawn) - OwningPawn->GetActorRotation();
}

void UACFFirstPersonFunctionLibrary::AddOrReplaceVirtualBone(USkeleton* Skeleton, const FName& SourceBoneName, const FName& TargetBoneName, FName VirtualBoneName)
{
	if (!ensure(IsValid(Skeleton)))
	{
		return;
	}

	const FString VirtualBoneString = VirtualBoneName.ToString().TrimStartAndEnd();
	VirtualBoneName = *VirtualBoneString;

	if (VirtualBoneName.IsNone())
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: Virtual bone must have a valid name!"), *FString(__FUNCTION__));
		return;
	}

	if (!VirtualBoneString.StartsWith(TEXTVIEW("VB "), ESearchCase::CaseSensitive))
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: Virtual bone %s must contain the \"VB \" prefix!"), *FString(__FUNCTION__), *VirtualBoneString);
		return;
	}

	const FVirtualBone* ExistingVirtualBone = Skeleton->GetVirtualBones().FindByPredicate([&VirtualBoneName](const FVirtualBone& VirtualBone)
	{
		return VirtualBone.VirtualBoneName == VirtualBoneName;
	});

	if (ExistingVirtualBone != nullptr &&
		ExistingVirtualBone->SourceBoneName == SourceBoneName &&
		ExistingVirtualBone->TargetBoneName == TargetBoneName)
	{
		return;
	}

	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(SourceBoneName) < 0)
	{
		LogMissingBoneWarning(Skeleton, SourceBoneName, __FUNCTION__);
		return;
	}

	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(TargetBoneName) < 0)
	{
		LogMissingBoneWarning(Skeleton, TargetBoneName, __FUNCTION__);
		return;
	}

	if (ExistingVirtualBone != nullptr)
	{
		ExistingVirtualBone = nullptr;

		Skeleton->RemoveVirtualBones({ VirtualBoneName });
	}

	FName TempVirtualBoneName;

	Skeleton->AddNewVirtualBone(SourceBoneName, TargetBoneName, TempVirtualBoneName);
	Skeleton->RenameVirtualBone(TempVirtualBoneName, VirtualBoneName);
}

void UACFFirstPersonFunctionLibrary::AddOrReplaceSocket(USkeleton* Skeleton, FName SocketName, const FName& BoneName, const FVector& RelativeLocation, const FRotator& RelativeRotation)
{
	if (!ensure(IsValid(Skeleton)))
	{
		return;
	}

	SocketName = *SocketName.ToString().TrimStartAndEnd();
	if (SocketName.IsNone())
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: Socket must have a valid name!"), *FString(__FUNCTION__));
		return;
	}

	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) < 0)
	{
		LogMissingBoneWarning(Skeleton, BoneName, __FUNCTION__);
		return;
	}

	USkeletalMeshSocket* ExistingSocket = Skeleton->FindSocket(SocketName);
	if (IsValid(ExistingSocket))
	{
		if (ExistingSocket->BoneName != BoneName ||
			ExistingSocket->RelativeLocation != RelativeLocation ||
			ExistingSocket->RelativeRotation != RelativeRotation ||
			ExistingSocket->RelativeScale != FVector::OneVector ||
			!ExistingSocket->bForceAlwaysAnimated)
		{
			Skeleton->Modify();
			ExistingSocket->Modify();

			ExistingSocket->BoneName = BoneName;
			ExistingSocket->RelativeLocation = RelativeLocation;
			ExistingSocket->RelativeRotation = RelativeRotation;
			ExistingSocket->RelativeScale = FVector::OneVector;
			ExistingSocket->bForceAlwaysAnimated = true;
		}

		return;
	}

	Skeleton->Modify();

	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Skeleton);
	Socket->SocketName = SocketName;
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = RelativeLocation;
	Socket->RelativeRotation = RelativeRotation;

	Skeleton->Sockets.Emplace(Socket);
}

void UACFFirstPersonFunctionLibrary::HandleMeshVisibility(UPrimitiveComponent* InMesh, bool bIsMeshFirstPerson, bool bIsViewFirstPerson)
{
	if (!IsValid(InMesh)) return;
	
	if (bIsViewFirstPerson)
	{
		if (bIsMeshFirstPerson)
		{
			InMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
			InMesh->SetOnlyOwnerSee(true);
			InMesh->SetOwnerNoSee(false);
			InMesh->SetVisibility(true, true);
		}
		else
		{
			InMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
			InMesh->SetOnlyOwnerSee(false);
			InMesh->SetOwnerNoSee(true);
			InMesh->SetVisibility(true, true);
		}
	}
	else
	{
		if (bIsMeshFirstPerson)
		{
			InMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
			InMesh->SetOnlyOwnerSee(false);
			InMesh->SetOwnerNoSee(false);
			InMesh->SetVisibility(false, true);
		}
		else
		{
			InMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;
			InMesh->SetOnlyOwnerSee(false);
			InMesh->SetOwnerNoSee(false);
			InMesh->SetVisibility(true, true);
		}
	}
}

FTransform UACFFirstPersonFunctionLibrary::GetSocketTransformRelativeTo(const USceneComponent* SocketOwner, const FName& SocketName, const USceneComponent* ReferenceOwner, const FName& ReferenceSocketName)
{
	if (!IsValid(SocketOwner) || !IsValid(ReferenceOwner))
	{
		return FTransform::Identity;
	}

	const FTransform SocketWorld = SocketOwner->GetSocketTransform(SocketName, RTS_World);
	const FTransform ReferenceWorld = ReferenceOwner->GetSocketTransform(ReferenceSocketName, RTS_World);

	return SocketWorld.GetRelativeTransform(ReferenceWorld);
}

#if WITH_EDITOR
namespace ACFProceduralCurveNames
{
	/**
	 * Curve names from every animation asset, optionally restricted to one skeleton.
	 *
	 * Everything comes from asset registry tags, so no animation is ever loaded.
	 */
	static void GatherFromAnimations(const USkeleton* Skeleton, TSet<FString>& OutNames)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FARFilter Filter;
		Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Assets;
		AssetRegistryModule.Get().GetAssets(Filter, Assets);

		const FString SkeletonName = Skeleton ? FAssetData(Skeleton).GetExportTextName() : FString();

		for (const FAssetData& Asset : Assets)
		{
			if (Skeleton)
			{
				FString AssetSkeletonName;
				if (!Asset.GetTagValue(TEXT("Skeleton"), AssetSkeletonName) || AssetSkeletonName != SkeletonName)
				{
					continue;
				}
			}

			FString CurveNameList;
			if (!Asset.GetTagValue(USkeleton::CurveNameTag, CurveNameList))
			{
				continue;
			}

			TArray<FString> CurveNames;
			CurveNameList.ParseIntoArray(CurveNames, *USkeleton::CurveTagDelimiter, true);

			for (const FString& CurveName : CurveNames)
			{
				OutNames.Add(CurveName);
			}
		}
	}
}

const TArray<FString>& UACFFirstPersonFunctionLibrary::GetProjectCurveNames()
{
	/** Long enough that opening a dropdown repeatedly is free, short enough to pick up new animations. */
	static constexpr double CacheSeconds = 10.0;

	static TArray<FString> CachedNames;
	static double LastGatherTime = 0.0;

	const double Now = FPlatformTime::Seconds();
	if (CachedNames.Num() > 0 && Now - LastGatherTime < CacheSeconds)
	{
		return CachedNames;
	}

	LastGatherTime = Now;

	TSet<FString> UniqueNames;
	ACFProceduralCurveNames::GatherFromAnimations(nullptr, UniqueNames);

	CachedNames = UniqueNames.Array();
	CachedNames.Sort();

	return CachedNames;
}

TArray<FString> UACFFirstPersonFunctionLibrary::GetCurveNamesForSkeleton(const USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return GetProjectCurveNames();
	}

	TSet<FString> UniqueNames;

	TArray<FName> MetaDataNames;
	Skeleton->GetCurveMetaDataNames(MetaDataNames);
	for (const FName& MetaDataName : MetaDataNames)
	{
		UniqueNames.Add(MetaDataName.ToString());
	}

	ACFProceduralCurveNames::GatherFromAnimations(Skeleton, UniqueNames);

	TArray<FString> Names = UniqueNames.Array();
	Names.Sort();

	return Names;
}
#endif

#undef LOCTEXT_NAMESPACE