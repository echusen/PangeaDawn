// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "AMSCompassWidget.generated.h"

class AActor;
class UAMSMapMarkerComponent;
class UAMSMapSubsystem;
class UAMSMarkerWidget;

/**
 * Base compass widget: owns the marker bookkeeping (gathering already-registered markers on
 * Construct, add/remove on subsystem events, stale-icon pruning). All visuals — icon creation,
 * layout, materials, per-frame rotation/fade — are delegated to Blueprint via CreateCompassIcon
 * and UpdateCompassIcon.
 */
UCLASS()
class ASCENTMAPSSYSTEM_API UAMSCompassWidget : public UUserWidget {
    GENERATED_BODY()

public:
    /** Rebuilds compass icons from the markers already registered in the map subsystem.
    Called automatically on Construct so markers registered before this widget existed
    (e.g. right after a save game load) are not lost. */
    UFUNCTION(BlueprintCallable, Category = AMS)
    void RefreshMarkers();

    /** Adds an icon for the given marker if it passes ShouldAppearInCompass. */
    UFUNCTION(BlueprintCallable, Category = AMS)
    void TryAddMarker(UAMSMapMarkerComponent* markerComp);

    /** Removes the compass icon associated with the given actor, if any. */
    UFUNCTION(BlueprintCallable, Category = AMS)
    void RemoveMarkedActor(AActor* markerOwner);

    /** Removes icons whose marker no longer exists or no longer passes ShouldAppearInCompass. */
    UFUNCTION(BlueprintCallable, Category = AMS)
    void CheckMarkersValidity();

    /** Per-frame update: prunes invalid icons and calls UpdateCompassIcon for each valid one.
    Call this from the Blueprint Tick. */
    UFUNCTION(BlueprintCallable, Category = AMS)
    void UpdateMarkers();

    /** Returns the icon widget currently displayed for the given actor, if any. */
    UFUNCTION(BlueprintPure, Category = AMS)
    UAMSMarkerWidget* GetIconForActor(AActor* markerOwner) const;

    /** Returns all actors currently shown on the compass. */
    UFUNCTION(BlueprintPure, Category = AMS)
    TArray<AActor*> GetMarkedActors() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Whether the marker should be displayed on the compass. Native default checks the marker's
    ShowOnCompass flag; override in Blueprint to add category filters from the compass settings. */
    UFUNCTION(BlueprintNativeEvent, Category = AMS)
    bool ShouldAppearInCompass(const UAMSMapMarkerComponent* markerComp) const;

    /** Creates the icon widget for a marker and adds it to the compass panel.
    Implement in Blueprint: visuals, layout and exposed-on-spawn parameters live there. */
    UFUNCTION(BlueprintImplementableEvent, Category = AMS)
    UAMSMarkerWidget* CreateCompassIcon(AActor* markerOwner, UAMSMapMarkerComponent* markerComp);

    /** Per-frame visual update of a single icon (translation/rotation/fade). Implement in Blueprint. */
    UFUNCTION(BlueprintImplementableEvent, Category = AMS)
    void UpdateCompassIcon(UAMSMarkerWidget* iconWidget, AActor* markerOwner);

private:
    UFUNCTION()
    void HandleMarkerAdded(UAMSMapMarkerComponent* marker);

    UFUNCTION()
    void HandleMarkerRemoved(UAMSMapMarkerComponent* marker);

    UFUNCTION()
    void HandleTrackedMarkerChanged(UAMSMapMarkerComponent* marker);

    void AddMarkedActor(AActor* markerOwner, UAMSMapMarkerComponent* markerComp);

    void RemoveIcon(AActor* key);

    UPROPERTY(Transient)
    TMap<TObjectPtr<AActor>, TObjectPtr<UAMSMarkerWidget>> MarkerWidgets;

    /*The exact component each icon was built from. World Partition can replace a component
    while its owner actor is still valid, so actor validity alone cannot protect Blueprint ticks.*/
    UPROPERTY(Transient)
    TMap<TObjectPtr<AActor>, TWeakObjectPtr<UAMSMapMarkerComponent>> MarkerComponents;

    UAMSMapSubsystem* GetMapSubsystem() const;
};
