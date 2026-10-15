// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ATSBaseTargetComponent.h"
#include "ATSTargetingFilter.h"
#include "CCMTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include <Components/SphereComponent.h>

#include "ATSTargetingComponent.generated.h"

class APlayerCameraManager;
class APawn;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetingStateChanged, bool, bIsTargeting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged, const class AActor*, targetActor);

UENUM(BlueprintType)
enum class ETargetingType : uint8 {
    EDontLock = 0 UMETA(DisplayName = "Don't Lock Camera"),
    EMagneticLock UMETA(DisplayName = "Adjust Camera Lock with Magnetism"),
    EMagneticLockYawOnly UMETA(DisplayName = "Adjust Camera Yaw Lock On Target"),
    EFrameBothActors UMETA(DisplayName = "Free Flow Style Target"),
};

UENUM(BlueprintType)
enum class ETargetingDirection : uint8 {
    ELeft = 0 UMETA(DisplayName = "Left"),
    ERight UMETA(DisplayName = "Right"),
    EUp UMETA(DisplayName = "Up"),
    EDown UMETA(DisplayName = "Down"),
};

UENUM(BlueprintType)
enum class ETargetSelectionType : uint8 {
    ENearestTarget = 0 UMETA(DisplayName = "Choose Lower Distance "),
    EForwardTarget = 1 UMETA(DisplayName = "Choose Lower Degrees From Forward"),
};

