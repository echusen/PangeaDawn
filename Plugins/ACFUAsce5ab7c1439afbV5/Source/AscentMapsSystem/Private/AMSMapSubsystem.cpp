// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AMSMapSubsystem.h"
#include "AMSActorMarker.h"
#include "AMSDeveloperSettings.h"
#include "AMSMapArea.h"
#include "AMSMapLocation.h"
#include "AMSMapMarkerComponent.h"
#include "AMSMarkerWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

UAMSMapSubsystem::UAMSMapSubsystem()
{
    bHasMarkerTracked = false;
}

void UAMSMapSubsystem::Deinitialize()
{
    CleanupFastTravel();
    Super::Deinitialize();
}

bool UAMSMapSubsystem::FastTravelSafe(APawn* pawn, FVector location, FRotator rotation, float streamingRadius, float timeout)
{
    return StartFastTravel(pawn, location, &rotation, streamingRadius, timeout);
}

bool UAMSMapSubsystem::StartFastTravel(APawn* pawn, const FVector& location, const FRotator* rotation, float streamingRadius, float timeout)
{
    if (!IsValid(pawn)) {
        return false;
    }

    if (bFastTravelInProgress) {
        // A travel interrupted by a map change leaves stale state behind.
        if (fastTravelPawn.IsValid() && fastTravelWorld.IsValid()) {
            return false;
        }
        CleanupFastTravel();
    }

    UWorld* world = pawn->GetWorld();
    if (!world) {
        return false;
    }

    bFastTravelInProgress = true;
    fastTravelPawn = pawn;
    fastTravelWorld = world;
    fastTravelStreamingRadius = FMath::Max(100.f, streamingRadius);
    fastTravelTimeout = FMath::Max(0.f, timeout);
    fastTravelStartTime = FPlatformTime::Seconds();

    if (ACharacter* character = Cast<ACharacter>(pawn)) {
        if (UCharacterMovementComponent* movement = character->GetCharacterMovement()) {
            movement->StopMovementImmediately();
            movement->DisableMovement();
            fastTravelMovement = movement;
        }
    }

    if (rotation) {
        pawn->SetActorLocationAndRotation(location, *rotation, false, nullptr, ETeleportType::TeleportPhysics);
        if (AController* controller = pawn->GetController()) {
            controller->SetControlRotation(*rotation);
        }
    } else {
        pawn->SetActorLocation(location, false, nullptr, ETeleportType::TeleportPhysics);
    }

    UWorldPartitionSubsystem* worldPartitionSubsystem = world->GetSubsystem<UWorldPartitionSubsystem>();
    if (world->IsGameWorld() && world->GetWorldPartition() && worldPartitionSubsystem) {
        fastTravelStreamingHandle = worldPartitionSubsystem->OnStreamingStateUpdated().AddUObject(
            this, &UAMSMapSubsystem::HandleFastTravelStreamingUpdated);
    } else {
        // Finish on the next tick so listeners bound right after this call still receive the event.
        fastTravelNextTickTimer = world->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &UAMSMapSubsystem::FinishFastTravel, true));
    }
    return true;
}

void UAMSMapSubsystem::HandleFastTravelStreamingUpdated()
{
    APawn* pawn = fastTravelPawn.Get();
    UWorld* world = fastTravelWorld.Get();
    UWorldPartitionSubsystem* worldPartitionSubsystem = world ? world->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
    if (!bFastTravelInProgress) {
        return;
    }
    if (!pawn || !worldPartitionSubsystem) {
        FinishFastTravel(false);
        return;
    }

    FWorldPartitionStreamingQuerySource query(pawn->GetActorLocation());
    query.bUseGridLoadingRange = false;
    query.Radius = fastTravelStreamingRadius;
    if (worldPartitionSubsystem->IsStreamingCompleted(EWorldPartitionRuntimeCellState::Activated, { query }, true)) {
        FinishFastTravel(true);
        return;
    }

    if (fastTravelTimeout > 0.f && (FPlatformTime::Seconds() - fastTravelStartTime) > fastTravelTimeout) {
        UE_LOG(LogTemp, Warning, TEXT("[AMS] FastTravelSafe: World Partition streaming did not complete within %.1f s."), fastTravelTimeout);
        FinishFastTravel(false);
    }
}

