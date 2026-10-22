#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/HuntingTypes.h"
#include "PangeaTrackEmitterComponent.generated.h"

struct FHuntSpecialClueConfig;
class UHuntSpeciesConfig;
class APangeaHuntClueActor;
class UPangeaTrackEmitterComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPangeaTrackSetIdentifiedSignature, UPangeaTrackEmitterComponent*, TrackEmitter, AActor*, Identifier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPangeaTrackTypeIdentifiedSignature, UPangeaTrackEmitterComponent*, TrackEmitter, EHuntClueType, ClueType, AActor*, Identifier);

UCLASS(ClassGroup=(Hunting), meta=(BlueprintSpawnableComponent))
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UPangeaTrackEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaTrackEmitterComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Hunting")
	const TArray<FHuntTrackPoint>& GetTrackPoints() const { return TrackPoints; }

	UFUNCTION(BlueprintPure, Category="Hunting")
	const UHuntSpeciesConfig* GetHuntSpeciesConfig() const { return HuntSpeciesConfig; }

	UFUNCTION(BlueprintPure, Category="Hunting")
	float GetRevealRadius() const;

	UFUNCTION(BlueprintPure, Category="Hunting")
	bool IsTrackSetIdentified() const { return bTrackSetIdentified; }

	UFUNCTION(BlueprintPure, Category="Hunting")
	bool IsClueTypeIdentified(EHuntClueType ClueType) const;

	UFUNCTION(BlueprintCallable, Category="Hunting")
	void MarkTrackSetIdentified(AActor* Identifier);

	UFUNCTION(BlueprintCallable, Category="Hunting")
	void MarkClueTypeIdentified(EHuntClueType ClueType, AActor* Identifier);

	UFUNCTION(BlueprintPure, Category="Hunting")
	bool IsTrackEmissionEnabled() const { return bRuntimeTrackEmissionEnabled; }

	UFUNCTION(BlueprintCallable, Category="Hunting")
	void SetTrackEmissionEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category="Hunting")
	FPangeaTrackSetIdentifiedSignature OnTrackSetIdentified;

	UPROPERTY(BlueprintAssignable, Category="Hunting")
	FPangeaTrackTypeIdentifiedSignature OnTrackTypeIdentified;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Hunting|Config", SaveGame)
	TObjectPtr<UHuntSpeciesConfig> HuntSpeciesConfig = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_TrackPoints, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	TArray<FHuntTrackPoint> TrackPoints;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	FVector LastTrackLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bNextTrackIsLeft = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	float LastSpecialClueTime = -1000.f;

	UPROPERTY(ReplicatedUsing=OnRep_TrackSetIdentified, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bTrackSetIdentified = false;

	UPROPERTY(ReplicatedUsing=OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bFootprintsIdentified = false;

	UPROPERTY(ReplicatedUsing=OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bBloodIdentified = false;

	UPROPERTY(ReplicatedUsing=OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bBrokenFoliageIdentified = false;

	UPROPERTY(ReplicatedUsing=OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bEatenFoodIdentified = false;

	UPROPERTY(ReplicatedUsing=OnRep_TrackIdentificationState, VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bDroppingsIdentified = false;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Hunting|Tracks")
	bool bRuntimeTrackEmissionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunting|Debug")
	bool bEnableDebugMessages = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hunting|Debug", meta=(ClampMin="0.01"))
	float DebugMessageDuration = 2.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Hunting|Debug")
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
