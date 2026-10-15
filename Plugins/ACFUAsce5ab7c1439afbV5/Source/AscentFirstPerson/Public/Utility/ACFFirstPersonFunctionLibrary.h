// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/Skeleton.h"
#include "ACFFirstPersonFunctionLibrary.generated.h"

class APawn;
class UPrimitiveComponent;
class USceneComponent;

UCLASS()
class ASCENTFIRSTPERSON_API UACFFirstPersonFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="ACF|First Person")
	static FRotator GetCameraRotation(const APawn* OwningPawn);

	UFUNCTION(BlueprintPure, Category="ACF|First Person")
	static FRotator GetCameraRelativeRotation(const APawn* OwningPawn);
	
	UFUNCTION(BlueprintCallable, Category="ACF|First Person", meta=(AutoCreateRefTerm="SourceBoneName, TargetBoneName"))
	static void AddOrReplaceVirtualBone(USkeleton* Skeleton, const FName& SourceBoneName, const FName& TargetBoneName, FName VirtualBoneName);

	UFUNCTION(BlueprintCallable, Category="ACF|First Person", meta=(AutoCreateRefTerm="BoneName, RelativeLocation, RelativeRotation"))
	static void AddOrReplaceSocket(USkeleton* Skeleton, FName SocketName, const FName& BoneName, const FVector& RelativeLocation, const FRotator& RelativeRotation);

	UFUNCTION(BlueprintCallable, Category="ACF|First Person")
	static void HandleMeshVisibility(UPrimitiveComponent* InMesh, bool bIsMeshFirstPerson, bool bIsViewFirstPerson);

	/**
	 * Transform of SocketName on SocketOwner, expressed in the space of ReferenceSocketName on ReferenceOwner.
	 *
	 * Both are read in world space, so the result absorbs attachment offsets and each component's own
	 * orientation. While the two stay rigidly attached the result is constant, so it only needs sampling once.
	 * This is how you build the aim point for UACFProceduralAimMotionComponent: equipment sight socket relative
	 * to the character's holder bone.
	 */
	UFUNCTION(BlueprintPure, Category="ACF|First Person", meta=(AutoCreateRefTerm="SocketName, ReferenceSocketName"))
	static FTransform GetSocketTransformRelativeTo(const USceneComponent* SocketOwner, const FName& SocketName, const USceneComponent* ReferenceOwner, const FName& ReferenceSocketName);

#if WITH_EDITOR
	/**
	 * Every curve name defined by any animation in the project, sorted.
	 *
	 * Read from asset registry tags, so nothing is loaded, and cached for a few seconds because the
	 * curve-name dropdowns ask for it on every open.
	 */
	static const TArray<FString>& GetProjectCurveNames();

	/**
	 * Curve names reachable from one skeleton: its own metadata plus every animation that targets it.
	 *
	 * Falls back to the project-wide list when Skeleton is null. An empty result means "no curves found",
	 * which callers validating against it must treat as "could not determine".
	 */
	static TArray<FString> GetCurveNamesForSkeleton(const USkeleton* Skeleton);
#endif
};
