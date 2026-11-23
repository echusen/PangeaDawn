// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/ACFFirstPersonItemTypes.h"
#include "Types/ACFFirstPersonTypes.h"
#include "ACFFirstPersonComponent.generated.h"

class ACharacter;
class UCameraComponent;
class UCurveFloat;
class USceneComponent;
class USkeletalMeshComponent;
class UACFProceduralPresetData;
class UACFProceduralPresetHandOffset;
class UPrimitiveComponent;
class APlayerCameraManager;
class UCameraModifier;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFirstPersonChanged, bool, bIsInFirstPerson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFirstPersonHeldItemChanged, AActor*, HeldItem);

/**
 * Switches the owning character between first and third person, and drives the procedural motion stack from
 * whatever the character holds.
 *
 * First person snaps the camera to the mesh at CameraSocketName, drives it with the control rotation and makes
 * the capsule follow the camera yaw instantly. Third person puts the camera back exactly where it was before
 * entering first person, with its original settings.
 *
 * The ACF movement component listens to OnFirstPersonChanged and locks the rotation mode to Strafing while first
 * person is active, so rotation mode requests made meanwhile are kept and applied again on leaving it.
 *
 * Call SetPerspective or TogglePerspective on the owning client, or on the server for a listen server host.
 * The state replicates to everyone else.
 *
 * Items: ComponentsToAdd are added to the owner when it initializes. The item system hands the held item to
 * SetHeldItem, and the preset data it returns through IACFFirstPersonItemInterface goes to every motion component.
 * The item settings come from that same data: aim points from the aim preset, the collision point from the
 * blocking preset, fire rate and fire mode from the recoil preset.
 *
 * Aiming: SetAiming turns the owner's aiming state into GetAimingAlpha, blended over AimInDuration and
 * AimOutDuration. The ACF movement component calls it whenever its aiming state changes.
 *
 * Head: while a local player looks through this character's first person camera, its owner or a spectator, the
 * head bone is hidden on the visual mesh, and the optional head meshes tagged HeadMeshTag turn on to give the head its
 * shadow back. Derived on each machine from the replicated perspective and the local view target, so nothing extra
 * replicates, and characters nobody here is viewing always keep their head.
 */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="First Person Component"))
