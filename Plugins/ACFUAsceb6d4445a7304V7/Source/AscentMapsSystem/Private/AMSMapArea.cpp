// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AMSMapArea.h"
#include "AMSActorMarker.h"
#include "AMSMapSubsystem.h"
#include "AMSTypes.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include <Engine/Canvas.h>
#include <Engine/GameInstance.h>
#include <GameFramework/Pawn.h>
#include <Kismet/KismetRenderingLibrary.h>
#include <Logging/StructuredLog.h>
#include <Materials/MaterialInstance.h>

// Sets default values
AAMSMapArea::AAMSMapArea()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;
    CaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Scene Capture"));
    MapBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Box Component"));
    SetRootComponent(MapBounds);
    CaptureComp->SetupAttachment(MapBounds);
    CaptureComp->bCaptureEveryFrame = false;

    MapBounds->OnComponentBeginOverlap.AddDynamic(this, &AAMSMapArea::OnBeginOverlap);
    MapBounds->OnComponentEndOverlap.AddDynamic(this, &AAMSMapArea::OnEndOverlap);
    MapBounds->InitBoxExtent(FVector(AreaSize));

    CaptureComp->ProjectionType = ECameraProjectionMode::Type::Orthographic;
    CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    TextureType = EAMSBackgroundType::ECustomTexture;

    UpdateBoxProperties();
}

void AAMSMapArea::ClearFogOfWarMatrix()
{
    FogOfWarBits.Init(true, FogWidthPixelsCount * GetFogHeightPixelsCount());
}

UTexture* AAMSMapArea::GetMapTexture() const
{
    if (TextureType == EAMSBackgroundType::ERenderTarget) {
        return CaptureComp->TextureTarget;
    } else {
        return Texture;
    }
}

FVector AAMSMapArea::GetMapAreaCenter() const
{
    return MapBounds->GetComponentLocation();
}

FVector AAMSMapArea::GetMapAreaExtent() const
{
    return MapBounds->GetScaledBoxExtent();
}

FVector2D AAMSMapArea::GetMapAreaTopLeftmostPoint() const
{
    return FVector2D(GetMapAreaCenter().X - GetMapAreaExtent().X,
        GetMapAreaCenter().Y - GetMapAreaExtent().Y);
}

FVector2D AAMSMapArea::GetMapAreaBottomRightmostPoint() const
{
    return FVector2D(GetMapAreaCenter().X + GetMapAreaExtent().X,
        GetMapAreaCenter().Y + GetMapAreaExtent().Y);
}

FVector AAMSMapArea::GetWorldLocationFromNormalized2DPosition(const FVector2D& normalizedPosition) const
{
    FVector outVector;

    outVector.X = normalizedPosition.X * GetMapAreaExtent().X * 2;
    outVector.Y = normalizedPosition.Y * GetMapAreaExtent().Y * 2;

    const FVector2D leftmost = GetMapAreaTopLeftmostPoint();
    outVector.X += leftmost.X;
    outVector.Y += leftmost.Y;

    outVector.Z = MapBounds->GetComponentLocation().Z;
    // 	UKismetSystemLibrary::LineTraceSingleForObjects(this, GetT)
    // 	outVector.Z =
    ensure(IsPointInThisArea(outVector));
    return outVector;
}

FVector2D AAMSMapArea::GetNormalized2DPositionFromWorldLocation(const FVector& worldPos) const
{
    FVector2D outPos;
    //     const FVector relativePos = GetPointRelativeLocation(worldPos);
    //
    //     outPos.X = relativePos.X / GetMapAreaExtent().X;
    //     outPos.Y = relativePos.Y / GetMapAreaExtent().Y;

    const FVector2D leftmost = GetMapAreaTopLeftmostPoint();
    const FVector2D rightmost = GetMapAreaBottomRightmostPoint();

    outPos.X = UKismetMathLibrary::NormalizeToRange(worldPos.X, leftmost.X,
        rightmost.X);

    outPos.Y = UKismetMathLibrary::NormalizeToRange(worldPos.Y, leftmost.Y,
        rightmost.Y);

    return outPos;
}

bool AAMSMapArea::IsPointInThisArea(const FVector& point) const
{
    return UKismetMathLibrary::IsPointInBox(point, GetMapAreaCenter(), GetMapAreaExtent());
}

FVector AAMSMapArea::GetPointRelativeLocation(const FVector& point) const
{
    return point - GetMapAreaCenter();
}

