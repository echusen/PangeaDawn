// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AMSMapMarkerComponent.h"
#include "AMSMapSubsystem.h"
#include "AMSMarkerWidget.h"
#include "Components/ActorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include <Engine/GameInstance.h>

/*Sets default values for this component's properties*/
UAMSMapMarkerComponent::UAMSMapMarkerComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = WorldWidgetUpdateInterval;
    SetHiddenInGame(true, true);
    bTracked = false;
    MarkerName = FText::FromString("Default Location");
}

// Called when the game starts
void UAMSMapMarkerComponent::BeginPlay()
{
    Super::BeginPlay();

    iconWidget = Cast<UAMSMarkerWidget>(GetWidget());

    if (IsValid(iconWidget)) {
        iconWidget->SetupMarkerIcon(this);
    }
    PrimaryComponentTick.TickInterval = WorldWidgetUpdateInterval;
}

void UAMSMapMarkerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bActivateWorldWidget || !IsValid(iconWidget)) {
        return;
    }

    const APlayerCameraManager* cameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!cameraManager) {
        return;
    }

    const float distance = FVector::Dist(cameraManager->GetCameraLocation(), GetComponentLocation());
    iconWidget->UpdateWorldIcon(distance);
}

void UAMSMapMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsMarked()) {
        RemoveMarker();
    }
    Super::EndPlay(EndPlayReason);
}

void UAMSMapMarkerComponent::AddMarker()
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    if (GameInstance) {
        UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
        if (mapSubsystem) {
            mapSubsystem->RegisterMarker(this);
        }
    }
}

void UAMSMapMarkerComponent::RemoveMarker()
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    if (GameInstance) {
        UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
        if (mapSubsystem) {
            mapSubsystem->RemoveMarker(this);
        }
    }
}

FVector UAMSMapMarkerComponent::GetOwnerLocation() const
{
    return GetOwner()->GetActorLocation();
}

bool UAMSMapMarkerComponent::IsMarked() const
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
    if (mapSubsystem) {
        return mapSubsystem->IsMarkerActive(this);
    }

    return false;
}

FRotator UAMSMapMarkerComponent::GetOwnerRotation() const
{
    return GetOwner()->GetActorRotation();
}

void UAMSMapMarkerComponent::SetTracked(bool val)
{
    bTracked = val;
}

bool UAMSMapMarkerComponent::ShowOnCompass() const
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    if (GameInstance) {
        UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
        if (mapSubsystem) {
            return mapSubsystem->IsMarkerActive(this) && GetTracked();
        }
    }
    return false;
}
