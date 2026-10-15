// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFFirstPersonComponent.h"

#include "ACFFirstPersonLog.h"
#include "Camera/CameraComponent.h"
#include "Components/ACFProceduralAdditiveMotionComponent.h"
#include "Components/ACFProceduralAimBounceMotionComponent.h"
#include "Components/ACFProceduralAimMotionComponent.h"
#include "Components/ACFProceduralBlockingMotionComponent.h"
#include "Components/ACFProceduralLocomotionMotionComponent.h"
#include "Components/ACFProceduralOffsetMotionComponent.h"
#include "Components/ACFProceduralRecoilMotionComponent.h"
#include "Components/ACFProceduralSwayMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/ACFFirstPersonItemInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Presets/ACFProceduralPreset.h"
#include "Presets/ACFProceduralPresetAimMotion.h"
#include "Presets/ACFProceduralPresetBlockingMotion.h"
#include "Presets/ACFProceduralPresetHandOffset.h"
#include "Presets/ACFProceduralPresetRecoilMotion.h"
#include "Utility/ACFFirstPersonFunctionLibrary.h"

namespace ACFFirstPersonComponent
{
	/**
	 * Whether a component class replicates state or sends RPCs of its own, beyond what every actor component has.
	 * Only those are worth replicating, and they then need a stable name to resolve across the network.
	 */
	static bool ClassNeedsReplication(const UClass* ComponentClass)
	{
		if (const UActorComponent* Defaults = ComponentClass->GetDefaultObject<UActorComponent>(); Defaults && Defaults->GetIsReplicated())
		{
			return true;
		}

		for (TFieldIterator<FProperty> It(ComponentClass); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Net) && It->GetOwnerClass() != UActorComponent::StaticClass())
			{
				return true;
			}
		}

		for (TFieldIterator<UFunction> It(ComponentClass); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_Net) && It->GetOwnerClass() != UActorComponent::StaticClass())
			{
				return true;
			}
		}

		return false;
	}
}

UACFFirstPersonComponent::UACFFirstPersonComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Only ticks while an item is held, to follow its aim point and collision point, or while aiming blends.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Same group as the skeletal mesh, which waits on this tick: a later group would drag the mesh tick along with it.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	bWantsInitializeComponent = true;

	SetIsReplicatedByDefault(true);

	ComponentsToAdd = {
		UACFProceduralOffsetMotionComponent::StaticClass(),
		UACFProceduralAimMotionComponent::StaticClass(),
		UACFProceduralAdditiveMotionComponent::StaticClass(),
		UACFProceduralAimBounceMotionComponent::StaticClass(),
		UACFProceduralLocomotionMotionComponent::StaticClass(),
		UACFProceduralSwayMotionComponent::StaticClass(),
		UACFProceduralRecoilMotionComponent::StaticClass(),
		UACFProceduralBlockingMotionComponent::StaticClass(),
	};
}

void UACFFirstPersonComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AddComponents();
}

void UACFFirstPersonComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ticking ahead of the mesh means the animation reads this frame's aiming alpha and aim point, not the last one.
	if (const ACharacter* Character = GetOwnerCharacter())
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}

	// Applies DefaultPresetData when nothing is held yet, and restarts the tick for an item set before play began.
	RefreshHeldItem();
}

void UACFFirstPersonComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The mesh may outlive this component, and must not keep a hidden head.
	SetHeadHidden(false);

	Super::EndPlay(EndPlayReason);
}

void UACFFirstPersonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Nothing reports view target changes, e.g. a spectator switching who they watch, so it is checked every frame.
	UpdateViewedInFirstPerson();

	UpdateAiming(DeltaTime);

	if (HeldItem.IsStale())
	{
		// Gone without the item system saying so, e.g. destroyed along with the character.
		HeldItem.Reset();
		ResetItemPoints();
		RefreshHeldItem();
		OnHeldItemChanged.Broadcast(nullptr);
	}
	else if (HeldItem.IsValid())
	{
		if (bHeldItemPending)
		{
			RefreshHeldItem();
		}

		UpdateItemPoints(DeltaTime);
	}

	RefreshTickEnabled();
}

void UACFFirstPersonComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UACFFirstPersonComponent, bFirstPerson, COND_SkipOwner);
}

void UACFFirstPersonComponent::TogglePerspective()
{
	SetPerspective(!bFirstPerson);
}

void UACFFirstPersonComponent::SetPerspective(bool bInFirstPerson)
{
	// Applied locally first so the switch is instant for whoever asked for it.
	bFirstPerson = bInFirstPerson;
	ApplyPerspective();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		ServerSetPerspective(bInFirstPerson);
		return;
	}

	// A server-side change on a pawn a remote client controls. Dropped silently when there is no owning client.
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && !Pawn->IsLocallyControlled())
	{
		ClientSetPerspective(bInFirstPerson);
	}
}

void UACFFirstPersonComponent::ServerSetPerspective_Implementation(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	ApplyPerspective();
}

void UACFFirstPersonComponent::ClientSetPerspective_Implementation(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	ApplyPerspective();
}

void UACFFirstPersonComponent::OnRep_FirstPerson()
{
	ApplyPerspective();
}

void UACFFirstPersonComponent::SetCameraComponent(UCameraComponent* InCamera)
{
	// Swapping cameras mid first person would strand the old one on the mesh.
	if (bAppliedFirstPerson && InCamera != Camera)
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: camera changed while in first person. Switch to third person first."), *GetNameSafe(GetOwner()));
		return;
	}

	Camera = InCamera;
}

void UACFFirstPersonComponent::ApplyPerspective()
{
	if (bAppliedFirstPerson == bFirstPerson)
	{
		return;
	}

	bAppliedFirstPerson = bFirstPerson;

	if (bFirstPerson)
	{
		EnterFirstPerson();
	}
	else
	{
		EnterThirdPerson();
	}

	OnFirstPersonChanged.Broadcast(bFirstPerson);

	UpdateViewedInFirstPerson();
	RefreshTickEnabled();
}

void UACFFirstPersonComponent::EnterFirstPerson()
{
	ACharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	if (UCameraComponent* CameraComponent = ResolveCamera())
	{
		// Whatever third person rig the camera is on, it goes back to exactly this.
		ThirdPersonCameraParent = CameraComponent->GetAttachParent();
		ThirdPersonCameraSocket = CameraComponent->GetAttachSocketName();
		ThirdPersonCameraRelativeTransform = CameraComponent->GetRelativeTransform();
		bThirdPersonUsePawnControlRotation = CameraComponent->bUsePawnControlRotation;

		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (!Mesh->DoesSocketExist(CameraSocketName))
			{
				UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: mesh has no %s socket, the first person camera sits at the mesh origin."),
					*GetNameSafe(Character), *CameraSocketName.ToString());
			}

			CameraComponent->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetIncludingScale, CameraSocketName);
		}

		CameraComponent->bUsePawnControlRotation = true;
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		ThirdPersonRotationRateYaw = Movement->RotationRate.Yaw;
		Movement->RotationRate.Yaw = FirstPersonRotationRateYaw;
	}
}

void UACFFirstPersonComponent::EnterThirdPerson()
{
	ACharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return;
	}

	if (UCameraComponent* CameraComponent = ResolveCamera())
	{
		USceneComponent* Parent = ThirdPersonCameraParent.Get();
		FName Socket = ThirdPersonCameraSocket;
		FTransform RelativeTransform = ThirdPersonCameraRelativeTransform;

		// The original parent is gone. The spring arm end is the usual third person spot.
		if (!Parent)
		{
			Parent = Character->FindComponentByClass<USpringArmComponent>();
			Socket = USpringArmComponent::SocketName;
			RelativeTransform = FTransform::Identity;
		}

		if (Parent)
		{
			CameraComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, Socket);
			CameraComponent->SetRelativeTransform(RelativeTransform);
		}

		CameraComponent->bUsePawnControlRotation = bThirdPersonUsePawnControlRotation;
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->RotationRate.Yaw = ThirdPersonRotationRateYaw;
	}
}