void UAMSMapSubsystem::FinishFastTravel(bool bSuccess)
{
    if (!bFastTravelInProgress) {
        return;
    }
    APawn* pawn = fastTravelPawn.Get();
    CleanupFastTravel();
    OnFastTravelFinished.Broadcast(pawn, bSuccess && pawn != nullptr);
}

void UAMSMapSubsystem::CleanupFastTravel()
{
    if (UWorld* world = fastTravelWorld.Get()) {
        if (UWorldPartitionSubsystem* worldPartitionSubsystem = world->GetSubsystem<UWorldPartitionSubsystem>()) {
            worldPartitionSubsystem->OnStreamingStateUpdated().Remove(fastTravelStreamingHandle);
        }
        world->GetTimerManager().ClearTimer(fastTravelNextTickTimer);
    }
    fastTravelStreamingHandle.Reset();

    if (UCharacterMovementComponent* movement = fastTravelMovement.Get()) {
        if (movement->MovementMode == MOVE_None) {
            movement->SetDefaultMovementMode();
        }
    }

    bFastTravelInProgress = false;
    fastTravelPawn.Reset();
    fastTravelWorld.Reset();
    fastTravelMovement.Reset();
}

void UAMSMapSubsystem::RegisterMapArea(const FName& tag, TObjectPtr<class AAMSMapArea> map)
{
    Maps.Add(tag, map);
}

void UAMSMapSubsystem::UnregisterMapArea(const FName& tag)
{
    Maps.Remove(tag);
    Priorities.Remove(tag);

    if (currentMap && (!IsValid(currentMap) || currentMap->GetAreaName() == tag)) {
        currentMap = nullptr;
    }

    UpdateCurrentMap();
}

void UAMSMapSubsystem::RegisterPlayerInArea(const FName& tag, int32 priority)
{
    Priorities.Add(tag, priority);
    UpdateCurrentMap();
}

void UAMSMapSubsystem::RegisterMarker(class UAMSMapMarkerComponent* markerComp)
{
    if (markerComp && !Markers.Contains(markerComp)) {
        Markers.AddUnique(markerComp);
        if (markerComp->GetActivateWorldWidget()) {
            markerComp->SetHiddenInGame(false, true);
        }
        OnMapMarkerAdded.Broadcast(markerComp);
    }
}

void UAMSMapSubsystem::RemoveMarker(class UAMSMapMarkerComponent* markerComp)
{
    if (Markers.Contains(markerComp)) {
        Markers.Remove(markerComp);
        if (IsValid(markerComp) && markerComp->GetActivateWorldWidget()) {
            markerComp->SetHiddenInGame(true, true);
        }
        OnMapMarkerRemoved.Broadcast(markerComp);

        if (TrackedMarker == markerComp) {
            if (IsValid(markerComp)) {
                markerComp->SetTracked(false);
            }
            TrackedMarker = nullptr;
            bHasMarkerTracked = false;
            OnTrackedMarkerChanged.Broadcast(nullptr);
        }
    }
}

AAMSActorMarker* UAMSMapSubsystem::SpawnMarkerActor(const TSubclassOf<AAMSActorMarker>& markerClass, const FVector& worldPos, bool bProjectToNavmesh)
{
    FVector finalPos = worldPos;
    //     if (bProjectToNavmesh) {
    //         FVector outLoc;
    //         if (UNavigationSystemV1::K2_ProjectPointToNavigation(GetWorld(),worldPos, outLoc, nullptr, nullptr)) {
    //             finalPos = outLoc;
    //         }
    //     }
    AAMSActorMarker* newMarker = GetWorld()->SpawnActor<AAMSActorMarker>(markerClass, finalPos, FRotator::ZeroRotator);
    MarkerActors.Add(newMarker);
    return newMarker;
}

void UAMSMapSubsystem::UnregisterMarkerActor(AAMSActorMarker* marker)
{
    MarkerActors.Remove(marker);
}

