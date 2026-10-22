#include "ACF/PangeaItemsManagerComponent.h"
#include "ACFMountSystemFunctionLibrary.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Preview/ACFCharacterPreviewStage.h"

namespace
{
// Private implementation, not a gameplay/Blueprint API.
void MakePreviewInert(AACFCharacter& Character)
{
    Character.AutoPossessAI = EAutoPossessAI::Disabled;
    Character.AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Character.SetReplicates(false);
    Character.SetReplicateMovement(false);
    Character.SetCanBeDamaged(false);
    Character.SetIsImmortal(true);
    Character.SetActorEnableCollision(false);
    Character.SetActorTickEnabled(false); // Mesh/capture component ticks remain available.
    if (UCharacterMovementComponent* Movement = Character.GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        Movement->GravityScale = 0.f;
        Movement->SetComponentTickEnabled(false);
    }
}
}

UPangeaItemsManagerComponent::UPangeaItemsManagerComponent()
{
    bWantsInitializeComponent = true;
}

void UPangeaItemsManagerComponent::CaptureConfiguration()
{
    if (bCapturedConfiguration) return;
    ConfiguredStageClass = ItemPreviewStageClass;
    // ACF's spawn API is nonvirtual. Gate its unsafe spawn path before any
    // component BeginPlay/UI call; reopen it only while a complete safe pair exists.
    ItemPreviewStageClass.Reset();
    bCapturedConfiguration = true;
}

void UPangeaItemsManagerComponent::InitializeComponent()
{
    CaptureConfiguration();
    Super::InitializeComponent();
}

void UPangeaItemsManagerComponent::BeginPlay()
{
    CaptureConfiguration();
    Super::BeginPlay(); // Native SpawnItemPreviewStage sees the gated/null class.
    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
        PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    RequestLocalPreview();
}

AACFCharacterPreviewStage* UPangeaItemsManagerComponent::RequestLocalPreview()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (bEndingPlay || !HasBegunPlay() || !PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer) return nullptr;
    CaptureConfiguration();
    if (IsValid(ItemPreviewStage) && IsValid(PreviewCharacter)) return ItemPreviewStage;
    if (ConfiguredStageClass.IsNull()) return nullptr;
    AACFCharacter* Player = UACFMountSystemFunctionLibrary::GetLocalRiderPlayerCharacter(this);
    if (!IsValid(Player)) return nullptr; // Possession notification will retry; no stranded stage.

    if (!ConfiguredStageClass.IsValid())
    {
        if (!PreviewStreamableHandle.IsValid())
            PreviewStreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
                ConfiguredStageClass.ToSoftObjectPath(),
                FStreamableDelegate::CreateUObject(this, &ThisClass::OnPreviewAssetsLoaded),
                FStreamableManager::AsyncLoadHighPriority);
        return nullptr;
    }
    ReleaseLocalPreview(); // Recover any partial/destroyed pair without leaking a stage.
    if (!CreateSafePreview(*Player))
    {
        ReleaseLocalPreview();
        return nullptr;
    }
    // All inherited UI calls now return the already-safe stage without spawning.
    ItemPreviewStageClass = ConfiguredStageClass;
    return ItemPreviewStage;
}