ACharacter* UACFFirstPersonComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UCameraComponent* UACFFirstPersonComponent::ResolveCamera()
{
	if (!Camera && GetOwner())
	{
		Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	}

	return Camera;
}

void UACFFirstPersonComponent::AddComponents()
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();

	// Game worlds only, or every Blueprint editor preview would collect a copy of each component.
	if (!Owner || !World || !World->IsGameWorld())
	{
		return;
	}

	for (const TSubclassOf<UActorComponent>& ComponentClass : ComponentsToAdd)
	{
		if (!ComponentClass || ComponentClass->HasAnyClassFlags(CLASS_Abstract) || Owner->FindComponentByClass(ComponentClass))
		{
			continue;
		}

		// Same name on the server and every client, so a replicating component resolves across the network the way
		// one added in the Blueprint does. Runs on every machine during actor construction, before replication.
		const FName ComponentName(*FString::Printf(TEXT("FirstPerson_%s"), *ComponentClass->GetName()));
		UActorComponent* Component = NewObject<UActorComponent>(Owner, ComponentClass, ComponentName);

		if (ACFFirstPersonComponent::ClassNeedsReplication(ComponentClass))
		{
			Component->SetNetAddressable();
			Component->SetIsReplicated(true);
		}

		Component->RegisterComponent();
		Owner->AddInstanceComponent(Component);

		AddedComponents.Add(Component);
	}
}

void UACFFirstPersonComponent::SetHeldItem(AActor* InItem)
{
	if (HeldItem.Get() == InItem)
	{
		return;
	}

	HeldItem = InItem;

	// The previous item's points mean nothing for this one, and its first aim point is taken as is.
	ResetItemPoints();
	bSnapAimPoint = true;

	RefreshHeldItem();

	OnHeldItemChanged.Broadcast(InItem);
}

void UACFFirstPersonComponent::RefreshHeldItem()
{
	AActor* Item = HeldItem.Get();
	const bool bDescribedItem = Item && Item->Implements<UACFFirstPersonItemInterface>();

	UACFProceduralPresetData* ItemPresetData = nullptr;
	FName ItemBoneOverride = NAME_None;
	HeldItemMesh.Reset();
	HeldItemCollisionPoint = FACFFirstPersonItemPoint();

	// Until the item can describe itself the character runs on the defaults, and the tick asks again.
	bHeldItemPending = bDescribedItem && IACFFirstPersonItemInterface::Execute_IsFirstPersonItemPending(Item);
	if (bDescribedItem && !bHeldItemPending)
	{
		ItemPresetData = IACFFirstPersonItemInterface::Execute_GetFirstPersonPresetData(Item);
		HeldItemMesh = IACFFirstPersonItemInterface::Execute_GetFirstPersonItemMesh(Item);
		ItemBoneOverride = IACFFirstPersonItemInterface::Execute_GetFirstPersonItemReferenceBone(Item);
	}

	if (ItemBoneOverride != ActiveItemReferenceBone)
	{
		ActiveItemReferenceBone = ItemBoneOverride;

		// A different bone may well exist where the last one did not.
		bWarnedMissingReferenceBone = false;
	}

	ActivePresetData = ItemPresetData ? ItemPresetData : DefaultPresetData.Get();
	ApplyPresetData(ActivePresetData);

	// Each item setting sits in the preset of the motion it drives, so data without that motion has no setting either.
	const UACFProceduralPresetAimMotion* AimPreset = ActivePresetData ? ActivePresetData->GetPreset<UACFProceduralPresetAimMotion>() : nullptr;
	AimPoints = AimPreset ? AimPreset->AimPoints : TArray<FACFFirstPersonItemPoint>();
	ActiveAimPointIndex = 0;

	ActiveHandOffsetPreset = ActivePresetData ? ActivePresetData->GetPreset<UACFProceduralPresetHandOffset>() : nullptr;

	const UACFProceduralPresetBlockingMotion* BlockingPreset = ActivePresetData ? ActivePresetData->GetPreset<UACFProceduralPresetBlockingMotion>() : nullptr;
	if (BlockingPreset && BlockingPreset->CollisionPoint.IsSet())
	{
		HeldItemCollisionPoint = BlockingPreset->CollisionPoint;
	}
	else if (bDescribedItem && !bHeldItemPending)
	{
		IACFFirstPersonItemInterface::Execute_GetFirstPersonDefaultCollisionPoint(Item, HeldItemCollisionPoint);
	}

	SetupRecoil();

	RefreshTickEnabled();
}

