// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CCMTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include <GameFramework/Actor.h>

#include "CCMCameraFaderComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;
class AActor;
class APawn;
class UCCMFadeableActorComponent;

UCLASS(ClassGroup = (CCM), meta = (BlueprintSpawnableComponent))
class CINEMATICCAMERAMANAGER_API UCCMCameraFaderComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UCCMCameraFaderComponent();

    UFUNCTION(BlueprintPure, Category = CCM)
    bool GetOcclusionEnabled() const { return bOcclusionEnabled; }

    UFUNCTION(BlueprintCallable, Category = CCM)
    void SetOcclusionEnabled(bool val) { bOcclusionEnabled = val; }

    UFUNCTION(BlueprintCallable, Category = CCM)
    void AddActorToIgnore(AActor* newActor);

    UFUNCTION(BlueprintCallable, Category = CCM)
    void RemoveActorToIgnore(AActor* newActor);

    UFUNCTION(BlueprintCallable, Category = CCM)
    bool HideActor(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = CCM)
    void ShowActor(AActor* Actor);

    UFUNCTION(BlueprintPure, Category = CCM)
    TArray<AActor*> GetOccludedActors() const { return OccludedActors; }

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CCM)
    UMaterialInterface* FadeMaterial;

    /*Enables or disables occlusion check*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    bool bOcclusionEnabled;

    /*Max distance of occluding actor from the camera*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    float MaxOccludingDistance = 280.f;

    /*Enables or disables occlusion check for player pawn*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    bool bFadePlayer = true;

    /*Distance at which the player gets faded*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    float MaxPlayerFadeDistance = 70.f;

    /*Collisions channels to check for occluders*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

    /*Enable the fader also for actor WITHOUT the fadeable component*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    bool bForceFaderComponent = false;

    /*Component to be forcibly added to fade an occluding actor*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    TSubclassOf<UCCMFadeableActorComponent> FadeableComponentClass;

    /*Shows debug info in editor*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    bool bShowDebug = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CCM)
    TArray<AActor*> IgnoredActors;

public:
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    TObjectPtr<USpringArmComponent> ActiveSpringArm;

    TObjectPtr<UCameraComponent> ActiveCamera;

    TObjectPtr<APlayerController> ActiveController;

    UPROPERTY()
    TArray<AActor*> OccludedActors;

    UFUNCTION()
    void HandlePawnChanges(APawn* newPawn);

    UFUNCTION()
    void CheckOcclusion();

    bool HideOccludedActor(AActor* Actor);
    void ShowOccludedActor(AActor* OccludedActor);

    void ForceShowOccludedActors();

    bool CanOccludeActor(const AActor* Actor) const;

    bool bPlayerOccluded = false;
};