void AAMSMapArea::OnSave()
{
    const int32 numBytes = (FogOfWarBits.Num() + 7) / 8;
    SavedFogMapData.Init(0, numBytes);
    for (int32 BitIndex = 0; BitIndex < FogOfWarBits.Num(); ++BitIndex) {
        if (FogOfWarBits[BitIndex]) {
            const int32 ByteIndex = BitIndex / 8;
            const int32 BitOffset = BitIndex % 8;
            SavedFogMapData[ByteIndex] |= static_cast<uint8>(1 << BitOffset);
        }
    }
}

void AAMSMapArea::OnLoad()
{
    const FIntPoint fogSize = GetFogCellsCount();
    const int32 numBits = fogSize.X * fogSize.Y;
    if (numBits <= 0) {
        return;
    }

    FogOfWarBits.Init(true, numBits);
    if (SavedFogMapData.Num() == 0) {
        return;
    }

    for (int32 BitIndex = 0; BitIndex < numBits; ++BitIndex) {
        const int32 ByteIndex = BitIndex / 8;
        if (!SavedFogMapData.IsValidIndex(ByteIndex)) {
            break;
        }
        const int32 BitOffset = BitIndex % 8;
        FogOfWarBits[BitIndex] = ((SavedFogMapData[ByteIndex] >> BitOffset) & 1) != 0;
    }
}

void AAMSMapArea::OnSaved_Implementation()
{
    OnSave();
}

void AAMSMapArea::OnLoaded_Implementation()
{
    OnLoad();
    if (UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(this)) {
        if (UAMSMapSubsystem* mapSubsystem = gameInstance->GetSubsystem<UAMSMapSubsystem>()) {
            mapSubsystem->BroadcastFogOfWarChanged(this);
        }
    }
}

int AAMSMapArea::GetFogCellWidth() const
{
    const FVector2D textureSize = GetTextureSize();
    return FMath::CeilToInt(textureSize.X / FogWidthPixelsCount);
}

int AAMSMapArea::GetFogHeightPixelsCount() const
{
    FVector2D textureSize = GetTextureSize();
    int32 fogTextureSide = GetFogCellWidth();
    return FMath::CeilToInt(textureSize.Y / fogTextureSide);
}

FIntPoint AAMSMapArea::GetFogCellsCount() const
{
    return FIntPoint(FogWidthPixelsCount, GetFogHeightPixelsCount());
}

// Called when the game starts or when spawned
void AAMSMapArea::BeginPlay()
{
    Super::BeginPlay();
    CaptureComp->bCaptureEveryFrame = false;
    if (TextureType == EAMSBackgroundType::ERenderTarget) {
        CaptureComp->TextureTarget = GetRenderTarget();
        GenerateMapTexture();
    }

    if (EnableFog && FogWidthPixelsCount != 0) {
        if (SavedFogMapData.Num() > 0) {
            OnLoad();
        } else {
            ClearFogOfWarMatrix();
        }
    }

    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
    if (mapSubsystem) {
        mapSubsystem->RegisterMapArea(AreaName, this);
        const APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        if (currentPawn) {
            TArray<AActor*> overlappingActors;
            MapBounds->GetOverlappingActors(overlappingActors, currentPawn->StaticClass());
            if (overlappingActors.Contains(currentPawn)) {
                mapSubsystem->RegisterPlayerInArea(AreaName, AreaPriority);
            }
        }
    }
}

void AAMSMapArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this)) {
        if (UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>()) {
            mapSubsystem->UnregisterMapArea(AreaName);
        }
    }

    Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AAMSMapArea::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    UpdateBoxProperties();
}
#endif

FVector2D AAMSMapArea::GetTextureSize() const
{
    float textureSizeX, textureSizeY;
    if (TextureType == EAMSBackgroundType::ERenderTarget) {
        textureSizeX = CaptureComp->TextureTarget->SizeX;
        textureSizeY = CaptureComp->TextureTarget->SizeY;
    } else {
        textureSizeX = textureSizeY = AreaSize;
    }
    return FVector2D(textureSizeX, textureSizeY);
}

void AAMSMapArea::UpdateBoxProperties()
{
    MapBounds->SetBoxExtent(FVector(AreaSize));
    const float lineTickness = FMath::Clamp(5.0f, 500.f, AreaSize / 100.f);
    MapBounds->SetLineThickness(lineTickness);
    CaptureComp->SetRelativeLocation(FVector(0, 0, AreaSize));
    CaptureComp->SetWorldRotation(FRotator(-90, -90, 0));
    CaptureComp->OrthoWidth = AreaSize * 2;
}

