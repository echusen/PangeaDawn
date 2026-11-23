// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFMotionImportLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AscentAIToolsSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ReferenceSkeleton.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargetChainMapping.h"
#include "Retargeter/IKRetargetSettings.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigController.h"
#include "StaticToSkeletalMeshConverter.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "ACFMotionImportLibrary"

FString UACFMotionImportLibrary::GetDefaultMotionFolder()
{
	FString Folder = UACFAIToolsSettings::GetGeneratedContentFolder();
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	return (Folder.IsEmpty() ? FString(TEXT("/Game/Yarin")) : Folder) / TEXT("Animations");
}

FACFMotionImportResult UACFMotionImportLibrary::MakeFailure(const FString& Reason, const FString& Message)
{
	FACFMotionImportResult Result;
	Result.Reason = Reason;
	Result.Error = Message;
	return Result;
}

TArray<FACFMotionChain> UACFMotionImportLibrary::MotionChains()
{
	// Kimodo SOMA joints on the source side, UE5 Mannequin bones (UE4 fallbacks) on the target side.
	// Source and target chains share names so the retargeter maps them exactly.
	auto Chain = [](const TCHAR* Name, const TCHAR* SourceStart, const TCHAR* SourceEnd, const TCHAR* TargetStart, const TCHAR* TargetEnd, bool bRequired)
	{
		FACFMotionChain Item;
		Item.Name = FName(Name);
		Item.SourceStart = SourceStart;
		Item.SourceEnd = SourceEnd;
		Item.TargetStart = TargetStart;
		Item.TargetEnd = TargetEnd;
		Item.bRequired = bRequired;
		return Item;
	};
	return {
		Chain(TEXT("Pelvis"), TEXT("Hips"), TEXT("Hips"), TEXT("pelvis"), TEXT("pelvis"), true),
		Chain(TEXT("Spine"), TEXT("Spine1"), TEXT("Chest"), TEXT("spine_01"), TEXT("spine_05|spine_04|spine_03"), true),
		Chain(TEXT("Neck"), TEXT("Neck1"), TEXT("Neck2"), TEXT("neck_01"), TEXT("neck_02|neck_01"), false),
		Chain(TEXT("Head"), TEXT("Head"), TEXT("Head"), TEXT("head"), TEXT("head"), true),
		Chain(TEXT("LeftClavicle"), TEXT("LeftShoulder"), TEXT("LeftShoulder"), TEXT("clavicle_l"), TEXT("clavicle_l"), false),
		Chain(TEXT("RightClavicle"), TEXT("RightShoulder"), TEXT("RightShoulder"), TEXT("clavicle_r"), TEXT("clavicle_r"), false),
		Chain(TEXT("LeftArm"), TEXT("LeftArm"), TEXT("LeftHand"), TEXT("upperarm_l"), TEXT("hand_l"), true),
		Chain(TEXT("RightArm"), TEXT("RightArm"), TEXT("RightHand"), TEXT("upperarm_r"), TEXT("hand_r"), true),
		Chain(TEXT("LeftLeg"), TEXT("LeftLeg"), TEXT("LeftToeBase"), TEXT("thigh_l"), TEXT("ball_l"), true),
		Chain(TEXT("RightLeg"), TEXT("RightLeg"), TEXT("RightToeBase"), TEXT("thigh_r"), TEXT("ball_r"), true),
		Chain(TEXT("LeftThumb"), TEXT("LeftHandThumb1"), TEXT("LeftHandThumb3"), TEXT("thumb_01_l"), TEXT("thumb_03_l"), false),
		Chain(TEXT("LeftIndex"), TEXT("LeftHandIndex1"), TEXT("LeftHandIndex4"), TEXT("index_metacarpal_l|index_01_l"), TEXT("index_03_l"), false),
		Chain(TEXT("LeftMiddle"), TEXT("LeftHandMiddle1"), TEXT("LeftHandMiddle4"), TEXT("middle_metacarpal_l|middle_01_l"), TEXT("middle_03_l"), false),
		Chain(TEXT("LeftRing"), TEXT("LeftHandRing1"), TEXT("LeftHandRing4"), TEXT("ring_metacarpal_l|ring_01_l"), TEXT("ring_03_l"), false),
		Chain(TEXT("LeftPinky"), TEXT("LeftHandPinky1"), TEXT("LeftHandPinky4"), TEXT("pinky_metacarpal_l|pinky_01_l"), TEXT("pinky_03_l"), false),
		Chain(TEXT("RightThumb"), TEXT("RightHandThumb1"), TEXT("RightHandThumb3"), TEXT("thumb_01_r"), TEXT("thumb_03_r"), false),
		Chain(TEXT("RightIndex"), TEXT("RightHandIndex1"), TEXT("RightHandIndex4"), TEXT("index_metacarpal_r|index_01_r"), TEXT("index_03_r"), false),
		Chain(TEXT("RightMiddle"), TEXT("RightHandMiddle1"), TEXT("RightHandMiddle4"), TEXT("middle_metacarpal_r|middle_01_r"), TEXT("middle_03_r"), false),
		Chain(TEXT("RightRing"), TEXT("RightHandRing1"), TEXT("RightHandRing4"), TEXT("ring_metacarpal_r|ring_01_r"), TEXT("ring_03_r"), false),
		Chain(TEXT("RightPinky"), TEXT("RightHandPinky1"), TEXT("RightHandPinky4"), TEXT("pinky_metacarpal_r|pinky_01_r"), TEXT("pinky_03_r"), false)
	};
}