FACFProceduralHandOffsetData UACFFirstPersonComponent::GetHandOffsetData() const
{
	return ActiveHandOffsetPreset ? ActiveHandOffsetPreset->HandOffsetData : FACFProceduralHandOffsetData();
}

void UACFFirstPersonComponent::SetAimPoints(const TArray<FACFFirstPersonItemPoint>& InAimPoints, int32 ActiveIndex)
{
	AimPoints = InAimPoints;
	ActiveAimPointIndex = AimPoints.IsValidIndex(ActiveIndex) ? ActiveIndex : 0;
}

void UACFFirstPersonComponent::SetActiveAimPoint(int32 Index)
{
	if (AimPoints.IsValidIndex(Index))
	{
		ActiveAimPointIndex = Index;
	}
}

void UACFFirstPersonComponent::CycleAimPoint()
{
	if (AimPoints.Num() > 0)
	{
		ActiveAimPointIndex = (ActiveAimPointIndex + 1) % AimPoints.Num();
	}
}

void UACFFirstPersonComponent::NotifyItemFired()
{
	TInlineComponentArray<UACFProceduralRecoilMotionComponent*> RecoilComponents(GetOwner());
	for (UACFProceduralRecoilMotionComponent* Recoil : RecoilComponents)
	{
		Recoil->Fire();
	}
}

void UACFFirstPersonComponent::SetAiming(bool bInAiming)
{
	if (bAiming == bInAiming)
	{
		return;
	}

	bAiming = bInAiming;

	// Applies a zero duration right away instead of a frame later.
	UpdateAiming(0.f);
	RefreshTickEnabled();
}

void UACFFirstPersonComponent::UpdateAiming(float DeltaSeconds)
{
	const float TargetProgress = bAiming ? 1.f : 0.f;
	if (AimingProgress == TargetProgress)
	{
		return;
	}

	// Progress is linear and the curve reads it, so turning around mid blend continues from the same alpha.
	const float Duration = bAiming ? AimInDuration : AimOutDuration;
	AimingProgress = Duration > KINDA_SMALL_NUMBER
		? FMath::FInterpConstantTo(AimingProgress, TargetProgress, DeltaSeconds, 1.f / Duration)
		: TargetProgress;

	AimingAlpha = AimingCurve ? AimingCurve->GetFloatValue(AimingProgress) : AimingProgress;
}

void UACFFirstPersonComponent::RefreshTickEnabled()
{
	const bool bBlendingAim = AimingProgress != (bAiming ? 1.f : 0.f);

	// Only a first person camera can be viewed in first person, and a dedicated server views nothing.
	const bool bMayBeViewedInFirstPerson = bAppliedFirstPerson && GetNetMode() != NM_DedicatedServer;

	SetComponentTickEnabled(HeldItem.IsValid() || bBlendingAim || bMayBeViewedInFirstPerson);
}

void UACFFirstPersonComponent::UpdateViewedInFirstPerson()
{
	const bool bViewed = bAppliedFirstPerson && IsLocallyViewed();
	if (bViewed != bViewedInFirstPerson)
	{
		bViewedInFirstPerson = bViewed;
		SetHeadHidden(bViewedInFirstPerson);
		OnViewedInFirstPersonChanged.Broadcast(bViewedInFirstPerson);
	}
	else if (bViewedInFirstPerson)
	{
		// Swapping the skeletal mesh asset resets bone visibility. Cheap when the head is still hidden.
		SetHeadHidden(true);
	}
}

bool UACFFirstPersonComponent::IsLocallyViewed() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	// Covers the owner looking through its own camera as well as a spectator, local split screen players included.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController && PlayerController->IsLocalController() && PlayerController->GetViewTarget() == Owner)
		{
			return true;
		}
	}

	return false;
}

