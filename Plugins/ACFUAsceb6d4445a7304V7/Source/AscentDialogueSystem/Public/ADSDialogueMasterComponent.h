// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ADSCameraConfigDataAsset.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include <CineCameraActor.h>
#include <LevelSequence.h>
#include <GameFramework/PlayerController.h>
#include "ADSBaseDialoguePartecipantComponent.h"

#include "ADSDialogueMasterComponent.generated.h"

class APlayerController;
class ULevelSequence;
class ACineCameraActor;
class UADSWorldDialogue;

UCLASS(ClassGroup = (ATS), meta = (BlueprintSpawnableComponent))
class ASCENTDIALOGUESYSTEM_API UADSDialogueMasterComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UADSDialogueMasterComponent();

    /** Adds or updates a dialogue variable with the given key and value
     * @param key The name of the variable
     * @param value The value to assign to the variable
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void AddDialogueVariable(FString key, FString value);

    /** Removes a dialogue variable by key
     * @param key The name of the variable to remove
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    void RemoveDialogueVariable(FString key);

    /** Retrieves the value of a dialogue variable
     * @param key The name of the variable to retrieve
     * @param outValue The output variable that will contain the value if found
     * @return True if the variable was found, false otherwise
     */
    UFUNCTION(BlueprintPure, Category = ADS)
    bool GetDialogueVariable(FString key, FString& outValue) const;

    /** Replaces all variables in the given text with their stored values
     * @param inText The input text that may contain variables to replace
     * @return The processed text with variables replaced by their values
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    FText ReplaceVariablesInText(const FText& inText);

    /** Moves the controlled player to the specified position in the world
     * @param finalPoint The destination transform to move the player to
     */
    UFUNCTION(Server, Reliable, Category = ADS)
    void MoveControlledPlayerToPosition(const FTransform& finalPoint);

    /** Starts a world dialogue on this player only. Called by the server on the owning client. */
    UFUNCTION(Client, Reliable)
    void ClientPlayWorldDialogue(UADSWorldDialogue* dialogue, const TArray<UADSBaseDialoguePartecipantComponent*>& participants);

    /**
     * Starts a world dialogue on this machine only, without any networking.
     * Use Play Replicated World Dialogue on the participant to reach every nearby player.
     */
    UFUNCTION(BlueprintCallable, Category = ADS, meta = (DisplayName = "Play World Dialogue Locally"))
    bool PlayWorldDialogueLocally(UADSWorldDialogue* dialogue, const TArray<UADSBaseDialoguePartecipantComponent*>& participants);

    UFUNCTION(BlueprintCallable, Category = ADS)
    void ApplyCameralShot( UADSCameraConfigDataAsset* ShotAsset, const AActor* Speaker, APlayerController* PlayerController);

    UFUNCTION(BlueprintCallable, Category = ADS)
    void StopCurrentShot();

    void ApplyDialogueCinematic(UADSBaseDialoguePartecipantComponent* Speaker, const FDialogueCinematic& NodeCinematic);
    void EndDialogueCinematic();
    void PlayLevelSequence(ULevelSequence* Sequence, APlayerController* PlayerController);
    void StopLevelSequence();

    TObjectPtr<ACineCameraActor> GetDialogueCamera();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Savegame, Category = ADS)
    TMap<FString, FString> DialogueVariables;

    // Camera class to spawn for dialogues
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ADS|Camera")
    TSubclassOf<ACineCameraActor> DialogueCameraClass = ACineCameraActor::StaticClass();

    UPROPERTY()
    TObjectPtr<ACineCameraActor> DialogueCamera;

    UPROPERTY()
    TObjectPtr<class ALevelSequenceActor> SequenceActor;

    UPROPERTY()
    TObjectPtr<class ULevelSequencePlayer> SequencePlayer;

    UPROPERTY()
    TObjectPtr<class UADSCameraConfigDataAsset> CurrentShot;

    void ConfigureCamera(ACineCameraActor* Camera, const UADSCameraConfigDataAsset* ShotAsset, const AActor* Speaker);
};
