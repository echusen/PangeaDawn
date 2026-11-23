// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ACFMotionImportLibrary.generated.h"

class UAnimSequence;
class UIKRetargeter;
class UIKRigDefinition;
class USkeletalMesh;
struct FReferenceSkeleton;

/** One joint read from a BVH file. */
USTRUCT()
struct FACFMotionJoint
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 Parent = INDEX_NONE;

	/** Rest offset in the BVH file, Y-up. */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	TArray<FString> Channels;

	UPROPERTY()
	int32 ChannelStart = 0;
};

/** A parsed BVH file: the joint hierarchy and every channel value of every frame. */
USTRUCT()
struct FACFMotionClip
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FACFMotionJoint> Joints;

	UPROPERTY()
	TArray<double> Samples;

	UPROPERTY()
	int32 ChannelCount = 0;

	UPROPERTY()
	int32 FrameCount = 0;

	UPROPERTY()
	double FrameTime = 1.0 / 30.0;
};

/** One retarget chain. Bone lists are separated by '|': the first bone present on the skeleton is used. */
USTRUCT()
struct FACFMotionChain
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString SourceStart;

	UPROPERTY()
	FString SourceEnd;

	UPROPERTY()
	FString TargetStart;

	UPROPERTY()
	FString TargetEnd;

	UPROPERTY()
	bool bRequired = false;
};

/** What Import Motion File produced. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLS_API FACFMotionImportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	bool bSuccess = false;

	/** Why the import failed, readable by the user. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	FString Error;

	/** motion_unreadable, mannequin_missing, skeleton_unsupported, create_failed or retarget_failed. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	FString Reason;

	/** The animation retargeted onto Target Mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	TObjectPtr<UAnimSequence> Animation;

	/** The raw motion on the BVH skeleton, kept under <Folder>/Source. */
	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	TObjectPtr<UAnimSequence> SourceAnimation;

	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	TObjectPtr<USkeletalMesh> TargetMesh;

	/** True when the requested mesh was not found and SKM_Manny was used instead. */
	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	bool bUsedFallbackMesh = false;

	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	int32 Frames = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ACF")
	int32 FrameRate = 0;
};

/** Imports body motions made outside Unreal (BVH files, for example from Kimodo) and retargets them onto a character. */
UCLASS()
class ASCENTAITOOLS_API UACFMotionImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Imports a BVH motion and retargets it onto a skeletal mesh, then saves every asset.
	 * The BVH skeleton gets its own skeleton, preview mesh and IK Rig under <Folder>/Rigs, reused on later imports. The raw
	 * motion is kept under <Folder>/Source. The target gets an IK Rig and an IK Retargeter per skeleton under <Folder>/Rigs.
	 * An existing animation with the same name in Folder is replaced.
	 */
	UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Import Motion File", Keywords = "bvh kimodo motion retarget animation import",
		ToolTip = "Imports a BVH file and retargets it onto Target Mesh (empty uses SKM_Manny). Folder empty uses the Yarin content folder /Motions. Editor only."))
	static FACFMotionImportResult ImportMotionFile(const FString& MotionFile, const FString& Folder, const FString& AnimationName, const FString& TargetMesh);

	/** The folder Import Motion File uses when Folder is empty. */
	UFUNCTION(BlueprintPure, Category = "ACF", meta = (DisplayName = "Default Motion Folder", ToolTip = "Content folder for imported motions: the Yarin content folder followed by /Motions."))
	static FString GetDefaultMotionFolder();

private:
	static FACFMotionImportResult MakeFailure(const FString& Reason, const FString& Message);
	static TArray<FACFMotionChain> MotionChains();
	static FVector ToUnrealPosition(const FVector& Position);
	static FQuat ToUnrealRotation(const FQuat& Rotation);
	static bool ParseBvh(const FString& Text, FACFMotionClip& Clip, FString& Error);
	static void JointPose(const FACFMotionClip& Clip, const FACFMotionJoint& Joint, int32 Frame, FVector& OutPosition, FQuat& OutRotation);
	static FName PickBone(const FString& Candidates, const FReferenceSkeleton& Skeleton);
	static bool PackageExists(const FString& PackagePath);
	static UObject* LoadAt(const FString& PackagePath, UClass* Class);
	static FString PackageOf(const UObject* Asset);
	static USkeletalMesh* FindMesh(const FString& NameOrPath);
	static bool SkeletonMatches(const FReferenceSkeleton& Skeleton, const FACFMotionClip& Clip);
	static USkeletalMesh* CreateSourceMesh(const FACFMotionClip& Clip, const FString& SkeletonPath, const FString& MeshPath, FString& Error);
	static USkeletalMesh* EnsureSourceMesh(const FACFMotionClip& Clip, const FString& RigsFolder, FString& Error);
	static UIKRigDefinition* EnsureIKRig(const FString& PackagePath, USkeletalMesh* Mesh, bool bSource, FString& Error);
	static UIKRetargeter* EnsureRetargeter(const FString& PackagePath, UIKRigDefinition* SourceRig, UIKRigDefinition* TargetRig, const USkeletalMesh* TargetMesh);
	static UAnimSequence* BuildSourceAnimation(const FACFMotionClip& Clip, USkeletalMesh* SourceMesh, const FString& PackagePath, FString& Error);
};