void AAMSMapArea::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    const APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (currentPawn) {
        if (OtherActor == currentPawn) {
            UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);

            UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();

            mapSubsystem->RegisterPlayerInArea(AreaName, AreaPriority);
        }
    }
}

void AAMSMapArea::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    const APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (currentPawn) {
        if (OtherActor == currentPawn) {
            UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
            UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
            mapSubsystem->UnregisterPlayerInArea(AreaName);
        }
    }
}

void AAMSMapArea::UpdateFogOfWar(FVector worldPosition, float revealRadius)
{
    if (!EnableFog) {
        return;
    }

    if (FogOfWarBits.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("FogOfWar not initialized!"));
        return;
    }

    const FIntPoint cellsCount = GetFogCellsCount();
    const FVector2D normalizedPos = GetNormalized2DPositionFromWorldLocation(worldPosition);
    const FVector2D centerCell(normalizedPos.X * cellsCount.X, normalizedPos.Y * cellsCount.Y);

    const FVector2D cellWorldSize(
        (GetMapAreaExtent().X * 2.f) / cellsCount.X,
        (GetMapAreaExtent().Y * 2.f) / cellsCount.Y);

    bool bChanged = false;

    if (revealRadius <= 0.f) {
        const int32 cellX = FMath::Clamp(FMath::FloorToInt(centerCell.X), 0, cellsCount.X - 1);
        const int32 cellY = FMath::Clamp(FMath::FloorToInt(centerCell.Y), 0, cellsCount.Y - 1);
        const int32 index = cellY * FogWidthPixelsCount + cellX;
        if (FogOfWarBits.IsValidIndex(index) && FogOfWarBits[index]) {
            FogOfWarBits[index] = false;
            bChanged = true;
        }
    } else {
        const int32 minX = FMath::Max(0, FMath::FloorToInt(centerCell.X - revealRadius / cellWorldSize.X));
        const int32 maxX = FMath::Min(cellsCount.X - 1, FMath::CeilToInt(centerCell.X + revealRadius / cellWorldSize.X));
        const int32 minY = FMath::Max(0, FMath::FloorToInt(centerCell.Y - revealRadius / cellWorldSize.Y));
        const int32 maxY = FMath::Min(cellsCount.Y - 1, FMath::CeilToInt(centerCell.Y + revealRadius / cellWorldSize.Y));

        for (int32 y = minY; y <= maxY; ++y) {
            for (int32 x = minX; x <= maxX; ++x) {
                const FVector2D cellNormalized(
                    (static_cast<float>(x) + 0.5f) / cellsCount.X,
                    (static_cast<float>(y) + 0.5f) / cellsCount.Y);
                const FVector cellWorld = GetWorldLocationFromNormalized2DPosition(cellNormalized);
                if (FVector::Dist2D(worldPosition, cellWorld) <= revealRadius) {
                    const int32 index = y * FogWidthPixelsCount + x;
                    if (FogOfWarBits.IsValidIndex(index) && FogOfWarBits[index]) {
                        FogOfWarBits[index] = false;
                        bChanged = true;
                    }
                }
            }
        }
    }

    if (bChanged) {
        OnSave();
        if (UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(this)) {
            if (UAMSMapSubsystem* mapSubsystem = gameInstance->GetSubsystem<UAMSMapSubsystem>()) {
                mapSubsystem->BroadcastFogOfWarChanged(this);
            }
        }
    }
}

bool AAMSMapArea::IsFogCellHidden(int32 cellX, int32 cellY) const
{
    const int32 index = cellY * FogWidthPixelsCount + cellX;
    return FogOfWarBits.IsValidIndex(index) ? FogOfWarBits[index] : true;
}

void AAMSMapArea::GenerateMapTexture()
{
    CaptureComp->CaptureScene();
}

void AAMSMapArea::GenerateMap(const TArray<AActor*>& ActorsToHide, float ExposuteCompensation /*= 1.f*/)
{
    CaptureComp->HiddenActors = ActorsToHide;
    CaptureComp->PostProcessSettings.AutoExposureBias = ExposuteCompensation;
    GenerateMapTexture();
}

// TODO: IMPLEMENT ME
AAMSActorMarker* AAMSMapArea::SpawnActorMarkerAtMapLocation(const FVector2D& normalizedMapPosition, const TSubclassOf<AAMSActorMarker>& markerClass)
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    UAMSMapSubsystem* mapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>();
    const FVector worldPos = GetWorldLocationFromNormalized2DPosition(normalizedMapPosition);

    return mapSubsystem->SpawnMarkerActor(markerClass, worldPos);
}