UCLASS(ClassGroup = (ATS), Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENTTARGETINGSYSTEM_API UATSTargetingComponent : public UATSBaseTargetComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UATSTargetingComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /*InputThrashold to start looking for targets with input*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS)
    float InputTrasholdForSearch = .7f;

    /*MaxDistance for target detection*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS)
    float MaxTargetingDistance = 1500;

    /*MaxAngular distance from OwnerActor for target detection*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ATS)
    float MaxAngularDistanceDegree = 60.f;

    /*Select if you prefer to choose the nearest target or
    the one that is closer to your forward vector*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ATS)
    ETargetSelectionType TargetSelectionType;

    /*Select your targeting type:
    Dont Lock = does not affect camera
    Magnetic Lock = smooth camera lock on that interpolates thought the target
    MagneticYawOnly = Affects only yaw for the camera
    Frame Both Actors = no lock, the camera only steps in to keep both on screen*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ATS)
    ETargetingType TargetingType = ETargetingType::EMagneticLock;

    /*How hard the camera works: the strength of the magnetism for the two lock types, and how hard
    the framing type pushes to bring the pair back on screen. Unused by Dont Lock.*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = ATS, meta = (EditCondition = "TargetingType != ETargetingType::EDontLock"))
    float LockMagnetism = 15.f;

    /*Limit on camera pitch to avoid gimbal lock*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS)
    float BottomPitchLimitDegree = -15.f;

    /*Limit on camera pitch to avoid gimbal lock*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS)
    float UpperPitchLimitDegree = 75.f;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = ATS)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectsToQuery;

    /*Filters to avoid an acotr from being targeted*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = ATS)
    TArray<class UATSTargetingFilter*> TargetFilters;

    /*If your target sight to the target is interruptet by something, stop targeting*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS)
    bool bStopTargetingIfOutOfSight = true;

    /*Composition of the "Frame Both Actors" targeting type: how much of the screen the pair may span
    before the camera reacts, arm cap, how far the camera may slide sideways, and the interpolation
    speed. Handed to the camera manager when the framing starts.*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = ATS, meta = (EditCondition = "TargetingType == ETargetingType::EFrameBothActors"))
    FCCMActorFramingSettings ActorFramingSettings;

public:
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category = ATS)
    float GetLockMagnetism() const { return LockMagnetism; }

    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetLockMagnetism(float val) { LockMagnetism = val; }

    UFUNCTION(BlueprintPure, Category = ATS)
    ETargetingType GetTargetingType() const { return TargetingType; }

    /*Changes the camera behaviour of the lock, framing included. Applies immediately, even mid-lock.*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetTargetingType(ETargetingType val);

    /*True while the camera is composing the player and the target together instead of locking.*/
    UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Get Is Framing Both Actors", Keywords = "camera framing composition on screen thirds zoom arm"))
    bool GetIsFramingBothActors() const;

    UFUNCTION(BlueprintPure, Category = ATS)
    float GetMaxAngularDistanceDegree() const { return MaxAngularDistanceDegree; }

    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetMaxAngularDistanceDegree(float val) { MaxAngularDistanceDegree = val; }

    UFUNCTION(BlueprintPure, Category = ATS)
    ETargetSelectionType GetTargetSelectionType() const { return TargetSelectionType; }

    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetTargetSelectionType(ETargetSelectionType val) { TargetSelectionType = val; }

    UFUNCTION(BlueprintPure, Category = ATS)
    float GetMaxTargetingDistance() const { return MaxTargetingDistance; }

    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetMaxTargetingDistance(float val) { MaxTargetingDistance = val; }
    /*Use this to trigger targeting mode*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void TriggerTargeting(bool bActivate);

    /*Used to add Filter for potential targets at runtime*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void AddFilter(TSubclassOf<UATSTargetingFilter> filter);

    /*Used to remove a Filter for potential targets at runtime*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    bool RemoveFilter(TSubclassOf<UATSTargetingFilter> filterClass);

    UFUNCTION(BlueprintCallable, Category = ATS)
    void AddObjectType(TEnumAsByte<EObjectTypeQuery> inTraceChannel);

    UFUNCTION(BlueprintCallable, Category = ATS)
    void RemoveObjectType(TEnumAsByte<EObjectTypeQuery> inTraceChannel);

    /*Use this to toggle targeting mode*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void ToggleTargeting();

    /*Used to search for another target on Left/right*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void RightSearchTargetWithInput(float InputValue);

    /*Used to search for another target  Up/Down*/
    UFUNCTION(BlueprintCallable, Category = ATS)
    void UpSearchTargetWithInput(float InputValue);

    /*Returns true if targeting mode is currently active*/
    UFUNCTION(BlueprintPure, Category = ATS)
    FORCEINLINE bool IsTargetingEnabled() const { return bIsTargeting; }

    /*This component is the lock-on one, so lock-on is active while it holds a target.
    TriggerTargeting keeps bIsTargeting equal to "has a target", and unlike bIsTargeting the
    target is replicated: the server therefore agrees with the owning client, which matters
    because the movement component rotates the body from this.*/
    virtual bool GetIsLockOnActive() const override { return CurrentTarget != nullptr; }

    virtual void SetCurrentTarget(AActor* target) override;

    /** Screen fraction, arm cap, sideways room and interpolation speed of the framing camera mode. */
    UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Get Actor Framing Settings"))
    FCCMActorFramingSettings GetActorFramingSettings() const { return ActorFramingSettings; }

    /** Recomposes the framing camera mode at runtime; reaches the camera right away while targeting. */
    UFUNCTION(BlueprintCallable, Category = ATS, meta = (DisplayName = "Set Actor Framing Settings", AutoCreateRefTerm = "inSettings"))
    void SetActorFramingSettings(const FCCMActorFramingSettings& inSettings);

    UFUNCTION(BlueprintCallable, Category = ATS)
    void SetOwnerReferences();

    UFUNCTION(BlueprintCallable, Category = ATS)
    APawn* GetControlledPawn() const;

    UFUNCTION(BlueprintCallable, Category = ATS)
    APlayerCameraManager* GetCameraManager() const;

    UPROPERTY(BlueprintAssignable, Category = ATS)
    FOnTargetingStateChanged OnTargetingStateChanged;

    UPROPERTY(BlueprintAssignable, Category = ATS)
    FOnTargetChanged OnTargetChanged;

private:
    ELockType GetLockTypeByTargetingType(ETargetingType targetType);

    void PopulatePotentialTargetsArray();

    void SwitchTargetByDirection(ETargetingDirection direction);

    UATSTargetPointComponent* GetNearestTargetPoint(TArray<UATSTargetPointComponent*> points);

    UFUNCTION()
    void ActivateTargeting();

    UFUNCTION()
    TArray<AActor*> GetAllTargetsByDirection(ETargetingDirection direction);

    UFUNCTION()
    bool TrySwitchPointOnCurrentTarget(ETargetingDirection direction);

    UFUNCTION()
    class AActor* GetNearestTarget(TArray<AActor*>& actorFilter);

    UFUNCTION()
    bool IsValidTarget(AActor* target);

    /*False once the target is dead, so neither a new lock nor an existing one can sit on a corpse.
    Actors that don't implement the ACF entity interface can't die, so they always pass.*/
    bool IsTargetAlive(AActor* target) const;

    /*Whether the current target still deserves the lock: alive, not destroyed, in range and,
    when bCheckLineSight is on, in sight.*/
    bool IsCurrentTargetStillValid() const;

    bool IsInFrontOfOwner(AActor* target);

    UFUNCTION()
    void UpdateTargeting(float deltaTime);

    UFUNCTION()
    void UpdateCurrentTargetPoint(UATSTargetPointComponent* inCurrentTargetPoint);

    void CleanCamera();

    /*Tells the camera manager what the current targeting type wants of the camera, and lets it do the
    rest. This is the only place where this component talks to the camera about the target, and the
    manager keeps the lock and the framing mutually exclusive, so they cannot fight over the camera.*/
    void UpdateCameraBehaviour();

    UFUNCTION()
    class UATSTargetPointComponent* GetBestTargetPointForTarget(AActor* target);

    UFUNCTION()
    bool IsRightOfCurrentTarget(FVector locationToCheck);

    UFUNCTION()
    bool IsUpOfCurrentTarget(FVector locationToCheck);

    FName currentCameraEvent = NAME_None;

    UPROPERTY()
    TArray<AActor*> availableTargets;

    bool bCanTarget = true;

    float Xvalue = 0.f;
    float YValue = 0.f;
    float deadband = .3f;

    UPROPERTY()
    bool bIsTargeting = false;
};
