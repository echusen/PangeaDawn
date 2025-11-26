#include "UI/VillageUpgradeMenuWidget.h"

#include "Actors/VillageBase.h"
#include "Components/UpgradeSystemComponent.h"
#include "Objects/UpgradeRequirement.h"

#include "UI/RequirementEntryWidget.h"
#include "UI/FacilityUnlockEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "DataAssets/VillageDefinitionData.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UVillageUpgradeMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	UE_LOG(LogTemp, Error, TEXT("UpgradeMenu: NativeOnInitialized FIRED"));

	UE_LOG(LogTemp, Error, TEXT("UpgradeMenu: Button ptr = %s"), *GetNameSafe(UpgradeButton));
	UE_LOG(LogTemp, Error, TEXT("UpgradeMenu: ReqScroll ptr = %s"), *GetNameSafe(RequirementsScroll));


	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.AddDynamic(this, &UVillageUpgradeMenuWidget::OnUpgradeButtonClicked);
	}
}

void UVillageUpgradeMenuWidget::InitializeFromVillage(AVillageBase* InVillage, APawn* InInteractingPawn)
{
	UE_LOG(LogTemp, Warning, TEXT("InitializeFromVillage called"));
	UE_LOG(LogTemp, Warning, TEXT("Village = %s"), *GetNameSafe(InVillage));
	UE_LOG(LogTemp, Warning, TEXT("Pawn = %s"), *GetNameSafe(InInteractingPawn));
	
	VillageActor = InVillage;
	InteractingPawn = InInteractingPawn;

	RefreshUI();
}

UUpgradeSystemComponent* UVillageUpgradeMenuWidget::GetUpgradeSystem() const
{
	if (!VillageActor)
		return nullptr;

	// Either you have a cached pointer, or use FindComponentByClass
	return VillageActor->FindComponentByClass<UUpgradeSystemComponent>();
}

FText UVillageUpgradeMenuWidget::GetFacilityDisplayName(const FGameplayTag& FacilityTag) const
{
    if (!VillageActor)
        return FText::FromString(FacilityTag.ToString());

    const UVillageDefinitionData* Def = VillageActor->GetUpgradeSystem()->VillageDefinition;
    if (!Def)
        return FText::FromString(FacilityTag.ToString());

    for (const FFacilityGroupReference& Ref : Def->FacilityGroups)
    {
        if (Ref.FacilityTag == FacilityTag)
        {
            return Ref.FacilityGroupName;
        }
    }

    // fallback
    return FText::FromString(FacilityTag.ToString());
}