class ASCENTFIRSTPERSON_API UACFFirstPersonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFFirstPersonComponent(const FObjectInitializer& ObjectInitializer);

	/** Minimum/maximum head yaw relative to the body, in degrees. The body remains free to turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|View Limits", meta=(ClampMin="-179", ClampMax="179"))
	FVector2D ViewYawLimits = FVector2D(-75.f, 75.f);

	/** Minimum/maximum pitch relative to the horizon, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|View Limits", meta=(ClampMin="-89", ClampMax="89"))
	FVector2D ViewPitchLimits = FVector2D(-60.f, 70.f);

	UFUNCTION(BlueprintCallable, Category="ACF", meta=(DisplayName="Set First Person View Limits", ToolTip="Changes the always-on first-person head limits in degrees. Call on the owning client. Sorts reversed bounds and clamps the current view immediately."))
	void SetFirstPersonViewLimits(float MinYaw, float MaxYaw, float MinPitch, float MaxPitch);

	UFUNCTION(BlueprintPure, Category="ACF", meta=(DisplayName="Clamp First Person View Rotation", ToolTip="Clamps a world rotation against body-relative yaw and horizon-relative pitch. Leaves third-person rotation unchanged."))
	FRotator ClampFirstPersonViewRotation(FRotator ViewRotation) const;

	//~ Begin UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	/** The character's perspective, the same on every machine. Use this one for gameplay. */
	UFUNCTION(BlueprintPure, Category="ACF|First Person", meta=(BlueprintThreadSafe))
	bool IsInFirstPerson() const { return bFirstPerson; }

	/**
	 * True on this machine while a local player looks through this character's first person camera: its owner, or a
	 * spectator viewing it. Always false on a dedicated server and for characters nobody here is viewing, whatever
	 * their perspective. Visual only and different on every machine, so never use it for gameplay.
	 */
	UFUNCTION(BlueprintPure, Category="ACF|First Person", meta=(BlueprintThreadSafe))
	bool IsViewedInFirstPerson() const { return bViewedInFirstPerson; }


	/**
	 * Whether the character's mesh plays its first person animation on this machine: while it is viewed in first
	 * person here, and on a dedicated server whenever it is in first person, so the server's weapon and muzzle stay
	 * where its player sees them. Every other machine shows it in third person. The ACF anim instance follows this.
	 */
	UFUNCTION(BlueprintPure, Category="ACF|First Person", meta=(BlueprintThreadSafe))
	bool IsAnimatedInFirstPerson() const { return bAnimatedInFirstPerson; }

	UFUNCTION(BlueprintCallable, Category="ACF|First Person")
	void TogglePerspective();

	UFUNCTION(BlueprintCallable, Category="ACF|First Person")
	void SetPerspective(bool bInFirstPerson);

	UFUNCTION(BlueprintGetter, Category="ACF|First Person", meta=(DisplayName="Is First Person Enabled", ToolTip="True when entering first person is allowed. Does not indicate the current perspective."))
	bool IsFirstPersonEnabled() const { return bFirstPersonEnabled; }

	UFUNCTION(BlueprintSetter, Category="ACF|First Person", meta=(DisplayName="Set First Person Enabled", ToolTip="Enables or disables first person. Disabling exits it if active; enabling does not enter it automatically. Call on the server or owning client; client requests wait for server replication."))
	void SetFirstPersonEnabled(bool bEnabled);

	/** Overrides the camera this component moves. Unset, the first camera component found on the owner is used. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person")
	void SetCameraComponent(UCameraComponent* InCamera);

	UFUNCTION(BlueprintPure, Category="ACF|First Person")
	UCameraComponent* GetCameraComponent() const { return Camera; }

	/** Components this one added to the owner from ComponentsToAdd. */
	UFUNCTION(BlueprintPure, Category="ACF|First Person|Components")
	TArray<UActorComponent*> GetAddedComponents() const { return ObjectPtrDecay(AddedComponents); }

	/**
	 * The item in the character's hands, or null. Call it on every machine, which the item system already does.
	 * Items that implement IACFFirstPersonItemInterface drive the motion components; anything else just clears them.
	 */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void SetHeldItem(AActor* InItem);

	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item")
	AActor* GetHeldItem() const { return HeldItem.Get(); }

	/** What the motion components currently run on: the held item's preset data, or DefaultPresetData. */
	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item")
	UACFProceduralPresetData* GetActivePresetData() const { return ActivePresetData; }

	/**
	 * Bone the held item is held by: its own override, else ItemReferenceBone. Item points are measured from it, and
	 * the Bone Rebase node's Pivot Source Bone Override pin should read it so the weapon holder copies the same bone.
	 */
	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item", meta=(BlueprintThreadSafe))
	FName GetItemReferenceBone() const { return ActiveItemReferenceBone.IsNone() ? ItemReferenceBone : ActiveItemReferenceBone; }

	/**
	 * The active preset data's Hand Offset preset, so the held item's own when it has one, else zero offsets. Read
	 * live, so tweaking the preset while playing shows at once. Animation graphs take it through the ACF anim
	 * instance's GetHandOffsetData, which is safe from any thread.
	 */
	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item")
	FACFProceduralHandOffsetData GetHandOffsetData() const;

	/** Reads the held item's preset data again, e.g. after its attachments changed. Resets the aim points to the data's own. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void RefreshHeldItem();

	/** Replaces the held item's aim points until the item changes or RefreshHeldItem is called. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void SetAimPoints(const TArray<FACFFirstPersonItemPoint>& InAimPoints, int32 ActiveIndex = 0);

	/** Moves aiming onto another aim point, blending at AimPointInterpSpeed. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void SetActiveAimPoint(int32 Index);

	/** Moves aiming onto the next aim point, wrapping around. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void CycleAimPoint();

	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item")
	int32 GetActiveAimPointIndex() const { return ActiveAimPointIndex; }

	UFUNCTION(BlueprintPure, Category="ACF|First Person|Item")
	int32 GetNumAimPoints() const { return AimPoints.Num(); }

	/** Fires recoil on the owner's recoil components. Call it where the shot is shown, which the shoot action does. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Item")
	void NotifyItemFired();

	/** Starts blending the aiming alpha in or out. The ACF movement component calls it on every aiming change. */
	UFUNCTION(BlueprintCallable, Category="ACF|First Person|Aiming")
	void SetAiming(bool bInAiming);

	UFUNCTION(BlueprintPure, Category="ACF|First Person|Aiming")
	bool IsAiming() const { return bAiming; }

	/** 0 at hip, 1 fully aimed, shaped by AimingCurve. Safe from thread-safe animation graphs. */
	UFUNCTION(BlueprintPure, Category="ACF|First Person|Aiming", meta=(BlueprintThreadSafe))
	float GetAimingAlpha() const { return AimingAlpha; }