void UACFFirstPersonComponent::SetHeadHidden(bool bHidden)
{
	USkeletalMeshComponent* HiddenMesh = HeadHiddenMesh.Get();
	USkeletalMeshComponent* VisualMesh = bHidden && bHideHeadInFirstPerson ? (HiddenMesh ? HiddenMesh : ResolveVisualMesh()) : nullptr;

	if (HiddenMesh && HiddenMesh != VisualMesh)
	{
		HiddenMesh->UnHideBoneByName(HeadBoneName);
	}

	HeadHiddenMesh = VisualMesh;

	// Hiding marks the required bones dirty, so it only happens when the head is actually visible.
	if (VisualMesh && !VisualMesh->IsBoneHiddenByName(HeadBoneName))
	{
		// No physics change, so a ragdoll keeps its head body.
		VisualMesh->HideBoneByName(HeadBoneName, PBO_None);
	}
}

USkeletalMeshComponent* UACFFirstPersonComponent::ResolveVisualMesh()
{
	const ACharacter* Character = GetOwnerCharacter();
	if (!Character)
	{
		return nullptr;
	}

	USkeletalMeshComponent* VisualMesh = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(VisualMeshTag))
		{
			VisualMesh = Mesh;
			break;
		}
	}

	FString Problem;
	if (!VisualMesh)
	{
		Problem = FString::Printf(TEXT("no skeletal mesh is tagged %s"), *VisualMeshTag.ToString());
	}
	else if (VisualMesh == Character->GetMesh())
	{
		// Hidden bones leave the required bones, so the head would stop animating along with the camera socket and the weapon holder.
		Problem = FString::Printf(TEXT("%s is the animated mesh, where a hidden head would stop animating along with everything below it"), *VisualMesh->GetName());
	}
	else if (VisualMesh->LeaderPoseComponent.IsValid())
	{
		Problem = FString::Printf(TEXT("%s follows %s through leader pose, which owns its bone visibility"), *VisualMesh->GetName(), *VisualMesh->LeaderPoseComponent->GetName());
	}
	else if (VisualMesh->GetBoneIndex(HeadBoneName) == INDEX_NONE)
	{
		Problem = FString::Printf(TEXT("%s has no %s bone"), *VisualMesh->GetName(), *HeadBoneName.ToString());
	}

	if (!Problem.IsEmpty())
	{
		if (!bWarnedHeadHiding)
		{
			bWarnedHeadHiding = true;
			UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: the head stays visible in first person because %s."), *GetNameSafe(Character), *Problem);
		}
		return nullptr;
	}

	return VisualMesh;
}

void UACFFirstPersonComponent::ApplyPresetData(UACFProceduralPresetData* InPresetData) const
{
	TInlineComponentArray<UACFProceduralMotionComponent*> MotionComponents(GetOwner());
	for (UACFProceduralMotionComponent* Motion : MotionComponents)
	{
		Motion->PresetData = InPresetData;
		Motion->bRequirePreset = bMotionRequiresPreset;
	}
}

void UACFFirstPersonComponent::SetupRecoil() const
{
	// Class defaults without a recoil preset, though recoil then stays still anyway while motion requires a preset.
	const UACFProceduralPresetRecoilMotion* RecoilPreset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetRecoilMotion>(ActivePresetData);

	TInlineComponentArray<UACFProceduralRecoilMotionComponent*> RecoilComponents(GetOwner());
	for (UACFProceduralRecoilMotionComponent* Recoil : RecoilComponents)
	{
		Recoil->Init(RecoilPreset->FireRateRPM);
		Recoil->SetFireMode(RecoilPreset->FireMode);
	}
}

