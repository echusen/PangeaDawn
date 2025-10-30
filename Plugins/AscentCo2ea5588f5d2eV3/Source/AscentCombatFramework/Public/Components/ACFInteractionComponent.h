// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <Components/SphereComponent.h>

#include "ACFInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableRegistered, AActor*, interctableActor);

UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTCOMBATFRAMEWORK_API UACFInteractionComponent : public USphereComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFInteractionComponent();

    /* Interacts with the best interactable nearby, calling both the server and client functions */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void Interact(const FString& interactionType = "");

    /* Sets the current best interactable actor */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetCurrentBestInteractable(class AActor* actor);

    /* Retrieves the current best interactable actor */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE class AActor* GetCurrentBestInteractableActor() const { return currentBestInteractableActor; }

    /* Enables or disables interaction detection */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void EnableDetection(bool bIsEnabled);

    /* Adds a collision channel to be considered for interaction */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void AddCollisionChannel(TEnumAsByte<ECollisionChannel> inTraceChannel);

    /* Removes a collision channel from the list of considered interactions */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void RemoveCollisionChannel(TEnumAsByte<ECollisionChannel> inTraceChannel);

    /* Refreshes the list of available interactables */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void RefreshInteractions();

    /* Checks if there is a valid interactable available */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool HasValidInteractable() const;

    /* Event triggered when an interactable is registered */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnInteractableRegistered OnInteractableRegistered;

    /* Event triggered when an interactable is unregistered */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnInteractableRegistered OnInteractableUnregistered;

    /* Event triggered when an interaction is successfully completed */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnInteractableRegistered OnInteractionSucceded;

    UFUNCTION(BlueprintCallable, Category = ACF)
    void RegisterInteractable(AActor* otherActor);

    UFUNCTION(BlueprintCallable, Category = ACF)
    void UnregisterInteractable(AActor* otherActor);

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /*Channels used to check for interactable objects*/
    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = ACF)
    TArray<TEnumAsByte<ECollisionChannel>> CollisionChannels;

    UPROPERTY(EditDefaultsOnly, Category = ACF)
    float InteractableArea = 180.f;

    UPROPERTY(EditDefaultsOnly, Category = ACF)
    bool bAutoEnableOnBeginPlay = false;

public:
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    class APawn* PawnOwner;

    UFUNCTION(Server, Reliable)
    void ServerInteract(const FString& interactionType = "", AActor* bestInteractable = nullptr);

    UFUNCTION(Client, Reliable)
    void LocalInteract(AActor* bestInteractable, const FString& interactionType = "");

    // UPROPERTY()
    class IACFInteractableInterface* currentBestInteractable;

    UPROPERTY(Transient)
    class AActor* currentBestInteractableActor;

    UPROPERTY()
    TArray<class AActor*> interactables;

    UFUNCTION()
    void UpdateInteractionArea();

    void InitChannels();

    UFUNCTION()
    void OnActorEnteredDetector(UPrimitiveComponent* _overlappedComponent,
        AActor* _otherActor, UPrimitiveComponent* _otherComp, int32 _otherBodyIndex,
        bool _bFromSweep, const FHitResult& _SweepResult);

    UFUNCTION()
    void OnActorLeavedDetector(UPrimitiveComponent* _overlappedComponent,
        AActor* _otherActor, UPrimitiveComponent* _otherComp, int32 _otherBodyIndex);

    void Internal_Interact(const FString& interactionType = "");
};