public:
	/** Fires on every machine once the new perspective has been applied. */
	UPROPERTY(BlueprintAssignable, Category="ACF|First Person")
	FOnFirstPersonChanged OnFirstPersonChanged;

	/** Mesh socket the camera is snapped to in first person. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person")
	FName CameraSocketName = TEXT("CameraSocket");

	/** Capsule yaw rotation rate while in first person. Negative turns instantly, locking the body to the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person")
	float FirstPersonRotationRateYaw = -1.f;

	/** Fires on this machine only, whenever IsViewedInFirstPerson changes. */
	UPROPERTY(BlueprintAssignable, Category="ACF|First Person")
	FOnFirstPersonChanged OnViewedInFirstPersonChanged;

	/** Hides HeadBoneName on the visual mesh while viewed in first person, and turns the head meshes on for its shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|Head")
	bool bHideHeadInFirstPerson = true;

	/** Hidden along with every bone below it and their shadow, so leader pose followers such as helmets and hair lose theirs too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|Head", meta=(EditCondition="bHideHeadInFirstPerson"))
	FName HeadBoneName = TEXT("head");

	/**
	 * Component tag of the skeletal mesh players see, which ACF tags "Mesh". It must not be the character's animated
	 * mesh: a hidden bone stops animating along with every bone below it, and the camera socket and weapon holder
	 * hang below the head. A visual mesh that retargets from the animated one is safe.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|Head", meta=(EditCondition="bHideHeadInFirstPerson"))
	FName VisualMeshTag = TEXT("Mesh");

	/**
	 * Component tag of optional head only meshes, which give back the shadow the hidden head bone takes along. Off
	 * entirely outside first person, so third person only shows the visual mesh. While viewed in first person they
	 * turn on with Owner No See and Cast Hidden Shadow: the view through this character's camera only gets their
	 * shadow, and any other view, such as a split screen partner, sees them in place of the hidden head.
	 * They must not take their pose from the visual mesh, whose hidden head stops animating. Attach them next to the
	 * visual mesh with the same anim class, so they retarget from the same animated mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ACF|First Person|Head", meta=(EditCondition="bHideHeadInFirstPerson"))
	FName HeadMeshTag = TEXT("HeadMesh");

	/** Fires on every machine when the held item changes, once its config has been applied. */
	UPROPERTY(BlueprintAssignable, Category="ACF|First Person|Item")
	FOnFirstPersonHeldItemChanged OnHeldItemChanged;

	/**
	 * Added to the owner when it initializes, skipping any class it already has a component of. Every motion
	 * component by default; the leaning component can go here too.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ACF|First Person|Components")
	TArray<TSubclassOf<UActorComponent>> ComponentsToAdd;

	/** Preset data while the held item has none, or nothing is held. Null leaves the motion components with none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Item")
	TObjectPtr<UACFProceduralPresetData> DefaultPresetData;

	/**
	 * Motion components only move while the current preset data carries their preset. Off, a missing preset runs
	 * on its class defaults, which is what a component placed on a single weapon expects, not one on the character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Item")
	bool bMotionRequiresPreset = true;

	/**
	 * Bone the weapon holder is rebased onto, which aim points and the collision point are measured from, for every
	 * item that does not name its own (GetFirstPersonItemReferenceBone, e.g. hand_l for a bow). It has to match the
	 * Bone Rebase node's pivot source with no pivot offset, or aiming lands off by the difference: bind that node's
	 * Pivot Source Bone Override pin to GetItemReferenceBone and both follow the held item.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Item")
	FName ItemReferenceBone = TEXT("hand_r");

	/** How fast aiming moves onto a newly selected aim point. 0 snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Item", meta=(ClampMin="0.0"))
	float AimPointInterpSpeed = 12.f;

	/** Seconds from hip to fully aimed. 0 snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Aiming", meta=(ClampMin="0.0", ForceUnits="s"))
	float AimInDuration = 0.2f;

	/** Seconds from fully aimed back to hip. 0 snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Aiming", meta=(ClampMin="0.0", ForceUnits="s"))
	float AimOutDuration = 0.2f;

	/**
	 * Optional. Maps the linear blend progress, 0 to 1, onto the aiming alpha, in both directions. Its output is used
	 * as is, so it can overshoot. Unset, the alpha is the progress itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ACF|First Person|Aiming")
	TObjectPtr<UCurveFloat> AimingCurve;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetPerspective(bool bInFirstPerson);

	/** Replication skips the owner, so a change started on the server reaches the owning client through this. */
	UFUNCTION(Client, Reliable)
	void ClientSetPerspective(bool bInFirstPerson);

	UFUNCTION()
	void OnRep_FirstPerson();

	UFUNCTION(Server, Reliable)
	void ServerSetFirstPersonEnabled(bool bEnabled);

	UFUNCTION()
	void OnRep_FirstPersonEnabled();

	/** Brings the camera and movement in line with bFirstPerson. Does nothing when they already are. */
	void ApplyPerspective();

	void EnterFirstPerson();

	void EnterThirdPerson();

	ACharacter* GetOwnerCharacter() const;

	UCameraComponent* ResolveCamera();

	void AddComponents();

	void ApplyPresetData(UACFProceduralPresetData* InPresetData) const;

	void SetupRecoil() const;

	/** Resolves the active aim point and the collision point, and hands them to the aim and blocking components. */
	void UpdateItemPoints(float DeltaSeconds);

	/** Back to no aim point and no collision point, until the held item's resolve. */
	void ResetItemPoints();

	/** The point relative to the reference bone. False while it cannot be resolved, e.g. before the item mesh loads. */
	bool ResolveItemPoint(const FACFFirstPersonItemPoint& Point, FTransform& OutTransform);

	USceneComponent* ResolveItemPointComponent(const FACFFirstPersonItemPoint& Point) const;

	/** Moves the aiming progress towards the aiming state and reshapes the alpha. */
	void UpdateAiming(float DeltaSeconds);

	/** Ticks while an item is held, while the aiming alpha is still blending, or while it may be viewed in first person. */
	void RefreshTickEnabled();

	/** Re-evaluates IsViewedInFirstPerson and IsAnimatedInFirstPerson, and keeps the head hidden while viewed. */
	void UpdateViewedInFirstPerson();

	/** Whether a local player's view target is the owner. */
	bool IsLocallyViewed() const;

	void SetHeadHidden(bool bHidden);

	/** Hides or shows HeadBoneName on the visual mesh. Returns whether it is hidden now. */
	bool SetHeadBoneHidden(bool bHidden);

	/** Turns the head meshes on as shadow casters that views through this character do not see, or off entirely. */
	void SetHeadMeshesEnabled(bool bEnabled);

	/** The mesh tagged VisualMeshTag, when hiding its head is safe. Warns once otherwise. */
	USkeletalMeshComponent* ResolveVisualMesh();

