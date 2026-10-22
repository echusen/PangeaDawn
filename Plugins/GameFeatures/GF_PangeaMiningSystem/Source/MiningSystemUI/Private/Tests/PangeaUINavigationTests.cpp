#include "ANSNavPageWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "ANSUIWidgetRegistryDataAsset.h"
#include "Actors/MiningSiteActor.h"
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/VillageBase.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/UpgradeSystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Actors/MiningSiteChestActor.h"
#include "Actors/MiningSettlementStockpileActor.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace
{
    UWorld* GetPangeaPIEWorld()
    {
        if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetFirstPlayerController()) return Context.World();
        return nullptr;
    }
    const TCHAR* FeatureTags[] = { TEXT("UI.Widget.Pangea.MiningSite"), TEXT("UI.Widget.Pangea.MiningChest"),
        TEXT("UI.Widget.Pangea.Stockpile"), TEXT("UI.Widget.Pangea.Village"), TEXT("UI.Widget.Pangea.Breeding") };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUIRegistryTest, "Pangea.ACF.UI.RegistryContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUIRegistryTest::RunTest(const FString& Parameters)
{
    const UANSUIWidgetRegistryDataAsset* Registry = LoadObject<UANSUIWidgetRegistryDataAsset>(nullptr,
        TEXT("/Game/Yarin/DA_PangeaWidgetRegistry.DA_PangeaWidgetRegistry"));
    if (!TestNotNull(TEXT("Pangea widget registry"), Registry)) return false;
    for (const TCHAR* Name : FeatureTags)
    {
        const FANSWidgetConfig* Config = Registry->FindConfigForTag(FGameplayTag::RequestGameplayTag(Name));
        if (!TestNotNull(Name, Config)) continue;
        TestTrue(FString(Name) + TEXT(" is an ACF navigation page"), Config->WidgetClass && Config->WidgetClass->IsChildOf(UANSNavPageWidget::StaticClass()));
        TestFalse(FString(Name) + TEXT(" keeps the simulation running"), Config->bPauseGame);
        TestTrue(FString(Name) + TEXT(" requests menu input through ANS"), Config->bLockGameInput);
    }
    const FANSWidgetConfig* Taming = Registry->FindConfigForTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Pangea.Taming")));
    if (TestNotNull(TEXT("Taming activity registered"), Taming))
    {
        TestFalse(TEXT("Taming never pauses its own timer"), Taming->bPauseGame);
        TestFalse(TEXT("Taming preserves gameplay input"), Taming->bLockGameInput);
        TestEqual(TEXT("Taming has a dedicated activity layer"), Taming->LayerTag.GetTagName(), FName(TEXT("UI.Layer.PangeaActivity")));
    }
    TestNotNull(TEXT("Settings has a valid registry key"), Registry->FindConfigForTag(FGameplayTag::RequestGameplayTag(TEXT("UI.Widget.Settings"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaUIPageSmokeTest, "Pangea.ACF.UI.PagesInPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaUIPageSmokeTest::RunTest(const FString& Parameters)
{
    if (Parameters != TEXT("PIEChecks"))
    {
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0]() mutable
        {
            if (!GetPangeaPIEWorld() && FPlatformTime::Seconds() < Deadline) return false;
            if (ReadyAt == 0.0) { ReadyAt = FPlatformTime::Seconds() + 1.0; return false; }
            if (FPlatformTime::Seconds() < ReadyAt) return false;
            RunTest(TEXT("PIEChecks")); return true;
        }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }
    UWorld* World = GetPangeaPIEWorld();
    if (!TestNotNull(TEXT("Start PIE before running the UI smoke test"), World)) return false;
    UANSUIPlayerSubsystem* UI = World->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>();
    if (!TestNotNull(TEXT("ANS subsystem"), UI)) return false;
    const bool bWasPaused = UGameplayStatics::IsGamePaused(World);
    for (const TCHAR* Name : FeatureTags)
    {
        UANSNavPageWidget* Page = Cast<UANSNavPageWidget>(UI->SpawnWidgetByTag(FGameplayTag::RequestGameplayTag(Name)));
        if (!TestNotNull(Name, Page)) continue;
        UUserWidget* Content = Cast<UUserWidget>(Page->GetWidgetFromName(TEXT("ScreenContent")));
        if (TestNotNull(TEXT("Feature content exists inside the page"), Content))
            TestTrue(TEXT("Content resolves its owning ANS page for close/cleanup"), Content->GetTypedOuter<UANSNavPageWidget>() == Page);
        TestNotNull(TEXT("Page has a predictable focus target"), Page->GetDesiredFocusTarget());
        UI->RemoveInGameWidget(Page);
        TestFalse(TEXT("Closing deactivates the page"), Page->IsActivated());
        TestEqual(TEXT("Feature pages preserve the existing pause state"), UGameplayStatics::IsGamePaused(World), bWasPaused);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaMiningMenuRouteTest, "Pangea.ACF.UI.MiningActorRoutesInPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaMiningMenuRouteTest::RunTest(const FString& Parameters)
{
    if (Parameters != TEXT("PIEChecks"))
    {
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0]() mutable
        {
            if (!GetPangeaPIEWorld() && FPlatformTime::Seconds() < Deadline) return false;
            if (ReadyAt == 0.0) { ReadyAt = FPlatformTime::Seconds() + 1.0; return false; }
            if (FPlatformTime::Seconds() < ReadyAt) return false;
            RunTest(TEXT("PIEChecks")); return true;
        }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }
    UWorld* World = GetPangeaPIEWorld();
    if (!TestNotNull(TEXT("Start PIE before testing mining interaction routes"), World)) return false;
    APawn* Pawn = World->GetFirstPlayerController()->GetPawn();
    if (!TestNotNull(TEXT("Local interacting pawn"), Pawn)) return false;
    AMiningSiteActor* Site = World->SpawnActor<AMiningSiteActor>();
    AMiningSiteChestActor* Chest = World->SpawnActor<AMiningSiteChestActor>();
    AMiningSettlementStockpileActor* Stockpile = World->SpawnActor<AMiningSettlementStockpileActor>();
    ON_SCOPE_EXIT { if (Site) Site->Destroy(); if (Chest) Chest->Destroy(); if (Stockpile) Stockpile->Destroy(); };
    if (!Site || !Chest || !Stockpile) { AddError(TEXT("Could not spawn mining test fixtures")); return false; }
    Site->OpenInteractionMenu(Pawn);
    Chest->OpenChestMenu(Pawn);
    Stockpile->OpenStockpileMenu(Pawn);
    TArray<UUserWidget*> Pages;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Pages, UANSNavPageWidget::StaticClass(), false);
    int32 Found = 0;
    for (UUserWidget* Widget : Pages)
    {
        if (Widget->GetOwningPlayer() != World->GetFirstPlayerController()) continue;
        UUserWidget* Content = Cast<UUserWidget>(Widget->GetWidgetFromName(TEXT("ScreenContent")));
        if (!Content) continue;
        const FObjectPropertyBase* SiteProperty = FindFProperty<FObjectPropertyBase>(Content->GetClass(), TEXT("SiteActor"));
        const FObjectPropertyBase* ChestProperty = FindFProperty<FObjectPropertyBase>(Content->GetClass(), TEXT("ChestActor"));
        const FObjectPropertyBase* StockpileProperty = FindFProperty<FObjectPropertyBase>(Content->GetClass(), TEXT("StockpileActor"));
        if ((SiteProperty && SiteProperty->GetObjectPropertyValue_InContainer(Content) == Site) ||
            (ChestProperty && ChestProperty->GetObjectPropertyValue_InContainer(Content) == Chest) ||
            (StockpileProperty && StockpileProperty->GetObjectPropertyValue_InContainer(Content) == Stockpile)) ++Found;
    }
    TestTrue(TEXT("All three mining actors route to ACF-hosted content"), Found >= 3);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaMiningWorldInteractionTest, "Pangea.ACF.UI.VillageToMiningInteractionInPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaMiningWorldInteractionTest::RunTest(const FString& Parameters)
{
    if (Parameters != TEXT("PIEChecks"))
    {
        ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0]() mutable
        {
            if (!GetPangeaPIEWorld() && FPlatformTime::Seconds() < Deadline) return false;
            if (ReadyAt == 0.0) { ReadyAt = FPlatformTime::Seconds() + 1.0; return false; }
            if (FPlatformTime::Seconds() < ReadyAt) return false;
            RunTest(TEXT("PIEChecks")); return true;
        }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }
    UWorld* World = GetPangeaPIEWorld();
    if (!TestNotNull(TEXT("PIE world"), World)) return false;
    APawn* Pawn = World->GetFirstPlayerController()->GetPawn();
    UACFInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UACFInteractionComponent>() : nullptr;
    if (!TestNotNull(TEXT("Real player ACF interaction component"), Interaction)) return false;
    AMiningDiscoveryNodeActor* Node = nullptr;
    UPangeaUpgradeSystemComponent* Upgrades = nullptr;
    for (TActorIterator<AMiningDiscoveryNodeActor> It(World); It; ++It)
        if (!It->ActorHasTag(TEXT("Pangea.Mining.Prototype"))) { Node = *It; break; }
    for (TActorIterator<AVillageBase> It(World); It; ++It)
        if ((Upgrades = It->FindComponentByClass<UPangeaUpgradeSystemComponent>())) break;
    if (!TestNotNull(TEXT("Map mining discovery node"), Node) ||
        !TestNotNull(TEXT("Map village upgrade component"), Upgrades)) return false;
    if (!TestNull(TEXT("Fresh PIE mine is not established"), Node->EstablishedSite.Get())) return false;
    Node->SetUnlocked(false);
    TestFalse(TEXT("Map mine starts locked"), Node->CanSetUpSite());
    Upgrades->OnLevelIncreased(1, Pawn);
    Upgrades->OnLevelIncreased(2, Pawn);
    if (!TestTrue(TEXT("Map village upgrade unlocks the actual mine"), Node->CanSetUpSite())) return false;
    Pawn->SetActorLocation(Node->GetActorLocation() + FVector(80.0, 0.0, 90.0), false, nullptr, ETeleportType::TeleportPhysics);
    TArray<UPrimitiveComponent*> Primitives;
    Pawn->GetComponents(Primitives);
    for (UPrimitiveComponent* Primitive : Primitives) Primitive->UpdateOverlaps();
    Interaction->RefreshInteractions();
    if (!TestTrue(TEXT("Walking into range selects discovery through ACF"), Interaction->GetCurrentBestInteractableActor() == Node)) return false;
    Interaction->Interact();
    AMiningSiteActor* Site = Node->EstablishedSite;
    if (!TestNotNull(TEXT("ACF interaction establishes the configured site"), Site)) return false;
    // SC_Demo deliberately spawns the site 300 cm from the discovery marker.
    // Enter the site's own interaction area rather than assuming both overlap.
    Pawn->SetActorLocation(Site->GetActorLocation() + FVector(80.0, 0.0, 90.0), false, nullptr, ETeleportType::TeleportPhysics);
    for (UPrimitiveComponent* Primitive : Primitives) Primitive->UpdateOverlaps();
    Interaction->RefreshInteractions();
    if (!TestTrue(TEXT("ACF hands interaction to the established site"), Interaction->GetCurrentBestInteractableActor() == Site)) return false;
    Interaction->Interact();
    TArray<UUserWidget*> Pages;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Pages, UANSNavPageWidget::StaticClass(), false);
    bool bFoundSitePage = false;
    for (UUserWidget* Widget : Pages)
    {
        UUserWidget* Content = Cast<UUserWidget>(Widget->GetWidgetFromName(TEXT("ScreenContent")));
        const FObjectPropertyBase* Property = Content ? FindFProperty<FObjectPropertyBase>(Content->GetClass(), TEXT("SiteActor")) : nullptr;
        if (Property && Property->GetObjectPropertyValue_InContainer(Content) == Site)
            bFoundSitePage |= CastChecked<UANSNavPageWidget>(Widget)->IsActivated();
    }
    TestTrue(TEXT("Real ACF interaction opens the active mining navigation page"), bFoundSitePage);
    return !HasAnyErrors();
}
#endif