void UACFFirstPersonComponent::UpdateItemPoints(float DeltaSeconds)
{
	AActor* Owner = GetOwner();

	TInlineComponentArray<UACFProceduralAimMotionComponent*> AimComponents(Owner);
	if (AimComponents.Num() > 0 && AimPoints.IsValidIndex(ActiveAimPointIndex))
	{
		FTransform Target;
		if (ResolveItemPoint(AimPoints[ActiveAimPointIndex], Target))
		{
			AimPointOutput = bSnapAimPoint ? Target : UKismetMathLibrary::TInterpTo(AimPointOutput, Target, DeltaSeconds, AimPointInterpSpeed);
			bSnapAimPoint = false;

			// Also hands the point to the recoil components as their aiming pivot.
			for (UACFProceduralAimMotionComponent* Aim : AimComponents)
			{
				Aim->SetAimPointTransform(AimPointOutput);
			}
		}
	}

	TInlineComponentArray<UACFProceduralBlockingMotionComponent*> BlockingComponents(Owner);
	if (BlockingComponents.Num() > 0)
	{
		FTransform CollisionPoint;
		const bool bHasCollisionPoint = HeldItemCollisionPoint.IsSet() && ResolveItemPoint(HeldItemCollisionPoint, CollisionPoint);

		for (UACFProceduralBlockingMotionComponent* Blocking : BlockingComponents)
		{
			if (bHasCollisionPoint)
			{
				Blocking->SetCollisionPoint(CollisionPoint);
			}
			else
			{
				Blocking->ClearCollisionPoint();
			}
		}
	}
}

void UACFFirstPersonComponent::ResetItemPoints()
{
	AActor* Owner = GetOwner();
	AimPointOutput = FTransform::Identity;

	TInlineComponentArray<UACFProceduralAimMotionComponent*> AimComponents(Owner);
	for (UACFProceduralAimMotionComponent* Aim : AimComponents)
	{
		Aim->SetAimPointTransform(FTransform::Identity);
	}

	TInlineComponentArray<UACFProceduralBlockingMotionComponent*> BlockingComponents(Owner);
	for (UACFProceduralBlockingMotionComponent* Blocking : BlockingComponents)
	{
		Blocking->ClearCollisionPoint();
	}
}

bool UACFFirstPersonComponent::ResolveItemPoint(const FACFFirstPersonItemPoint& Point, FTransform& OutTransform)
{
	const ACharacter* Character = GetOwnerCharacter();
	const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	const USceneComponent* PointComponent = ResolveItemPointComponent(Point);
	if (!CharacterMesh || !PointComponent)
	{
		return false;
	}

	const FName ReferenceBone = GetItemReferenceBone();
	if (!CharacterMesh->DoesSocketExist(ReferenceBone))
	{
		if (!bWarnedMissingReferenceBone)
		{
			bWarnedMissingReferenceBone = true;
			UE_LOG(LogACFFirstPerson, Warning, TEXT("%s: mesh has no %s bone, so item aim points and collision points cannot be resolved."),
				*GetNameSafe(Character), *ReferenceBone.ToString());
		}
		return false;
	}

	// Item meshes are often loaded after the item is equipped, so the socket may not exist yet.
	if (!Point.SocketName.IsNone() && !PointComponent->DoesSocketExist(Point.SocketName))
	{
		return false;
	}

	OutTransform = Point.Offset * UACFFirstPersonFunctionLibrary::GetSocketTransformRelativeTo(PointComponent, Point.SocketName, CharacterMesh, ReferenceBone);
	return true;
}

USceneComponent* UACFFirstPersonComponent::ResolveItemPointComponent(const FACFFirstPersonItemPoint& Point) const
{
	if (Point.Component)
	{
		return Point.Component;
	}

	if (Point.ComponentTag.IsNone())
	{
		return HeldItemMesh.Get();
	}

	AActor* Item = HeldItem.Get();
	if (!Item)
	{
		return nullptr;
	}

	// The item itself first, then whatever is attached to it, e.g. a sight attachment actor.
	TArray<AActor*> Actors = { Item };
	Item->GetAttachedActors(Actors, false, true);

	for (const AActor* Actor : Actors)
	{
		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (USceneComponent* Component : Components)
		{
			if (Component->ComponentHasTag(Point.ComponentTag))
			{
				return Component;
			}
		}
	}

	return nullptr;
}
