#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "Types/HuntingTypes.h"

#include "PangeaHunterSenseComponent.generated.h"

class AACFCharacter;
class UACFCharacterMovementComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPangeaFootprintFocusWidget;
class UStaticMesh;
class UStaticMeshComponent;

/** Broadcast when the focused footprint target or its focus progress changes. SourceActor is null when the focus is lost. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPangeaFootprintFocusSignature, bool, bHasTarget, float, Progress, AActor*, SourceActor);

/**
 * One entry of the hunter sense visual pool. Ties a reusable mesh component to the track
 * point it currently shows, so components are recycled between scans instead of recreated.
 */
USTRUCT()
struct FPangeaHuntVisualSlot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisualComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance = nullptr;

	FName TrackKey = NAME_None;
	TWeakObjectPtr<AActor> SourceActor;
	EHuntClueType ClueType = EHuntClueType::Custom;
	bool bAssignedThisUpdate = false;
};

/**
 * Player-side half of the hunting system. While the owning character is in hunting sense
 * (aiming and/or crouching) it reveals nearby creature tracks and clue actors for the local
 * player, and lets the player identify footprints by holding the crosshair on them.
 */
UCLASS(ClassGroup = (Hunting), meta = (BlueprintSpawnableComponent))
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UPangeaHunterSenseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaHunterSenseComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Identifies the footprints of the focused creature, or of the creature with the nearest visible
	 * footprint when nothing is focused. Returns false when there is no such creature or, on the
	 * server, when its footprints are already identified. Clients forward the request to the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hunting|Sense")
	bool IdentifyNearestVisibleFootprint();

	/** Fill amount of the footprint focus from 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Hunting|Sense")
	float GetFootprintFocusProgress() const
	{
		return FootprintFocusProgress;
	}

	/** True while the crosshair rests on an unidentified footprint. */
	UFUNCTION(BlueprintPure, Category = "Hunting|Sense")
	bool HasFocusedFootprintTarget() const
	{
		return FocusedFootprintSourceActor.IsValid();
	}

	/** Broadcast when the focused footprint target or its focus progress changes. */
	UPROPERTY(BlueprintAssignable, Category = "Hunting|Sense")
	FPangeaFootprintFocusSignature OnFootprintFocusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** When true, the hunting sense needs aiming and crouching together. When false, either one is enough. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense")
	bool bRequireAimAndCrouch = true;

	/** Seconds between two scans for nearby tracks and clues while the hunting sense is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense", meta = (ClampMin = "0.01"))
	float ScanInterval = 0.1f;

	/** Draws a debug sphere and arrow on every revealed track point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense")
	bool bDebugDrawTracks = false;

	/** Prints the hunter sense debug messages on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Debug")
	bool bEnableDebugMessages = false;

	/** Seconds each debug message stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Debug", meta = (ClampMin = "0.01"))
	float DebugMessageDuration = 2.0f;

	/** Radius of the debug sphere drawn with bDebugDrawTracks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense", meta = (ClampMin = "1.0"))
	float DebugSphereRadius = 18.f;

	/** Seconds the debug shapes drawn with bDebugDrawTracks stay visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense", meta = (ClampMin = "0.01"))
	float DebugDrawDuration = 0.15f;

	/** Shows track points as pooled mesh components. Footprint focus identification needs this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense")
	bool bUsePlaceholderMeshVisuals = true;

	/** Maximum number of track points shown at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense", meta = (ClampMin = "1"))
	int32 MaxVisualPoolSize = 48;

	/** Whether footprints can be identified by focusing them. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	bool bEnableFootprintFocusIdentification = true;

	/** Seconds the crosshair must stay on a footprint to fill the focus ring. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus", meta = (ClampMin = "0.05"))
	float FootprintFocusDuration = 1.25f;

	/** Screen distance in pixels from the crosshair within which a footprint counts as focused. The species config radius applies as well. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus", meta = (ClampMin = "1.0"))
	float FootprintFocusScreenRadius = 48.f;

	/** When true, the footprints are identified as soon as the focus ring is full. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	bool bAutoIdentifyFocusedFootprints = true;

	/** Whether the focus ring widget is shown. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	bool bShowFootprintFocusWidget = true;

	/** Widget class of the focus ring. The native focus widget is used when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	TSubclassOf<UPangeaFootprintFocusWidget> FootprintFocusWidgetClass;

	/** Size of the focus ring widget on the HUD canvas. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	FVector2D FootprintFocusWidgetSize = FVector2D(96.f, 96.f);

	/** Offset in pixels of the focus ring widget from the screen center. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Footprint Focus")
	FVector2D FootprintFocusWidgetScreenOffset = FVector2D::ZeroVector;

	/** ACF notification list widget class. Not read by the hunting C++ code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Messages")
	TSoftClassPtr<UUserWidget> ACFNotificationsListWidgetClass = TSoftClassPtr<UUserWidget>(
		FSoftClassPath(TEXT("/Game/FullSample/Integrations/UIIntegrations/Widgets/ACF_NotificationsList_WB.ACF_NotificationsList_WB_C")));

	/** Fallback on-screen message widget class. Not read by the hunting C++ code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Messages")
	TSoftClassPtr<UUserWidget> FallbackOnScreenMessageWidgetClass =
		TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/FullSample/UI/ACF_OnScreenMessage_WBP.ACF_OnScreenMessage_WBP_C")));

	/** Notification text shown when the player identifies footprints. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Messages")
	FText TracksIdentifiedMessage = FText::FromString(TEXT("Tracks Identified"));

	/** Seconds the identified notification stays on screen. The focused species config replaces this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Messages", meta = (ClampMin = "0.1"))
	float TracksIdentifiedMessageDuration = 3.f;

	/** When true, the owner's RevealRadiusStatTag statistic adds to the reveal radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Stats")
	bool bUseStatModifiedRevealRadius = false;

	/** Statistic read from the owner's ACF statistics component for the reveal radius bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Stats")
	FGameplayTag RevealRadiusStatTag;

	/** Distance always added to the reveal radius of every creature. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Stats")
	float FlatRevealRadiusBonus = 0.f;

	/** Distance added to the reveal radius for each point of the RevealRadiusStatTag statistic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Stats")
	float RevealRadiusPerStatPoint = 50.f;

	/** Upper limit of the combined flat and statistic reveal radius bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting|Sense|Stats", meta = (ClampMin = "0.0"))
	float MaxRevealRadiusBonus = 2500.f;

	UFUNCTION()
	void HandleAimChanged(bool bIsNowAiming);

	UFUNCTION()
	void HandleCrouchChanged(bool bIsNowCrouched);

private:
	void ResolveOwnerState();
	void RefreshSenseState();
	void DrawNearbyTracks();
	void UpdateTrackVisuals();
	void UpdateFootprintFocus(float DeltaTime);
	AActor* FindFocusedFootprintSource() const;
	void ResetFootprintFocus();
	void EnsureFootprintFocusWidget();
	void UpdateFootprintFocusWidget(bool bHasTarget, float Progress);
	void ShowTracksIdentifiedMessage();
	void PrintDebugMessage(const FString& Message, const FColor& Color = FColor::Green, bool bLogAlso = false) const;
	void ResetVisualAssignments();
	UStaticMeshComponent* AcquireVisualSlot(const FName& TrackKey);
	void ReleaseUnusedVisuals();
	FName BuildTrackKey(const AActor* SourceActor, const FHuntTrackPoint& TrackPoint) const;
	UStaticMesh* ResolveFallbackMesh() const;
	UMaterialInterface* ResolveFallbackMaterial() const;
	bool IsLocallyControlledHunter() const;
	float GetModifiedRevealRadius(float BaseRevealRadius) const;

	UFUNCTION(Server, Reliable)
	void ServerIdentifyFootprints(AActor* SourceActor);

	TWeakObjectPtr<AACFCharacter> CachedOwnerCharacter;
	TWeakObjectPtr<UACFCharacterMovementComponent> CachedMovementComponent;
	TWeakObjectPtr<AActor> NearestVisibleFootprintSourceActor;
	TWeakObjectPtr<AActor> FocusedFootprintSourceActor;
	UPROPERTY(Transient)
	TObjectPtr<UPangeaFootprintFocusWidget> FootprintFocusWidget;
	bool bIsAiming = false;
	bool bIsCrouched = false;
	bool bHuntingSenseActive = false;
	bool bFocusedFootprintAlreadyIdentified = false;
	float ScanAccumulator = 0.f;
	float FootprintFocusProgress = 0.f;
	TArray<FPangeaHuntVisualSlot> VisualPool;
};