void UAMSMapSubsystem::RemoveAllMarkerActors()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown) {
        MarkerActors.Empty();
        return;
    }

    TArray<TObjectPtr<AAMSActorMarker>> MarkerActorsCopy = MoveTemp(MarkerActors);
    for (const TObjectPtr<AAMSActorMarker>& Marker : MarkerActorsCopy) {
        if (IsValid(Marker)) {
            Marker->Destroy();
        }
    }
}

bool UAMSMapSubsystem::HasAtLeastOneMarkerActor() const
{
    return MarkerActors.Num() > 0 && IsValid(MarkerActors[0]);
}

TArray<UAMSMapMarkerComponent*> UAMSMapSubsystem::GetAllMarkersByCategory(const FGameplayTag& categoryTag) const
{
    TArray<UAMSMapMarkerComponent*> outMarkers;

    for (UAMSMapMarkerComponent* markerComp : Markers) {
        if (markerComp->GetMarkerCategory().HasTag(categoryTag)) {
            outMarkers.Add(markerComp);
        }
    }
    return outMarkers;
}

void UAMSMapSubsystem::RemoveAllMarkersByCategory(const FGameplayTag& categoryTag)
{
    const TArray<UAMSMapMarkerComponent*> outMarkers = GetAllMarkersByCategory(categoryTag);
    for (UAMSMapMarkerComponent* markerComp : outMarkers) {
        RemoveMarker(markerComp);
    }
}

// UAMSCompassConfigDataAsset* UAMSMapSubsystem::GetCompassConfig() const
// {
//     UAMSDeveloperSettings* settings = GetMutableDefault<UAMSDeveloperSettings>();
//     if (settings) {
//         return settings->GetCompassConfig();
//     }
//     return nullptr;
// }

UAMSMarkersConfigDataAsset* UAMSMapSubsystem::GetIconConfig() const
{
    UAMSDeveloperSettings* settings = GetMutableDefault<UAMSDeveloperSettings>();
    if (settings) {
        return settings->GetMarkerIconConfig();
    }
    return nullptr;
}

void UAMSMapSubsystem::UnregisterPlayerInArea(const FName& tag)
{
    Priorities.Remove(tag);
    UpdateCurrentMap();
}

AAMSMapArea* UAMSMapSubsystem::GetRegisteredMapArea(const FName& tag) const
{
    if (const TObjectPtr<AAMSMapArea>* Found = Maps.Find(tag)) {
        return IsValid(*Found) ? Found->Get() : nullptr;
    }
    return nullptr;
}

AAMSMapArea* UAMSMapSubsystem::GetCurrentMapArea() const
{
    return IsValid(currentMap) ? currentMap.Get() : nullptr;
}

void UAMSMapSubsystem::SetCurrentMapArea(AAMSMapArea* mapActor)
{
    currentMap = IsValid(mapActor) ? mapActor : nullptr;
    OnMapAreaChanged.Broadcast(currentMap);
}

TArray<AAMSMapLocation*> UAMSMapSubsystem::GetAllLocations() const
{
    TArray<AActor*> outLocs;
    TArray<AAMSMapLocation*> finalLocs;
    UGameplayStatics::GetAllActorsOfClass(this, AAMSMapLocation::StaticClass(), outLocs);

    for (const auto& loc : outLocs) {
        AAMSMapLocation* mapLoc = Cast<AAMSMapLocation>(loc);
        if (mapLoc) {
            finalLocs.Add(mapLoc);
        }
    }
    return finalLocs;
}

AAMSMapLocation* UAMSMapSubsystem::GetLocationByID(const FName& locationName) const
{
    TArray<AActor*> outLocs;
    UGameplayStatics::GetAllActorsOfClass(this, AAMSMapLocation::StaticClass(), outLocs);

    for (const auto& loc : outLocs) {
        AAMSMapLocation* mapLoc = Cast<AAMSMapLocation>(loc);
        if (mapLoc && mapLoc->GetLocationID() == locationName) {
            return mapLoc;
        }
    }
    return nullptr;
}