// BVH is Y-up right-handed. Swapping Y and Z is a reflection into Unreal's Z-up left-handed space, so rotations
// are conjugated by the same reflection; converting with a rotation instead would mirror the motion.
FVector UACFMotionImportLibrary::ToUnrealPosition(const FVector& Position)
{
	return FVector(Position.X, Position.Z, Position.Y);
}

FQuat UACFMotionImportLibrary::ToUnrealRotation(const FQuat& Rotation)
{
	return FQuat(-Rotation.X, -Rotation.Z, -Rotation.Y, Rotation.W);
}

bool UACFMotionImportLibrary::ParseBvh(const FString& Text, FACFMotionClip& Clip, FString& Error)
{
	FString Padded = Text.Replace(TEXT("{"), TEXT(" { ")).Replace(TEXT("}"), TEXT(" } "));
	TArray<FString> Tokens;
	Padded.ParseIntoArrayWS(Tokens);

	// Open braces: the joint they belong to, or INDEX_NONE for an End Site.
	TArray<int32> Stack;
	int32 Pending = INDEX_NONE;
	bool bPendingSite = false;
	int32 Index = 0;
	for (; Index < Tokens.Num(); ++Index)
	{
		const FString& Token = Tokens[Index];
		if (Token.Equals(TEXT("MOTION"), ESearchCase::IgnoreCase))
		{
			++Index;
			break;
		}
		if (Token.Equals(TEXT("ROOT"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("JOINT"), ESearchCase::IgnoreCase))
		{
			if (!Tokens.IsValidIndex(Index + 1))
			{
				break;
			}
			FACFMotionJoint Joint;
			Joint.Name = FName(*Tokens[++Index]);
			Joint.Parent = Stack.Num() > 0 ? Stack.Last() : INDEX_NONE;
			if (Clip.Joints.ContainsByPredicate([&Joint](const FACFMotionJoint& Each) { return Each.Name == Joint.Name; }))
			{
				Error = FString::Printf(TEXT("The joint %s appears twice."), *Joint.Name.ToString());
				return false;
			}
			Pending = Clip.Joints.Add(MoveTemp(Joint));
			bPendingSite = false;
			continue;
		}
		if (Token.Equals(TEXT("End"), ESearchCase::IgnoreCase))
		{
			++Index;
			bPendingSite = true;
			continue;
		}
		if (Token == TEXT("{"))
		{
			Stack.Add(bPendingSite ? INDEX_NONE : Pending);
			Pending = INDEX_NONE;
			bPendingSite = false;
			continue;
		}
		if (Token == TEXT("}"))
		{
			if (Stack.Num() > 0)
			{
				Stack.Pop();
			}
			continue;
		}
		const int32 Current = Stack.Num() > 0 ? Stack.Last() : INDEX_NONE;
		if (Token.Equals(TEXT("OFFSET"), ESearchCase::IgnoreCase))
		{
			if (!Tokens.IsValidIndex(Index + 3))
			{
				break;
			}
			const FVector Offset(FCString::Atod(*Tokens[Index + 1]), FCString::Atod(*Tokens[Index + 2]), FCString::Atod(*Tokens[Index + 3]));
			Index += 3;
			if (Current != INDEX_NONE)
			{
				Clip.Joints[Current].Offset = Offset;
			}
			continue;
		}
		if (Token.Equals(TEXT("CHANNELS"), ESearchCase::IgnoreCase))
		{
			if (!Tokens.IsValidIndex(Index + 1))
			{
				break;
			}
			const int32 Count = FCString::Atoi(*Tokens[++Index]);
			if (Count < 0 || !Tokens.IsValidIndex(Index + Count) || Current == INDEX_NONE)
			{
				Error = TEXT("A CHANNELS line is malformed.");
				return false;
			}
			FACFMotionJoint& Joint = Clip.Joints[Current];
			Joint.ChannelStart = Clip.ChannelCount;
			for (int32 Channel = 0; Channel < Count; ++Channel)
			{
				Joint.Channels.Add(Tokens[++Index]);
			}
			Clip.ChannelCount += Count;
			continue;
		}
	}

	for (; Index < Tokens.Num(); ++Index)
	{
		const FString& Token = Tokens[Index];
		if (Token.StartsWith(TEXT("Frames"), ESearchCase::IgnoreCase))
		{
			if (Tokens.IsValidIndex(Index + 1))
			{
				Clip.FrameCount = FCString::Atoi(*Tokens[++Index]);
			}
			continue;
		}
		if (Token.Equals(TEXT("Frame"), ESearchCase::IgnoreCase))
		{
			if (Tokens.IsValidIndex(Index + 2))
			{
				Index += 2;
				Clip.FrameTime = FCString::Atod(*Tokens[Index]);
			}
			continue;
		}
		Clip.Samples.Add(FCString::Atod(*Token));
	}

	if (Clip.Joints.IsEmpty() || Clip.ChannelCount == 0)
	{
		Error = TEXT("The file has no skeleton.");
		return false;
	}
	if (Clip.FrameCount <= 0)
	{
		Clip.FrameCount = Clip.Samples.Num() / Clip.ChannelCount;
	}
	if (Clip.FrameCount <= 0 || Clip.Samples.Num() < Clip.FrameCount * Clip.ChannelCount)
	{
		Error = TEXT("The motion block is incomplete.");
		return false;
	}
	if (Clip.FrameTime <= 0.0)
	{
		Clip.FrameTime = 1.0 / 30.0;
	}
	return true;
}

void UACFMotionImportLibrary::JointPose(const FACFMotionClip& Clip, const FACFMotionJoint& Joint, int32 Frame, FVector& OutPosition, FQuat& OutRotation)
{
	const double* Sample = Clip.Samples.GetData() + Frame * Clip.ChannelCount + Joint.ChannelStart;
	FVector Position = Joint.Offset;
	FQuat Rotation = FQuat::Identity;
	for (int32 Channel = 0; Channel < Joint.Channels.Num(); ++Channel)
	{
		const FString& Name = Joint.Channels[Channel];
		const double Value = Sample[Channel];
		const TCHAR Axis = FChar::ToUpper(Name.Len() > 0 ? Name[0] : TEXT('X'));
		if (Name.Contains(TEXT("position"), ESearchCase::IgnoreCase))
		{
			(Axis == TEXT('X') ? Position.X : Axis == TEXT('Y') ? Position.Y : Position.Z) = Value;
		}
		else if (Name.Contains(TEXT("rotation"), ESearchCase::IgnoreCase))
		{
			const FVector Around = Axis == TEXT('X') ? FVector::XAxisVector : Axis == TEXT('Y') ? FVector::YAxisVector : FVector::ZAxisVector;
			Rotation = Rotation * FQuat(Around, FMath::DegreesToRadians(Value));
		}
	}
	OutPosition = ToUnrealPosition(Position);
	OutRotation = ToUnrealRotation(Rotation).GetNormalized();
}

FName UACFMotionImportLibrary::PickBone(const FString& Candidates, const FReferenceSkeleton& Skeleton)
{
	TArray<FString> Names;
	Candidates.ParseIntoArray(Names, TEXT("|"));
	for (const FString& Name : Names)
	{
		if (Skeleton.FindBoneIndex(FName(*Name)) != INDEX_NONE)
		{
			return FName(*Name);
		}
	}
	return NAME_None;
}

bool UACFMotionImportLibrary::PackageExists(const FString& PackagePath)
{
	return FindPackage(nullptr, *PackagePath) != nullptr || FPackageName::DoesPackageExist(PackagePath);
}

UObject* UACFMotionImportLibrary::LoadAt(const FString& PackagePath, UClass* Class)
{
	if (!PackageExists(PackagePath))
	{
		return nullptr;
	}
	const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	return StaticLoadObject(Class, nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FString UACFMotionImportLibrary::PackageOf(const UObject* Asset)
{
	return Asset ? Asset->GetOutermost()->GetName() : FString();
}

USkeletalMesh* UACFMotionImportLibrary::FindMesh(const FString& NameOrPath)
{
	if (NameOrPath.StartsWith(TEXT("/")))
	{
		FString PackagePath = NameOrPath;
		PackagePath.Split(TEXT("."), &PackagePath, nullptr);
		return Cast<USkeletalMesh>(LoadAt(PackagePath, USkeletalMesh::StaticClass()));
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	auto Search = [&Registry, &NameOrPath]() -> USkeletalMesh*
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Assets, true);
		TArray<FAssetData> Found = Assets.FilterByPredicate([&NameOrPath](const FAssetData& Asset)
		{
			return Asset.AssetName.ToString().Equals(NameOrPath, ESearchCase::IgnoreCase) && Asset.PackagePath.ToString().StartsWith(TEXT("/Game"));
		});
		Found.Sort([](const FAssetData& A, const FAssetData& B)
		{
			const bool bA = A.PackagePath.ToString().Contains(TEXT("Mannequin"));
			const bool bB = B.PackagePath.ToString().Contains(TEXT("Mannequin"));
			return bA != bB ? bA : A.PackageName.ToString() < B.PackageName.ToString();
		});
		return Found.Num() > 0 ? Cast<USkeletalMesh>(Found[0].GetAsset()) : nullptr;
	};
	if (USkeletalMesh* Mesh = Search())
	{
		return Mesh;
	}
	// Mannequin files copied in while the editor is open may not be scanned yet.
	Registry.ScanPathsSynchronous(TArray<FString>{TEXT("/Game/Characters")}, true);
	return Search();
}

bool UACFMotionImportLibrary::SkeletonMatches(const FReferenceSkeleton& Skeleton, const FACFMotionClip& Clip)
{
	if (Skeleton.GetRawBoneNum() != Clip.Joints.Num())
	{
		return false;
	}
	for (int32 Bone = 0; Bone < Clip.Joints.Num(); ++Bone)
	{
		if (Skeleton.GetRawRefBoneInfo()[Bone].Name != Clip.Joints[Bone].Name || Skeleton.GetRawParentIndex(Bone) != Clip.Joints[Bone].Parent)
		{
			return false;
		}
	}
	return true;
}

USkeletalMesh* UACFMotionImportLibrary::CreateSourceMesh(const FACFMotionClip& Clip, const FString& SkeletonPath, const FString& MeshPath, FString& Error)
{
	const EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transactional;
	USkeleton* Skeleton = NewObject<USkeleton>(CreatePackage(*SkeletonPath), *FPackageName::GetShortName(SkeletonPath), Flags);
	{
		FReferenceSkeletonModifier Modifier(Skeleton);
		for (const FACFMotionJoint& Joint : Clip.Joints)
		{
			Modifier.Add(FMeshBoneInfo(Joint.Name, Joint.Name.ToString(), Joint.Parent), FTransform(FQuat::Identity, ToUnrealPosition(Joint.Offset)));
		}
	}
	// Only the bones matter for the IK Rig; the engine cube gives the mesh something to render.
	const UStaticMesh* Shape = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	USkeletalMesh* Mesh = NewObject<USkeletalMesh>(CreatePackage(*MeshPath), *FPackageName::GetShortName(MeshPath), Flags);
	if (!Shape || !FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromStaticMesh(Mesh, Shape, Skeleton->GetReferenceSkeleton(), NAME_None))
	{
		Error = TEXT("The preview mesh for the motion skeleton could not be built.");
		return nullptr;
	}
	Mesh->SetSkeleton(Skeleton);
	Skeleton->MergeAllBonesToBoneTree(Mesh);
	Skeleton->SetPreviewMesh(Mesh);
	FAssetRegistryModule::AssetCreated(Skeleton);
	FAssetRegistryModule::AssetCreated(Mesh);
	Skeleton->MarkPackageDirty();
	Mesh->MarkPackageDirty();
	return Mesh;
}

USkeletalMesh* UACFMotionImportLibrary::EnsureSourceMesh(const FACFMotionClip& Clip, const FString& RigsFolder, FString& Error)
{
	for (int32 Attempt = 1; Attempt <= 20; ++Attempt)
	{
		const FString Suffix = Attempt == 1 ? FString() : FString::Printf(TEXT("_%d"), Attempt);
		const FString SkeletonPath = RigsFolder / (TEXT("SK_MotionSource") + Suffix);
		const FString MeshPath = RigsFolder / (TEXT("SKM_MotionSource") + Suffix);
		if (USkeletalMesh* Existing = Cast<USkeletalMesh>(LoadAt(MeshPath, USkeletalMesh::StaticClass())))
		{
			if (Existing->GetSkeleton() && SkeletonMatches(Existing->GetRefSkeleton(), Clip))
			{
				return Existing;
			}
			continue;
		}
		if (!PackageExists(SkeletonPath) && !PackageExists(MeshPath))
		{
			return CreateSourceMesh(Clip, SkeletonPath, MeshPath, Error);
		}
	}
	Error = TEXT("Too many different motion skeletons in the rigs folder.");
	return nullptr;
}

UIKRigDefinition* UACFMotionImportLibrary::EnsureIKRig(const FString& PackagePath, USkeletalMesh* Mesh, bool bSource, FString& Error)
{
	UIKRigDefinition* Rig = Cast<UIKRigDefinition>(LoadAt(PackagePath, UIKRigDefinition::StaticClass()));
	if (!Rig)
	{
		Rig = NewObject<UIKRigDefinition>(CreatePackage(*PackagePath), *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Rig);
	}
	const UIKRigController* Controller = UIKRigController::GetController(Rig);
	if (!Controller)
	{
		Error = TEXT("The IK Rig could not be edited.");
		return nullptr;
	}
	if (Rig->GetPreviewMesh() != Mesh)
	{
		Controller->SetSkeletalMesh(Mesh);
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const FName Root = PickBone(bSource ? TEXT("Hips") : TEXT("pelvis|Pelvis|hips|Hips"), Skeleton);
	if (Root.IsNone())
	{
		Error = FString::Printf(TEXT("%s has no pelvis bone."), *Mesh->GetName());
		return nullptr;
	}
	Controller->SetRetargetRoot(Root);
	TSet<FName> Present;
	for (const FBoneChain& Existing : Controller->GetRetargetChains())
	{
		Present.Add(Existing.ChainName);
	}
	for (const FACFMotionChain& Chain : MotionChains())
	{
		const FName Start = PickBone(bSource ? Chain.SourceStart : Chain.TargetStart, Skeleton);
		const FName End = PickBone(bSource ? Chain.SourceEnd : Chain.TargetEnd, Skeleton);
		if (Start.IsNone() || End.IsNone())
		{
			if (Chain.bRequired)
			{
				Error = FString::Printf(TEXT("%s has no bones for the %s chain."), *Mesh->GetName(), *Chain.Name.ToString());
				return nullptr;
			}
			continue;
		}
		if (!Present.Contains(Chain.Name))
		{
			Controller->AddRetargetChain(Chain.Name, Start, End, NAME_None);
		}
	}
	Rig->MarkPackageDirty();
	return Rig;
}

UIKRetargeter* UACFMotionImportLibrary::EnsureRetargeter(const FString& PackagePath, UIKRigDefinition* SourceRig, UIKRigDefinition* TargetRig, const USkeletalMesh* TargetMesh)
{
	UIKRetargeter* Retargeter = Cast<UIKRetargeter>(LoadAt(PackagePath, UIKRetargeter::StaticClass()));
	const bool bFresh = Retargeter == nullptr;
	if (bFresh)
	{
		Retargeter = NewObject<UIKRetargeter>(CreatePackage(*PackagePath), *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Retargeter);
	}
	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (!Controller)
	{
		return nullptr;
	}
	if (!bFresh && Controller->GetIKRig(ERetargetSourceOrTarget::Source) == SourceRig && Controller->GetIKRig(ERetargetSourceOrTarget::Target) == TargetRig)
	{
		return Retargeter;
	}
	// Same setup as the editor's automatic retarget: default ops, exact chain mapping, and a target retarget pose
	// auto-aligned to the source rest pose with the feet left flat.
	{
		FScopedReinitializeIKRetargeter Reinitialize(Controller, ERetargetRefreshMode::ProcessorAndOpStack);
		Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
		Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
		Controller->RemoveAllOps();
		Controller->AddDefaultOps();
		Controller->AutoMapChains(EAutoMapChainType::Exact, true);
		const FName Pose = Controller->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target);
		Controller->ResetRetargetPose(Pose, TArray<FName>(), ERetargetSourceOrTarget::Target);
		Controller->AutoAlignAllBones(ERetargetSourceOrTarget::Target);
		TArray<FName> Feet;
		for (const TCHAR* Foot : {TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r")})
		{
			if (TargetMesh->GetRefSkeleton().FindBoneIndex(FName(Foot)) != INDEX_NONE)
			{
				Feet.Add(FName(Foot));
			}
		}
		if (Feet.Num() > 0)
		{
			Controller->ResetRetargetPose(Pose, Feet, ERetargetSourceOrTarget::Target);
		}
	}
	Retargeter->MarkPackageDirty();
	return Retargeter;
}

UAnimSequence* UACFMotionImportLibrary::BuildSourceAnimation(const FACFMotionClip& Clip, USkeletalMesh* SourceMesh, const FString& PackagePath, FString& Error)
{
	USkeleton* Skeleton = SourceMesh->GetSkeleton();
	FString UsePath = PackagePath;
	UAnimSequence* Sequence = Cast<UAnimSequence>(LoadAt(UsePath, UAnimSequence::StaticClass()));
	for (int32 Attempt = 2; Sequence && Sequence->GetSkeleton() != Skeleton; ++Attempt)
	{
		UsePath = FString::Printf(TEXT("%s_%d"), *PackagePath, Attempt);
		Sequence = Cast<UAnimSequence>(LoadAt(UsePath, UAnimSequence::StaticClass()));
	}
	const bool bFresh = Sequence == nullptr;
	if (bFresh)
	{
		Sequence = NewObject<UAnimSequence>(CreatePackage(*UsePath), *FPackageName::GetShortName(UsePath), RF_Public | RF_Standalone | RF_Transactional);
		Sequence->SetSkeleton(Skeleton);
	}
	Sequence->SetPreviewMesh(SourceMesh);

	const int32 FramesPerSecond = FMath::Clamp(FMath::RoundToInt(1.0 / Clip.FrameTime), 1, 240);
	// An Anim Sequence of N frames holds N + 1 keys, so a one-frame clip repeats its only pose.
	const int32 KeyCount = FMath::Max(Clip.FrameCount, 2);
	IAnimationDataController& Controller = Sequence->GetController();
	if (bFresh)
	{
		Controller.InitializeModel();
	}
	{
		IAnimationDataController::FScopedBracket Bracket(Controller, LOCTEXT("ImportMotion", "Import motion"), false);
		if (!bFresh)
		{
			Controller.ResetModel(false);
		}
		Controller.SetFrameRate(FFrameRate(FramesPerSecond, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(KeyCount - 1), false);
		for (const FACFMotionJoint& Joint : Clip.Joints)
		{
			TArray<FVector> Positions;
			TArray<FQuat> Rotations;
			TArray<FVector> Scales;
			Positions.Reserve(KeyCount);
			Rotations.Reserve(KeyCount);
			Scales.Init(FVector::OneVector, KeyCount);
			for (int32 Key = 0; Key < KeyCount; ++Key)
			{
				FVector Position;
				FQuat Rotation;
				JointPose(Clip, Joint, FMath::Min(Key, Clip.FrameCount - 1), Position, Rotation);
				Positions.Add(Position);
				Rotations.Add(Rotation);
			}
			if (!Controller.AddBoneCurve(Joint.Name, false) || !Controller.SetBoneTrackKeys(Joint.Name, Positions, Rotations, Scales, false))
			{
				Error = FString::Printf(TEXT("The joint %s could not be written into the animation."), *Joint.Name.ToString());
				return nullptr;
			}
		}
		Controller.NotifyPopulated();
	}
	if (bFresh)
	{
		FAssetRegistryModule::AssetCreated(Sequence);
	}
	Sequence->MarkPackageDirty();
	return Sequence;
}

FACFMotionImportResult UACFMotionImportLibrary::ImportMotionFile(const FString& MotionFile, const FString& Folder, const FString& AnimationName, const FString& TargetMesh)
{
	FString Text;
	if (MotionFile.IsEmpty() || !FFileHelper::LoadFileToString(Text, *MotionFile))
	{
		return MakeFailure(TEXT("motion_unreadable"), FString::Printf(TEXT("Could not read %s."), *MotionFile));
	}
	FACFMotionClip Clip;
	FString Error;
	if (!ParseBvh(Text, Clip, Error))
	{
		return MakeFailure(TEXT("motion_unreadable"), Error);
	}

	FString Root = Folder.TrimStartAndEnd();
	while (Root.EndsWith(TEXT("/")))
	{
		Root.LeftChopInline(1);
	}
	if (Root.IsEmpty())
	{
		Root = GetDefaultMotionFolder();
	}
	if (!Root.StartsWith(TEXT("/")) || !FPackageName::IsValidLongPackageName(Root / TEXT("Probe")))
	{
		return MakeFailure(TEXT("create_failed"), FString::Printf(TEXT("%s is not a content folder."), *Root));
	}
	FString AssetName = ObjectTools::SanitizeObjectName(AnimationName.TrimStartAndEnd());
	if (AssetName.IsEmpty())
	{
		AssetName = TEXT("Motion");
	}
	const FString RigsFolder = Root / TEXT("Rigs");
	const FString SourceFolder = Root / TEXT("Source");

	const FString DefaultMesh = TEXT("SKM_Manny");
	const FString Wanted = TargetMesh.TrimStartAndEnd().IsEmpty() ? DefaultMesh : TargetMesh.TrimStartAndEnd();
	USkeletalMesh* Target = FindMesh(Wanted);
	bool bFallback = false;
	if (!Target && !Wanted.Equals(DefaultMesh, ESearchCase::IgnoreCase))
	{
		Target = FindMesh(DefaultMesh);
		bFallback = Target != nullptr;
	}
	if (!Target)
	{
		return MakeFailure(TEXT("mannequin_missing"), FString::Printf(TEXT("No skeletal mesh named %s or %s in the project."), *Wanted, *DefaultMesh));
	}
	if (!Target->GetSkeleton())
	{
		return MakeFailure(TEXT("skeleton_unsupported"), FString::Printf(TEXT("%s has no skeleton."), *Target->GetName()));
	}

	USkeletalMesh* SourceMesh = EnsureSourceMesh(Clip, RigsFolder, Error);
	if (!SourceMesh)
	{
		return MakeFailure(TEXT("create_failed"), Error);
	}
	UIKRigDefinition* SourceRig = EnsureIKRig(RigsFolder / (TEXT("IK_") + SourceMesh->GetName().RightChop(4)), SourceMesh, true, Error);
	if (!SourceRig)
	{
		return MakeFailure(TEXT("skeleton_unsupported"), Error);
	}
	const FString TargetSkeletonName = Target->GetSkeleton()->GetName();
	UIKRigDefinition* TargetRig = EnsureIKRig(RigsFolder / FString::Printf(TEXT("IK_%s_Motion"), *TargetSkeletonName), Target, false, Error);
	if (!TargetRig)
	{
		return MakeFailure(TEXT("skeleton_unsupported"), Error);
	}
	UIKRetargeter* Retargeter = EnsureRetargeter(
		RigsFolder / FString::Printf(TEXT("RTG_%s_To_%s"), *SourceMesh->GetName().RightChop(4), *TargetSkeletonName), SourceRig, TargetRig, Target);
	if (!Retargeter)
	{
		return MakeFailure(TEXT("create_failed"), TEXT("The IK Retargeter could not be created."));
	}

	UAnimSequence* Source = BuildSourceAnimation(Clip, SourceMesh, SourceFolder / (AssetName + TEXT("_Source")), Error);
	if (!Source)
	{
		return MakeFailure(TEXT("create_failed"), Error);
	}

	FIKRetargetBatchOperationInputs Inputs;
	Inputs.AssetsToRetarget.Add(FAssetData(Source));
	Inputs.SourceMesh = SourceMesh;
	Inputs.TargetMesh = Target;
	Inputs.IKRetargetAsset = Retargeter;
	Inputs.Search = Source->GetName();
	Inputs.Replace = AssetName;
	Inputs.TargetPath = Root;
	Inputs.bUseSourcePath = false;
	Inputs.bIncludeReferencedAssets = false;
	Inputs.bOverwriteExistingFiles = true;
	const TArray<FAssetData> Results = UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
	UAnimSequence* Retargeted = nullptr;
	for (const FAssetData& Item : Results)
	{
		Retargeted = Cast<UAnimSequence>(Item.GetAsset());
		if (Retargeted)
		{
			break;
		}
	}
	if (!Retargeted)
	{
		return MakeFailure(TEXT("retarget_failed"), FString::Printf(TEXT("Retargeting onto %s produced no animation."), *Target->GetName()));
	}
	Retargeted->SetPreviewMesh(Target);

	const TArray<UObject*> ToSave = {SourceMesh->GetSkeleton(), SourceMesh, SourceRig, TargetRig, Retargeter, Source, Retargeted};
	if (!UEditorAssetLibrary::SaveLoadedAssets(ToSave, false))
	{
		return MakeFailure(TEXT("create_failed"), TEXT("The animation was made but could not be saved."));
	}

	FACFMotionImportResult Result;
	Result.bSuccess = true;
	Result.Animation = Retargeted;
	Result.SourceAnimation = Source;
	Result.TargetMesh = Target;
	Result.bUsedFallbackMesh = bFallback;
	Result.Frames = Clip.FrameCount;
	Result.FrameRate = FMath::Clamp(FMath::RoundToInt(1.0 / Clip.FrameTime), 1, 240);
	return Result;
}

#undef LOCTEXT_NAMESPACE
