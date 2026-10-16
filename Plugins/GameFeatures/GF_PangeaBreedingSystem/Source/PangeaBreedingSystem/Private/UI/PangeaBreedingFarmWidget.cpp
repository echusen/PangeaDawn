// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "UI/PangeaBreedingFarmWidget.h"

#include "Actors/PangeaEggActor.h"
#include "Components/PangeaBreedableComponent.h"
#include "Components/PangeaBreedingFarmComponent.h"
#include "Helpers/PangeaBreedingLibrary.h"
#include "ANSNavPageWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

UPangeaBreedingFarmWidget::UPangeaBreedingFarmWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UPangeaBreedingFarmWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UANSNavPageWidget* Page = GetTypedOuter<UANSNavPageWidget>())
    {
        Page->OnDeactivated().RemoveAll(this);
        Page->OnDeactivated().AddWeakLambda(this, [this]()
        {
            if (IsValid(CurrentFarm)) CurrentFarm->OnEggSpawned.RemoveAll(this);
        });
    }
    
    if (CurrentFarm)
    {
        // Bind UI to farm events
        CurrentFarm->RefreshContainedBreedables();
        CurrentFarm->OnEggSpawned.AddUniqueDynamic(this, &UPangeaBreedingFarmWidget::OnEggSpawned);
    }
}

void UPangeaBreedingFarmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UANSNavPageWidget* Page = GetTypedOuter<UANSNavPageWidget>();
    if (Page && !Page->IsActivated()) return;
    if (!IsValid(CurrentFarm)) { if (Page) Page->RemoveSelf(); return; }
    // Context is supplied by SetupWithActor after ANS has constructed the page.
    CurrentFarm->OnEggSpawned.AddUniqueDynamic(this, &UPangeaBreedingFarmWidget::OnEggSpawned);
    FString Reason;
    const bool bValidPair = ValidateParentPair(SelectedMale, SelectedFemale, Reason);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("B_BreedButton"))))
    {
        Button->SetIsEnabled(bValidPair);
        Button->SetToolTipText(FText::FromString(bValidPair ? TEXT("Breed selected parents") : Reason));
    }
    if (UTextBlock* Summary = Cast<UTextBlock>(GetWidgetFromName(TEXT("SelectionSummary"))))
        Summary->SetText(FText::Format(NSLOCTEXT("PangeaUI", "ParentSelection", "Male: {0}   |   Female: {1}"),
            IsValid(SelectedMale) ? FText::FromName(SelectedMale->BuildParentSnapshot().SpeciesID) : NSLOCTEXT("PangeaUI", "SelectMale", "Select a male"),
            IsValid(SelectedFemale) ? FText::FromName(SelectedFemale->BuildParentSnapshot().SpeciesID) : NSLOCTEXT("PangeaUI", "SelectFemale", "Select a female")));
}

void UPangeaBreedingFarmWidget::OnEggSpawned(APangeaEggActor* NewEgg)
{
    // Forward to Blueprint for UI updates
    AddEggEntry(NewEgg);
}

void UPangeaBreedingFarmWidget::NativeDestruct()
{
    if (IsValid(CurrentFarm))
        CurrentFarm->OnEggSpawned.RemoveAll(this);
    Super::NativeDestruct();
}

bool UPangeaBreedingFarmWidget::ValidateParentPair(UPangeaBreedableComponent* Male,UPangeaBreedableComponent* Female, FString& OutReason) const
{
    if (!IsValid(CurrentFarm) || !IsValid(Male) || !IsValid(Female) || Male == Female)
    {
        OutReason = TEXT("Missing parent(s).");
        return false;
    }

    const TArray<UPangeaBreedableComponent*> Residents = CurrentFarm->GetContainedBreedables();
    if (!Residents.Contains(Male) || !Residents.Contains(Female))
    {
        OutReason = TEXT("Both selected parents must be inside this farm.");
        return false;
    }

    if (Male->BuildParentSnapshot().SpeciesID != Female->BuildParentSnapshot().SpeciesID)
    {
        OutReason = TEXT("Mismatched species.");
        return false;
    }

    if (Male->Gender != ECreatureGender::Male || Female->Gender != ECreatureGender::Female)
    {
        OutReason = TEXT("Second parent must be female.");
        return false;
    }

    if (!IPDBreedableInterface::Execute_IsFertile(Male) || !IPDBreedableInterface::Execute_IsFertile(Female))
    {
        OutReason = TEXT("One or both parents are not fertile.");
        return false;
    }

    if (!UPangeaBreedingLibrary::ResolveEggActorClass(Female, CurrentFarm->EggClass))
    {
        OutReason = TEXT("No egg class configured for this species.");
        return false;
    }

    return true;
}

void UPangeaBreedingFarmWidget::TryBreedUI()
{
    if (!CurrentFarm)
    {
        UE_LOG(LogTemp, Warning, TEXT("UBreedingFarmWidget::TryBreed — No CurrentFarm set"));
        return;
    }

    FString FailureReason;
    if (!ValidateParentPair(SelectedMale, SelectedFemale, FailureReason))
    {
        if (UANSUIPlayerSubsystem* UI = GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>())
            UI->BroadcastNotification(FText::FromString(FailureReason), 3.f);
        UE_LOG(LogTemp, Warning, TEXT("UBreedingFarmWidget::ValidateParentPair: %s"), *FailureReason);
        return;
    }

    APangeaEggActor* OutEgg = CurrentFarm->TryBreed(SelectedMale, SelectedFemale);
    if (OutEgg)
    {
        if (UANSUIPlayerSubsystem* UI = GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>())
            UI->BroadcastNotification(NSLOCTEXT("PangeaUI", "EggCreated", "Breeding successful. An egg is incubating."), 3.f);
        SelectedMale = nullptr;
        SelectedFemale = nullptr;
        UE_LOG(LogTemp, Log, TEXT("Breeding successful: Egg spawned"));
        ClearSelection();
    }
    else
    {
        if (UANSUIPlayerSubsystem* UI = GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>())
            UI->BroadcastNotification(NSLOCTEXT("PangeaUI", "BreedingFailed", "Breeding could not start. Check that both parents are inside the farm and ready."), 3.f);
        UE_LOG(LogTemp, Warning, TEXT("Breeding attempt failed"));
    }
}





