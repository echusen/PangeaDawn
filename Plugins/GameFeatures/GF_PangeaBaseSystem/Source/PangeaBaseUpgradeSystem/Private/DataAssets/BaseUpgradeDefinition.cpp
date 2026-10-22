#include "DataAssets/BaseUpgradeDefinition.h"
#include "Definitions/BaseUpgradeFragment.h"
#include "Definitions/Fragments/BaseProgressionFragment.h"
#include "Objects/UpgradeAction.h"
#include "Objects/UpgradeRequirement.h"
#include "Objects/Requirements/Req_HasItems.h"
#include "Items/ACFItem.h"
#include "ACFItemSystemFunctionLibrary.h"
#include "Misc/DataValidation.h"

UBaseUpgradeFragment* UBaseUpgradeDefinition::GetFragmentByClass(TSubclassOf<UBaseUpgradeFragment> FragmentClass) const
{
    if (!FragmentClass) return nullptr;
    for (UBaseUpgradeFragment* Fragment : Fragments)
        if (Fragment && Fragment->IsA(FragmentClass)) return Fragment;
    return nullptr;
}

void UBaseUpgradeDefinition::GetConfigurationErrors(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    const auto Error = [&OutErrors](const FString& Message) { OutErrors.Add(FText::FromString(Message)); };
    TSet<UClass*> FragmentTypes;
    int32 Progressions = 0;
    for (const UBaseUpgradeFragment* Fragment : Fragments)
    {
        if (!Fragment) { Error(TEXT("Null base fragment.")); continue; }
        if (FragmentTypes.Contains(Fragment->GetClass())) Error(TEXT("Duplicate base fragment type: ") + Fragment->GetClass()->GetName());
        FragmentTypes.Add(Fragment->GetClass());
        if (Fragment->IsA<UBaseProgressionFragment>()) ++Progressions;
    }
    if (Progressions != 1) Error(TEXT("Exactly one progression fragment is required."));
    const UBaseProgressionFragment* Progression = GetFragment<UBaseProgressionFragment>();
    if (!Progression) return;
    TArray<int32> Levels;
    TSet<FGameplayTag> Milestones;
    for (const FUpgradeLevelDefinition& Level : Progression->Levels)
    {
        if (Level.Level <= 0 || Levels.Contains(Level.Level)) Error(TEXT("Level numbers must be positive and unique."));
        Levels.Add(Level.Level);
        TMap<TSubclassOf<UACFItem>, int64> CostTotals;
        for (const FPangeaUpgradeItemCost& Cost : Level.ItemCosts)
        {
            FItemDescriptor Data;
            if (!Cost.ItemClass || Cost.Count <= 0 || Cost.ItemClass->HasAnyClassFlags(CLASS_Abstract) ||
                !UACFItemSystemFunctionLibrary::GetItemData(Cost.ItemClass, Data) || Data.MaxInventoryStack <= 0 ||
                !FMath::IsFinite(Data.ItemWeight) || Data.ItemWeight < 0.f)
            { Error(TEXT("Explicit costs require valid concrete ACF items and positive counts.")); continue; }
            CostTotals.FindOrAdd(Cost.ItemClass) += Cost.Count;
            if (CostTotals[Cost.ItemClass] > MAX_int32) Error(TEXT("Aggregated cost count overflow."));
        }
        for (const FUpgradeMilestoneDefinition& Milestone : Level.Milestones)
        {
            if (!Milestone.MilestoneTag.IsValid() || Milestones.Contains(Milestone.MilestoneTag)) Error(TEXT("Milestone tags must be valid and globally unique."));
            Milestones.Add(Milestone.MilestoneTag);
            TSet<FString> Identities;
            for (const UUpgradeRequirement* Requirement : Milestone.Requirements)
            {
                if (!Requirement) { Error(TEXT("Null upgrade requirement.")); continue; }
                if (const UReq_HasItems* Gate = Cast<UReq_HasItems>(Requirement))
                {
                    TSet<UClass*> Classes;
                    for (const FRequiredACFItem& Item : Gate->RequiredItems)
                    {
                        if (!Item.ItemClass || Item.Count <= 0 || Classes.Contains(Item.ItemClass.Get()))
                            Error(TEXT("Ownership requirements need valid item classes, positive counts and one entry per class (not payment)."));
                        Classes.Add(Item.ItemClass.Get());
                    }
                }
            }
            for (const UUpgradeAction* Action : Milestone.Actions)
            {
                if (!Action) { Error(TEXT("Null upgrade action.")); continue; }
                if (!Action->IsIn(this)) Error(TEXT("Upgrade actions must be inline objects owned by this definition."));
                const FString Identity = Action->ActionId.IsNone() ? Action->GetPathName(this) : Action->ActionId.ToString();
                if (Identities.Contains(Identity)) Error(TEXT("Duplicate action identity in milestone: ") + Identity);
                Identities.Add(Identity);
                FText Reason;
                if (!Action->ValidateConfiguration(Reason)) Error(Action->GetName() + TEXT(": ") + Reason.ToString());
                const UFunction* Legacy = Action->GetClass()->FindFunctionByName(TEXT("Execute"));
                const UFunction* Checked = Action->GetClass()->FindFunctionByName(TEXT("TryExecuteUpgrade"));
                if (Legacy && Legacy->Script.Num() > 0 && (!Checked || Checked->Script.Num() == 0))
                    Error(Action->GetName() + TEXT(": migrate the legacy Blueprint Execute override to the checked contract."));
            }
        }
    }
    Levels.Sort();
    for (int32 Index = 0; Index < Levels.Num(); ++Index)
        if (Levels[Index] != Index + 1) { Error(TEXT("Progression must contain consecutive levels starting at 1.")); break; }
    if (Levels.IsEmpty()) Error(TEXT("Progression has no levels."));
}

#if WITH_EDITOR
EDataValidationResult UBaseUpgradeDefinition::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    TArray<FText> Errors;
    GetConfigurationErrors(Errors);
    for (const FText& Error : Errors) Context.AddError(Error);
    return Errors.IsEmpty() && ParentResult != EDataValidationResult::Invalid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
