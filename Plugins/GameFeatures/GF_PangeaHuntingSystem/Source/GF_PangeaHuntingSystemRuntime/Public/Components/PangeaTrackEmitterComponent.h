#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/HuntingTypes.h"
#include "PangeaTrackEmitterComponent.generated.h"

class APangeaHuntClueActor;
class UHuntSpeciesConfig;
class UPangeaTrackEmitterComponent;
struct FHuntSpecialClueConfig;

/** Broadcast when a creature's track set becomes identified. Identifier is null when the change arrived through replication. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPangeaTrackSetIdentifiedSignature, UPangeaTrackEmitterComponent*, TrackEmitter, AActor*, Identifier);

/** Broadcast when one clue type of a creature becomes identified. Identifier is null when the change arrived through replication. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FPangeaTrackTypeIdentifiedSignature, UPangeaTrackEmitterComponent*, TrackEmitter, EHuntClueType, ClueType, AActor*, Identifier);

/**
 * Creature-side half of the hunting system. On the server it records a replicated trail of
 * track points while the owner moves, spawns clue actors for special clues and keeps the
 * replicated state of which clue types have been identified by hunters.
 */
UCLASS(ClassGroup = (Hunting), meta = (BlueprintSpawnableComponent))
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UPangeaTrackEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaTrackEmitterComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Track points currently recorded for the owner, oldest first. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	const TArray<FHuntTrackPoint>& GetTrackPoints() const
	{
		return TrackPoints;
	}

	/** Hunting configuration of the owner's species. Can be null, in which case built-in defaults apply. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	const UHuntSpeciesConfig* GetHuntSpeciesConfig() const
	{
		return HuntSpeciesConfig;
	}

	/** Distance within which a hunter can see this creature's tracks, before hunter bonuses. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetRevealRadius() const;

	/** True once the track set as a whole or any single clue type of this creature has been identified. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	bool IsTrackSetIdentified() const
	{
		return bTrackSetIdentified;
	}

	/** True once the given clue type has been identified. Always false for Scent and Custom. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	bool IsClueTypeIdentified(EHuntClueType ClueType) const;

	/** Marks every clue type that has at least one recorded track point as identified and broadcasts the changes. */
	UFUNCTION(BlueprintCallable, Category = "Hunting")
	void MarkTrackSetIdentified(AActor* Identifier);

	/** Marks one clue type as identified. Blood and Broken Foliage also identify recorded footprints. */
	UFUNCTION(BlueprintCallable, Category = "Hunting")
	void MarkClueTypeIdentified(EHuntClueType ClueType, AActor* Identifier);

	/** Whether the owner is currently allowed to leave new tracks. */
	UFUNCTION(BlueprintPure, Category = "Hunting")
	bool IsTrackEmissionEnabled() const
	{
		return bRuntimeTrackEmissionEnabled;
	}

	/** Turns track emission on or off at runtime. Existing tracks are kept. */
	UFUNCTION(BlueprintCallable, Category = "Hunting")
	void SetTrackEmissionEnabled(bool bEnabled);

	/** Broadcast when the track set becomes identified. */
	UPROPERTY(BlueprintAssignable, Category = "Hunting")
	FPangeaTrackSetIdentifiedSignature OnTrackSetIdentified;

	/** Broadcast when one clue type becomes identified. */
	UPROPERTY(BlueprintAssignable, Category = "Hunting")
	FPangeaTrackTypeIdentifiedSignature OnTrackTypeIdentified;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Hunting configuration of the owner. When unset, BeginPlay takes it from the owner's creature definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Hunting|Config", SaveGame)
	TObjectPtr<UHuntSpeciesConfig> HuntSpeciesConfig = nullptr;

	/** Track points recorded for the owner, oldest first. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackPoints, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	TArray<FHuntTrackPoint> TrackPoints;

	/** Owner location when the last track point was recorded. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	FVector LastTrackLocation = FVector::ZeroVector;

	/** Side of the next track. Flips after every recorded track point. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bNextTrackIsLeft = true;

	/** World time in seconds when the last special clue was recorded. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	float LastSpecialClueTime = -1000.f;

	/** True once the track set as a whole or any single clue type has been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackSetIdentified, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bTrackSetIdentified = false;

	/** True once the footprints have been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bFootprintsIdentified = false;

	/** True once the blood clues have been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bBloodIdentified = false;

	/** True once the broken foliage clues have been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bBrokenFoliageIdentified = false;

	/** True once the eaten food clues have been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bEatenFoodIdentified = false;

	/** True once the droppings clues have been identified. */
	UPROPERTY(ReplicatedUsing = OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bDroppingsIdentified = false;

	/** Runtime switch for leaving new tracks. Changed through SetTrackEmissionEnabled. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bRuntimeTrackEmissionEnabled = true;

	/** Prints the track emitter debug messages on screen. Overwritten at BeginPlay by the species config when one is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Debug")
	bool bEnableDebugMessages = false;

	/** Seconds each debug message stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Debug", meta = (ClampMin = "0.01"))
	float DebugMessageDuration = 2.0f;

	/** World time in seconds of the last skipped-track debug message. Limits those messages to one per second. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hunting|Debug")
	float LastSkipDebugTime = -1000.f;

	UFUNCTION()
	void OnRep_TrackPoints();

	UFUNCTION()
	void OnRep_TrackSetIdentified();

	UFUNCTION()
	void OnRep_TrackIdentificationState();

private:
	static float GetDefaultMinDistanceBetweenTracks();
	static float GetDefaultMinSpeedToLeaveTracks();
	static float GetDefaultTrackLifetime();
	static int32 GetDefaultMaxTrackPoints();
	static float GetDefaultRevealRadius();
	static float GetDefaultTrackZOffset();

	void ResolveConfigFromDefinition();
	void RemoveExpiredTracks(float CurrentTimeSeconds);
	bool ShouldEmitTrack(const FVector& CurrentLocation, float CurrentSpeed) const;
	FHuntTrackPoint BuildTrackPoint(float CurrentTimeSeconds) const;
	EHuntClueType DetermineClueType(float CurrentTimeSeconds) const;
	FString ResolveSourceCreatureName() const;
	void SpawnSpecialClueActor(const FHuntTrackPoint& TrackPoint);
	TArray<TWeakObjectPtr<APangeaHuntClueActor>> SpawnedClues;
	const FHuntSpecialClueConfig* FindAdditionalClueConfig(EHuntClueType ClueType) const;
	float GetTrackLifetimeForClueType(EHuntClueType ClueType) const;
	EHuntClueType DetermineAdditionalClueType(float CurrentTimeSeconds) const;
	bool HasTrackType(EHuntClueType ClueType) const;
	bool SetClueTypeIdentified(EHuntClueType ClueType);
	void DebugWhyTrackWasSkipped(const FVector& CurrentLocation, float CurrentSpeed, float CurrentTimeSeconds);
	void PrintDebugMessage(const FString& Message, const FColor& Color = FColor::Yellow, bool bLogAlso = false) const;
};
