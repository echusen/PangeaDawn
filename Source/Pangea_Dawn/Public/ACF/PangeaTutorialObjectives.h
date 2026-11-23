#pragma once

#include "CoreMinimal.h"
#include "AQSQuestObjective.h"
#include "GameplayTagContainer.h"
#include "PangeaTutorialObjectives.generated.h"

class APawn;
class UACFAbilitySystemComponent;

/** Movement state the player must hold while travelling. */
UENUM(BlueprintType)
enum class EPangeaTutorialMoveState : uint8
{
    Any,
    Sprinting,
    Crouched
};

/**
 * Base for tutorial objectives that watch the player's pawn on the server.
 * Polls on a short timer from OnObjectiveStarted until the objective ends.
 * Project-owned: subclasses UAQSQuestObjective without modifying the vendor.
 */
UCLASS(Abstract)
class PANGEA_DAWN_API UPangeaTutorialObjectiveBase : public UAQSQuestObjective
{
    GENERATED_BODY()

public:
    /** Player pawn this objective watches; null before start or while unpossessed. */
    UFUNCTION(BlueprintPure, Category = "Pangea|Tutorial")
    APawn* GetTutorialPawn() const;

protected:
    /** Seconds between polls of the player's state. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial", meta = (ClampMin = "0.02", Units = "s"))
    float PollInterval = 0.1f;

    virtual void OnObjectiveStarted_Implementation() override;
    virtual void OnObjectiveCompleted_Implementation() override;
    virtual void OnObjectiveInterrupted_Implementation() override;

    /** Called on every poll while active (server). */
    virtual void PollTutorial(float DeltaSeconds) {}
    /** Called once when watching starts (server). */
    virtual void BeginWatching() {}
    /** Called once when the objective ends for any reason (server). */
    virtual void EndWatching() {}

    /** True when the watched pawn is sprinting (ACF locomotion target state). */
    bool IsPawnSprinting() const;
    /** True when the watched pawn is crouched. */
    bool IsPawnCrouched() const;

    /** Completes the objective at most once per active run. */
    void CompleteOnce();

    bool bTutorialActive = false;

private:
    void HandlePoll();
    void StopWatching();

    FTimerHandle PollTimer;
    double LastPollTime = 0.0;
};

/**
 * Completes after the player covers a horizontal distance while holding a movement state.
 * Tutorial use: WASD move (Any), sprint 30m (Sprinting), sneak 10m (Crouched).
 */
UCLASS(meta = (DisplayName = "Pangea Distance Travelled Objective"))
class PANGEA_DAWN_API UPangeaDistanceTravelledObjective : public UPangeaTutorialObjectiveBase
{
    GENERATED_BODY()

protected:
    /** Horizontal distance to cover, in meters. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial", meta = (ClampMin = "0.1", Units = "m"))
    float RequiredDistanceMeters = 1.0f;

    /** Only distance covered in this state counts. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial")
    EPangeaTutorialMoveState RequiredState = EPangeaTutorialMoveState::Any;

    /** Distance covered so far this run, in meters. */
    UPROPERTY(BlueprintReadOnly, Transient, Category = "Pangea|Tutorial")
    float DistanceTravelledMeters = 0.0f;

    virtual void BeginWatching() override;
    virtual void PollTutorial(float DeltaSeconds) override;

private:
    FVector LastLocation = FVector::ZeroVector;
    bool bHasLastLocation = false;
};

/**
 * Calls CompleteObjective each time the player starts an ACF action matching ActionTags.
 * Set the node's Repetitions to require several (e.g. 3 attacks).
 * Tutorial use: roll while sprinting (Actions.Dodge + bRequireSprinting), attack (Actions.Attack).
 */
UCLASS(meta = (DisplayName = "Pangea Action Performed Objective"))
class PANGEA_DAWN_API UPangeaActionPerformedObjective : public UPangeaTutorialObjectiveBase
{
    GENERATED_BODY()

protected:
    /** Any started action matching one of these tags counts. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial")
    FGameplayTagContainer ActionTags;

    /** Only count the action if the player was sprinting at, or just before, its start. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial")
    bool bRequireSprinting = false;

    /** How long sprinting still counts after it stops, in seconds. */
    UPROPERTY(EditDefaultsOnly, Category = "Pangea|Tutorial", meta = (EditCondition = "bRequireSprinting", ClampMin = "0.0", Units = "s"))
    float SprintGraceSeconds = 0.5f;

    virtual void BeginWatching() override;
    virtual void EndWatching() override;
    virtual void PollTutorial(float DeltaSeconds) override;

    UFUNCTION()
    void HandleActionStarted(FGameplayTag ActionTag);

private:
    void TryBindAbilitySystem();

    TWeakObjectPtr<UACFAbilitySystemComponent> BoundAbilitySystem;
    double LastSprintTime = -1.0;
};

/**
 * Completes when the player wields a main-hand weapon (ACF equipment component).
 * Tutorial use: open inventory and select the weapon to wield it.
 */
UCLASS(meta = (DisplayName = "Pangea Weapon Wielded Objective"))
class PANGEA_DAWN_API UPangeaWeaponWieldedObjective : public UPangeaTutorialObjectiveBase
{
    GENERATED_BODY()

protected:
    virtual void PollTutorial(float DeltaSeconds) override;
};