bool UPangeaItemsManagerComponent::CreateSafePreview(AACFCharacter& Player)
{
    UWorld* World = GetWorld();
    if (!World || !ConfiguredStageClass.IsValid()) return false;
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.ObjectFlags |= RF_Transient;
    Params.bDeferConstruction = true;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ItemPreviewStage = World->SpawnActor<AACFCharacterPreviewStage>(ConfiguredStageClass.Get(), ItemPreviewStageTransform, Params);
    if (!IsValid(ItemPreviewStage)) return false;
    ItemPreviewStage->SetReplicates(false);
    ItemPreviewStage->FinishSpawning(ItemPreviewStageTransform);
    if (!IsValid(ItemPreviewStage)) return false;
    ItemPreviewStage->SetReplicates(false);
    ItemPreviewStage->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandlePreviewDestroyed);

    PreviewCharacter = World->SpawnActorDeferred<AACFCharacter>(Player.GetClass(), ItemPreviewStageTransform, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(PreviewCharacter)) return false;
    PreviewCharacter->SetFlags(RF_Transient);
    MakePreviewInert(*PreviewCharacter);
    PreviewCharacter->FinishSpawning(ItemPreviewStageTransform);
    if (!IsValid(PreviewCharacter)) return false;
    MakePreviewInert(*PreviewCharacter);
    PreviewCharacter->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandlePreviewDestroyed);
    PreviewCharacter->AttachToActor(ItemPreviewStage, FAttachmentTransformRules::KeepWorldTransform);

    if (USceneCaptureComponent2D* Capture = PreviewCharacter->FindComponentByClass<USceneCaptureComponent2D>())
    {
        Capture->TextureTarget = ItemPreviewRenderTarget;
        Capture->bCaptureEveryFrame = true;
        Capture->SetTickableWhenPaused(true);
    }
    AppearanceSource = &Player;
    if (UACFEquipmentComponent* Equipment = Player.FindComponentByClass<UACFEquipmentComponent>())
        Equipment->OnEquipmentChanged.AddUniqueDynamic(static_cast<UACFItemsManagerComponent*>(this), &UACFItemsManagerComponent::ApplyAppearance);
    SpawnItemPreviewLight();
    ApplyAppearance({});
    TArray<ACharacter*> Characters { PreviewCharacter };
    ItemPreviewStage->PositioningCharactersOnPreviewSphere(Characters, ItemPreviewSphereRadius, ItemPreviewSpacingMultiplier);
    UpdateItemPreviewLightSettings();
    return true;
}

void UPangeaItemsManagerComponent::ReleaseLocalPreview()
{
    ItemPreviewStageClass.Reset();
    if (AppearanceSource.IsValid())
        if (UACFEquipmentComponent* Equipment = AppearanceSource->FindComponentByClass<UACFEquipmentComponent>())
            Equipment->OnEquipmentChanged.RemoveDynamic(static_cast<UACFItemsManagerComponent*>(this), &UACFItemsManagerComponent::ApplyAppearance);
    AppearanceSource.Reset();
    if (IsValid(ItemPreviewLight)) ItemPreviewLight->DestroyComponent();
    ItemPreviewLight = nullptr;
    AACFCharacter* OldPreview = PreviewCharacter;
    AACFCharacterPreviewStage* OldStage = ItemPreviewStage;
    PreviewCharacter = nullptr;
    ItemPreviewStage = nullptr;
    if (IsValid(OldPreview))
    {
        OldPreview->OnDestroyed.RemoveDynamic(this, &ThisClass::HandlePreviewDestroyed);
        OldPreview->Destroy();
    }
    if (IsValid(OldStage))
    {
        OldStage->OnDestroyed.RemoveDynamic(this, &ThisClass::HandlePreviewDestroyed);
        OldStage->Destroy();
    }
}

void UPangeaItemsManagerComponent::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
    ReleaseLocalPreview();
    if (GetWorld() && NewPawn && !bEndingPlay)
    {
        GetWorld()->GetTimerManager().ClearTimer(PreviewRetry);
        PreviewRetry = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RetryPreview);
    }
}

void UPangeaItemsManagerComponent::HandlePreviewDestroyed(AActor* DestroyedActor)
{
    // Gate the inherited nonvirtual factory synchronously before a UI can retry.
    ReleaseLocalPreview();
    if (GetWorld() && !bEndingPlay)
    {
        GetWorld()->GetTimerManager().ClearTimer(PreviewRetry);
        PreviewRetry = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RetryPreview);
    }
}

void UPangeaItemsManagerComponent::OnPreviewAssetsLoaded()
{
    if (bEndingPlay) return;
    if (!ConfiguredStageClass.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Pangea item preview stage failed to load: %s"), *ConfiguredStageClass.ToSoftObjectPath().ToString());
        PreviewStreamableHandle.Reset();
        return;
    }
    RequestLocalPreview();
}

void UPangeaItemsManagerComponent::RetryPreview()
{
    RequestLocalPreview();
}

void UPangeaItemsManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PreviewRetry);
    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
        PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
    if (PreviewStreamableHandle.IsValid())
    {
        PreviewStreamableHandle->CancelHandle();
        PreviewStreamableHandle.Reset();
    }
    ReleaseLocalPreview();
    Super::EndPlay(EndPlayReason);
}