TArray<AAMSMapLocation*> UAMSMapSubsystem::GetAllDiscoveredLocation() const
{
    TArray<AActor*> outLocs;
    TArray<AAMSMapLocation*> finalLocs;
    UGameplayStatics::GetAllActorsOfClass(this, AAMSMapLocation::StaticClass(), outLocs);

    for (const auto& loc : outLocs) {
        AAMSMapLocation* mapLoc = Cast<AAMSMapLocation>(loc);
        if (mapLoc && mapLoc->IsDiscovered()) {
            finalLocs.Add(mapLoc);
        }
    }
    return finalLocs;
}

TArray<AAMSMapLocation*> UAMSMapSubsystem::GetAllDiscoveredFastTravelLocation() const
{
    TArray<AActor*> outLocs;
    TArray<AAMSMapLocation*> finalLocs;
    UGameplayStatics::GetAllActorsOfClass(this, AAMSMapLocation::StaticClass(), outLocs);

    for (const auto& loc : outLocs) {
        AAMSMapLocation* mapLoc = Cast<AAMSMapLocation>(loc);
        if (mapLoc && mapLoc->IsDiscovered() && mapLoc->CanFastTravel()) {
            finalLocs.Add(mapLoc);
        }
    }
    return finalLocs;
}

TArray<class UAMSMapMarkerComponent*> UAMSMapSubsystem::GetAllMarkers() const
{
    TArray<UAMSMapMarkerComponent*> ValidMarkers;
    ValidMarkers.Reserve(Markers.Num());
    for (UAMSMapMarkerComponent* Marker : Markers) {
        if (IsValid(Marker)) {
            ValidMarkers.Add(Marker);
        }
    }
    return ValidMarkers;
}

void UAMSMapSubsystem::DiscoverAllLocation()
{
    TArray<AActor*> outLocs;
    UGameplayStatics::GetAllActorsOfClass(this, AAMSMapLocation::StaticClass(), outLocs);

    for (const auto& loc : outLocs) {
        AAMSMapLocation* mapLoc = Cast<AAMSMapLocation>(loc);
        if (mapLoc) {
            mapLoc->SetDiscoveredState(true);
        }
    }
}

bool UAMSMapSubsystem::IsMarkerActive(const class UAMSMapMarkerComponent* markerComp) const
{
    return Markers.Contains(markerComp);
}

void UAMSMapSubsystem::UpdateCurrentMap()
{
    int32 currentMax = 0;
    FName bestMap;
    for (auto& map : Priorities) {
        if (map.Value > currentMax) {
            currentMax = map.Value;
            bestMap = map.Key;
        }
    }

    const auto newMap = GetRegisteredMapArea(bestMap);
    if (newMap != GetCurrentMapArea()) {
        currentMap = newMap;
        OnMapAreaChanged.Broadcast(currentMap);
    }
}

bool UAMSMapSubsystem::HasAnyTrackedMarker() const
{
    return bHasMarkerTracked && IsValid(TrackedMarker);
}

void UAMSMapSubsystem::TrackMarker(UAMSMapMarkerComponent* marker)
{

    UAMSDeveloperSettings* settings = GetMutableDefault<UAMSDeveloperSettings>();

    if (marker && settings) {
        TrackedMarker = marker;
        TrackedMarker->SetTracked(true);
        bHasMarkerTracked = true;
        OnTrackedMarkerChanged.Broadcast(marker);
    }
}

void UAMSMapSubsystem::UntrackMarker()
{
    if (TrackedMarker) {
        TrackedMarker->SetTracked(false);
        bHasMarkerTracked = false;
        OnTrackedMarkerChanged.Broadcast(TrackedMarker);
    }
}

UAMSMapMarkerComponent* UAMSMapSubsystem::GetCurrentlytTrackedMarker() const
{
    return bHasMarkerTracked && IsValid(TrackedMarker) ? TrackedMarker : nullptr;
}

void UAMSMapSubsystem::UpdateDiscovererPosition(FVector worldPosition)
{
    if (IsValid(GetCurrentMapArea()) && GetCurrentMapArea()->IsFogEnabled()) {
        GetCurrentMapArea()->UpdateFogOfWar(worldPosition);
    }
}

void UAMSMapSubsystem::BroadcastFogOfWarChanged(AAMSMapArea* mapArea)
{
    OnFogOfWarChanged.Broadcast(mapArea);
}
