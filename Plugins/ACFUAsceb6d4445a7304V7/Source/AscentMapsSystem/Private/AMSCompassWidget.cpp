// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AMSCompassWidget.h"

#include "AMSMapMarkerComponent.h"
#include "AMSMapSubsystem.h"
#include "AMSMarkerWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

void UAMSCompassWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UAMSMapSubsystem* mapSubsystem = GetMapSubsystem();
    if (mapSubsystem) {
        mapSubsystem->OnMapMarkerAdded.AddDynamic(this, &UAMSCompassWidget::HandleMarkerAdded);
        mapSubsystem->OnMapMarkerRemoved.AddDynamic(this, &UAMSCompassWidget::HandleMarkerRemoved);
        mapSubsystem->OnTrackedMarkerChanged.AddDynamic(this, &UAMSCompassWidget::HandleTrackedMarkerChanged);
    }

    // Markers registered before this widget was constructed (e.g. actors loaded from a save game)
    // would never trigger OnMapMarkerAdded, so gather them explicitly.
    RefreshMarkers();
}

void UAMSCompassWidget::NativeDestruct()
{
    UAMSMapSubsystem* mapSubsystem = GetMapSubsystem();
    if (mapSubsystem) {
        mapSubsystem->OnMapMarkerAdded.RemoveDynamic(this, &UAMSCompassWidget::HandleMarkerAdded);
        mapSubsystem->OnMapMarkerRemoved.RemoveDynamic(this, &UAMSCompassWidget::HandleMarkerRemoved);
        mapSubsystem->OnTrackedMarkerChanged.RemoveDynamic(this, &UAMSCompassWidget::HandleTrackedMarkerChanged);
    }

    Super::NativeDestruct();
}

void UAMSCompassWidget::RefreshMarkers()
{
    const UAMSMapSubsystem* mapSubsystem = GetMapSubsystem();
    if (!mapSubsystem) {
        return;
    }

    for (UAMSMapMarkerComponent* marker : mapSubsystem->GetAllMarkers()) {
        TryAddMarker(marker);
    }
}

void UAMSCompassWidget::TryAddMarker(UAMSMapMarkerComponent* markerComp)
{
    if (!IsValid(markerComp)) {
        return;
    }

    if (ShouldAppearInCompass(markerComp)) {
        AddMarkedActor(markerComp->GetOwner(), markerComp);
    }
}

void UAMSCompassWidget::AddMarkedActor(AActor* markerOwner, UAMSMapMarkerComponent* markerComp)
{
    if (!IsValid(markerOwner) || !IsValid(markerComp)) {
        return;
    }

    if (MarkerWidgets.Contains(markerOwner)) {
        const TWeakObjectPtr<UAMSMapMarkerComponent>* ExistingMarker = MarkerComponents.Find(markerOwner);
        if (ExistingMarker && ExistingMarker->Get() == markerComp) {
            return;
        }
        RemoveIcon(markerOwner);
    }

    UAMSMarkerWidget* iconWidget = CreateCompassIcon(markerOwner, markerComp);
    if (iconWidget) {
        MarkerWidgets.Add(markerOwner, iconWidget);
        MarkerComponents.Add(markerOwner, markerComp);
    }
}

void UAMSCompassWidget::RemoveMarkedActor(AActor* markerOwner)
{
    if (MarkerWidgets.Contains(markerOwner)) {
        RemoveIcon(markerOwner);
    }
}

void UAMSCompassWidget::RemoveIcon(AActor* key)
{
    const TObjectPtr<UAMSMarkerWidget>* iconWidget = MarkerWidgets.Find(key);
    if (iconWidget && IsValid(*iconWidget)) {
        (*iconWidget)->RemoveFromParent();
    }
    MarkerWidgets.Remove(key);
    MarkerComponents.Remove(key);
}

void UAMSCompassWidget::CheckMarkersValidity()
{
    TArray<TObjectPtr<AActor>> keys;
    MarkerWidgets.GetKeys(keys);
    for (AActor* key : keys) {
        UAMSMapMarkerComponent* markerComp = IsValid(key) ? key->FindComponentByClass<UAMSMapMarkerComponent>() : nullptr;
        const TWeakObjectPtr<UAMSMapMarkerComponent>* IconMarker = MarkerComponents.Find(key);
        const bool bShouldAppear = IsValid(markerComp) && ShouldAppearInCompass(markerComp);
        if (!bShouldAppear || !IconMarker || IconMarker->Get() != markerComp) {
            RemoveIcon(key);
            if (bShouldAppear) {
                TryAddMarker(markerComp);
            }
        }
    }
}

void UAMSCompassWidget::UpdateMarkers()
{
    TArray<TObjectPtr<AActor>> keys;
    MarkerWidgets.GetKeys(keys);
    for (AActor* key : keys) {
        UAMSMarkerWidget* iconWidget = MarkerWidgets.FindRef(key);
        UAMSMapMarkerComponent* MarkerComp =
            IsValid(key) ? key->FindComponentByClass<UAMSMapMarkerComponent>() : nullptr;
        const TWeakObjectPtr<UAMSMapMarkerComponent>* IconMarker = MarkerComponents.Find(key);
        if (IsValid(key) && IsValid(iconWidget) && IsValid(MarkerComp) && IconMarker
            && IconMarker->Get() == MarkerComp
            && ShouldAppearInCompass(MarkerComp)) {
            UpdateCompassIcon(iconWidget, key);
        } else {
            RemoveIcon(key);
            if (IsValid(MarkerComp)) {
                TryAddMarker(MarkerComp);
            }
        }
    }
}

UAMSMarkerWidget* UAMSCompassWidget::GetIconForActor(AActor* markerOwner) const
{
    return MarkerWidgets.FindRef(markerOwner);
}

TArray<AActor*> UAMSCompassWidget::GetMarkedActors() const
{
    TArray<AActor*> actors;
    for (const auto& pair : MarkerWidgets) {
        actors.Add(pair.Key);
    }
    return actors;
}

bool UAMSCompassWidget::ShouldAppearInCompass_Implementation(const UAMSMapMarkerComponent* markerComp) const
{
    return IsValid(markerComp) && markerComp->ShowOnCompass();
}

void UAMSCompassWidget::HandleMarkerAdded(UAMSMapMarkerComponent* marker)
{
    TryAddMarker(marker);
}

void UAMSCompassWidget::HandleMarkerRemoved(UAMSMapMarkerComponent* marker)
{
    /*EndPlay can mark the component pending-kill before this delegate reaches the compass.
    The UObject still exists for the duration of the broadcast, so use its owner to remove the
    icon, but never pass the stale component back into Blueprint.*/
    if (marker) {
        if (AActor* MarkerOwner = marker->GetOwner()) {
            const TWeakObjectPtr<UAMSMapMarkerComponent>* IconMarker = MarkerComponents.Find(MarkerOwner);
            if (!IconMarker || IconMarker->Get() == marker) {
                RemoveMarkedActor(MarkerOwner);
            }
        }
    }
}

void UAMSCompassWidget::HandleTrackedMarkerChanged(UAMSMapMarkerComponent* marker)
{
    if (IsValid(marker)) {
        TryAddMarker(marker);
    }
}

UAMSMapSubsystem* UAMSCompassWidget::GetMapSubsystem() const
{
    const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    return GameInstance ? GameInstance->GetSubsystem<UAMSMapSubsystem>() : nullptr;
}