protected:
	/** Skips the owner, which applies its own switch immediately and must not have it undone by a late update. */
	UPROPERTY(ReplicatedUsing=OnRep_FirstPerson)
	bool bFirstPerson = false;

	/** Replicated to every client, including the owner. Blueprint writes must go through the setter. */
	UPROPERTY(EditAnywhere, BlueprintGetter=IsFirstPersonEnabled, BlueprintSetter=SetFirstPersonEnabled, ReplicatedUsing=OnRep_FirstPersonEnabled, Category="ACF|First Person")
	bool bFirstPersonEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

private:
	void RefreshViewLimitModifier();
	void RemoveViewLimitModifier();
	TWeakObjectPtr<APlayerCameraManager> ViewLimitCameraManager;
	TWeakObjectPtr<UCameraModifier> ViewLimitModifier;

	/** The perspective actually applied on this machine. */
	bool bAppliedFirstPerson = false;

	bool bViewedInFirstPerson = false;


	bool bAnimatedInFirstPerson = false;

	/** The components tagged HeadMeshTag, found on BeginPlay. */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HeadMeshes;

	bool bHeadMeshesEnabled = false;

	/** The mesh whose head bone this component hid, so it is the one shown again. */
	TWeakObjectPtr<USkeletalMeshComponent> HeadHiddenMesh;

	bool bWarnedHeadHiding = false;

	/** Third person camera setup, captured on entering first person and restored on leaving it. */
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> ThirdPersonCameraParent;

	FName ThirdPersonCameraSocket;

	FTransform ThirdPersonCameraRelativeTransform = FTransform::Identity;

	bool bThirdPersonUsePawnControlRotation = false;

	float ThirdPersonRotationRateYaw = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> AddedComponents;

	TWeakObjectPtr<AActor> HeldItem;

	UPROPERTY(Transient)
	TObjectPtr<UACFProceduralPresetData> ActivePresetData;

	/** ActivePresetData's Hand Offset preset, possibly inherited from its parents. */
	UPROPERTY(Transient)
	TObjectPtr<UACFProceduralPresetHandOffset> ActiveHandOffsetPreset;

	TWeakObjectPtr<USceneComponent> HeldItemMesh;

	/** From the blocking preset, or the held item's default. */
	UPROPERTY(Transient)
	FACFFirstPersonItemPoint HeldItemCollisionPoint;

	/** The held item's reference bone override. None until resolved, or when it has none. */
	FName ActiveItemReferenceBone;

	/** The held item could not describe itself yet, so it is asked again every frame. */
	bool bHeldItemPending = false;

	UPROPERTY(Transient)
	TArray<FACFFirstPersonItemPoint> AimPoints;

	int32 ActiveAimPointIndex = 0;

	/** What the aim components currently have, blending towards the active aim point. */
	FTransform AimPointOutput = FTransform::Identity;

	/** Set on an item change, so the new item's first resolved aim point is taken as is instead of blended into. */
	bool bSnapAimPoint = true;

	bool bWarnedMissingReferenceBone = false;

	bool bAiming = false;

	/** Linear blend progress, 0 at hip and 1 fully aimed. */
	float AimingProgress = 0.f;

	/** AimingProgress through AimingCurve. */
	float AimingAlpha = 0.f;
};
