// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFCCTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ACFCameraAimComponent.generated.h"

class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnACFCameraAimChanged, bool, bIsActive);

/**
 * Publishes the rotation the ACFCameraAim animation node aims the upper body at, sampled once per
 * frame on the Game Thread between the controller and the mesh, so the pose is solved against the
 * rotation of this frame instead of the previous one.
 *
 * It follows the control rotation and nothing else: it neither knows nor cares which camera is
 * currently rendering, so the same setup covers a camera at eye height and an over the shoulder one.
 * Adding it to a pawn is all it takes, and it works on a pawn that is not an ACF character.
 */
UCLASS(ClassGroup = ACF, Blueprintable, meta = (BlueprintSpawnableComponent))
class CHARACTERCONTROLLER_API UACFCameraAimComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UACFCameraAimComponent();

    /**
     * Turns the anchoring on or off. Only has an effect on a locally controlled pawn: the correction
     * is a local view concern and there is no reason to pay for it on replicated characters.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetCameraAimEnabled(bool bInEnabled);

    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE bool IsCameraAimActive() const { return bCameraAimActive; }

    /** Rotation the node spreads over the aim chain. Only meaningful while active. */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE FRotator GetViewRotation() const { return cachedViewRotation; }

    /** Fires on every state change, with the state that was just entered. */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnACFCameraAimChanged OnCameraAimChanged;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Component tag of the mesh the node runs on. Leave None on a character with a single mesh:
     * the character's own is used, and any skeletal mesh on the actor is the last resort.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Tags", meta = (ExposeOnSpawn = "true"))
    FName AnimatedMeshTag;

    /** Bones hidden while active, for when the view sits inside the character's own head. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Mesh")
    TArray<FName> BonesToHideWhenActive;

    /** The body has to face where the view points, otherwise the chain is asked for a yaw it cannot give. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Movement")
    bool bForceStrafingWhenActive = true;

private:
    /** Re-samples the view rotation. Game Thread: it reads the controller. */
    void UpdateViewRotation();

    /**
     * Keeps the tick between the controller and the mesh: the rotation sampled here is the one the
     * controller just produced, and the animation reads it in the same frame instead of the previous one.
     */
    void RefreshTickPrerequisites();

    void ApplyMeshVisibility(bool bInEnabled);
    void ApplyRotationMode(bool bInEnabled);

    USkeletalMeshComponent* ResolveMesh() const;

    UPROPERTY(Transient)
    bool bCameraAimActive = false;

    UPROPERTY(Transient)
    FRotator cachedViewRotation = FRotator::ZeroRotator;

    /** Rotation mode the character had before it was forced to strafe, restored when switching off. */
    ERotationMode previousRotationMode = ERotationMode::EForwardFacing;
    bool bHasStoredRotationMode = false;

    TWeakObjectPtr<class AController> cachedController;
    TWeakObjectPtr<USkeletalMeshComponent> cachedMesh;
};