void UVillageUpgradeMenuWidget::RefreshUI()
{
    UE_LOG(LogTemp, Warning, TEXT("=== RefreshUI() START ==="));

    UE_LOG(LogTemp, Warning, TEXT("VillageActor = %s"), *GetNameSafe(VillageActor));
    UE_LOG(LogTemp, Warning, TEXT("InteractingPawn = %s"), *GetNameSafe(InteractingPawn));

    UUpgradeSystemComponent* UpgradeSystem = GetUpgradeSystem();
    UE_LOG(LogTemp, Warning, TEXT("UpgradeSystem = %s"), *GetNameSafe(UpgradeSystem));

    if (!UpgradeSystem)
    {
        UE_LOG(LogTemp, Error, TEXT("RefreshUI ABORT — UpgradeSystem is NULL"));
        UE_LOG(LogTemp, Warning, TEXT("=== RefreshUI() END ==="));
        return;
    }

    // ---------------- Current / Next Level ----------------
    if (CurrentLevelText)
    {
        UE_LOG(LogTemp, Warning, TEXT("CurrentLevelText VALID"));
    }
    else
        UE_LOG(LogTemp, Error, TEXT("CurrentLevelText NULL"));

    if (NextLevelText)
    {
        UE_LOG(LogTemp, Warning, TEXT("NextLevelText VALID"));
    }
    else
        UE_LOG(LogTemp, Error, TEXT("NextLevelText NULL"));

    // Show current level
    if (CurrentLevelText)
        CurrentLevelText->SetText(FText::AsNumber(UpgradeSystem->CurrentLevel));

    // Get next level
    FUpgradeLevelDefinition NextLevelDef;
    const bool bHasNextLevel = UpgradeSystem->GetNextLevelDefinition(NextLevelDef);

    UE_LOG(LogTemp, Warning, TEXT("bHasNextLevel = %s"), bHasNextLevel ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Warning, TEXT("NextLevelDef.Level = %d"), NextLevelDef.Level);
    UE_LOG(LogTemp, Warning, TEXT("NextLevelDef.Milestones = %d"), NextLevelDef.Milestones.Num());

    if (NextLevelText)
    {
        if (bHasNextLevel)
            NextLevelText->SetText(FText::AsNumber(NextLevelDef.Level));
        else
            NextLevelText->SetText(FText::FromString(TEXT("MAX")));
    }

    // ---------------- Requirements List ----------------
    UE_LOG(LogTemp, Warning, TEXT("=== Building Requirements List ==="));

    if (!RequirementsScroll)
    {
        UE_LOG(LogTemp, Error, TEXT("RequirementsScroll is NULL"));
    }
    else
    {
        RequirementsScroll->ClearChildren();
        UE_LOG(LogTemp, Warning, TEXT("RequirementsScroll cleared"));
    }

    if (!RequirementEntryClass)
    {
        UE_LOG(LogTemp, Error, TEXT("RequirementEntryClass is NULL (NOT SET IN BP!)"));
    }

    int32 AddedRequirementCount = 0;

    if (RequirementsScroll && bHasNextLevel && RequirementEntryClass)
    {
        // Unmet list
        TArray<UUpgradeRequirement*> Unmet;
        UpgradeSystem->GetUnmetRequirementsForNextLevel(InteractingPawn, Unmet);
        UE_LOG(LogTemp, Warning, TEXT("Unmet requirement count: %d"), Unmet.Num());

        // Loop through milestones
        for (const FUpgradeMilestoneDefinition& Milestone : NextLevelDef.Milestones)
        {
            UE_LOG(LogTemp, Warning, TEXT("Milestone Tag: %s — Requirements: %d"),
                   *Milestone.MilestoneTag.ToString(), Milestone.Requirements.Num());

            for (UUpgradeRequirement* Req : Milestone.Requirements)
            {
                if (!Req)
                {
                    UE_LOG(LogTemp, Error, TEXT("Null requirement detected — skipping"));
                    continue;
                }

                bool bMet = !Unmet.Contains(Req);
                UE_LOG(LogTemp, Warning, TEXT("Requirement '%s' Met = %s"),
                       *GetNameSafe(Req),
                       bMet ? TEXT("TRUE") : TEXT("FALSE"));

                URequirementEntryWidget* Entry = CreateWidget<URequirementEntryWidget>(this, RequirementEntryClass);

                if (!Entry)
                {
                    UE_LOG(LogTemp, Error, TEXT("FAILED to create RequirementEntryWidget!"));
                    continue;
                }

                Entry->InitFromRequirement(Req, bMet);
                RequirementsScroll->AddChild(Entry);

                AddedRequirementCount++;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Total Requirements Added: %d"), AddedRequirementCount);

    // ---------------- Facilities Unlock List ----------------
    UE_LOG(LogTemp, Warning, TEXT("=== Building Facility Unlock List ==="));

    if (!UnlocksScroll)
    {
        UE_LOG(LogTemp, Error, TEXT("UnlocksScroll is NULL"));
    }
    else
    {
        UnlocksScroll->ClearChildren();
        UE_LOG(LogTemp, Warning, TEXT("UnlocksScroll cleared"));
    }

    if (!FacilityUnlockEntryClass)
    {
        UE_LOG(LogTemp, Error, TEXT("FacilityUnlockEntryClass is NULL (NOT SET IN BP!)"));
    }

    int32 AddedFacilities = 0;

    if (UnlocksScroll && FacilityUnlockEntryClass && bHasNextLevel)
    {
        TArray<FGameplayTag> Facilities;
        UpgradeSystem->GetFacilitiesUnlockedAtLevel(NextLevelDef.Level, Facilities);

        UE_LOG(LogTemp, Warning, TEXT("Facilities unlocked at next level: %d"), Facilities.Num());

        for (const FGameplayTag& Tag : Facilities)
        {
            UE_LOG(LogTemp, Warning, TEXT("FacilityTag = %s"), *Tag.ToString());

            if (!Tag.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid facility tag — skipping"));
                continue;
            }

            UFacilityUnlockEntryWidget* Entry =
                CreateWidget<UFacilityUnlockEntryWidget>(this, FacilityUnlockEntryClass);

            if (!Entry)
            {
                UE_LOG(LogTemp, Error, TEXT("FAILED to create FacilityUnlockEntryWidget!"));
                continue;
            }

            const FText DisplayName = GetFacilityDisplayName(Tag);
            Entry->InitFromFacilityDisplayName(Tag, DisplayName);
            UnlocksScroll->AddChild(Entry);
            AddedFacilities++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Total Facilities Added: %d"), AddedFacilities);

    // ---------------- Upgrade Button ----------------
    if (UpgradeButton)
    {
        bool bCanUpgrade = UpgradeSystem->CanUpgradeToNextLevel(InteractingPawn);
        UpgradeButton->SetIsEnabled(bCanUpgrade);

        UE_LOG(LogTemp, Warning, TEXT("UpgradeButton Enabled = %s"), 
               bCanUpgrade ? TEXT("TRUE") : TEXT("FALSE"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UpgradeButton is NULL"));
    }

    UE_LOG(LogTemp, Warning, TEXT("=== RefreshUI() END ==="));
}

void UVillageUpgradeMenuWidget::OnUpgradeButtonClicked()
{
	
    if (!VillageActor || !InteractingPawn)
        return;

    const bool bSuccess = VillageActor->UpgradeBase(InteractingPawn);

    if (bSuccess)
    {
        // Auto-close UI
        RemoveFromParent();

        if (APlayerController* PC = Cast<APlayerController>(InteractingPawn->GetController()))
        {
            PC->bShowMouseCursor = false;
            PC->SetInputMode(FInputModeGameOnly());
        }
    }
}