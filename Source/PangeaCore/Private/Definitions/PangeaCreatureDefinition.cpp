#include "Definitions/PangeaCreatureDefinition.h"

#include "Definitions/PangeaDefinitionFragment.h"

#if WITH_EDITOR
#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"
#endif

UPangeaDefinitionFragment* UPangeaCreatureDefinition::GetFragmentByClass(TSubclassOf<UPangeaDefinitionFragment> FragmentClass) const
{
	if (!FragmentClass)
	{
		return nullptr;
	}

	for (UPangeaDefinitionFragment* Fragment : Fragments)
	{
		if (Fragment && Fragment->IsA(FragmentClass))
		{
			return Fragment;
		}
	}

	return nullptr;
}

#if WITH_EDITOR
EDataValidationResult UPangeaCreatureDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result != EDataValidationResult::Invalid) Result = EDataValidationResult::Valid;
	if (SpeciesId.IsNone())
	{
		Context.AddError(NSLOCTEXT("PangeaDefinition", "MissingSpecies", "A creature definition requires a Species Id."));
		Result = EDataValidationResult::Invalid;
	}
	UClass* SpawnClass = CreatureClass.IsNull() ? nullptr : CreatureClass.LoadSynchronous();
	if (!SpawnClass || !SpawnClass->IsChildOf(AActor::StaticClass()) || SpawnClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Context.AddError(NSLOCTEXT("PangeaDefinition", "InvalidClass", "Creature Class must resolve to a concrete, non-deprecated actor class."));
		Result = EDataValidationResult::Invalid;
	}
	TSet<UClass*> FragmentClasses;
	for (const UPangeaDefinitionFragment* Fragment : Fragments)
	{
		if (!Fragment)
		{
			Context.AddError(NSLOCTEXT("PangeaDefinition", "NullFragment", "Remove empty entries from the creature definition's Fragments array."));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (FragmentClasses.Contains(Fragment->GetClass()))
		{
			Context.AddError(FText::Format(NSLOCTEXT("PangeaDefinition", "DuplicateFragment", "Only one fragment of class {0} is supported."), FText::FromString(Fragment->GetClass()->GetName())));
			Result = EDataValidationResult::Invalid;
		}
		FragmentClasses.Add(Fragment->GetClass());
		if (Fragment->IsDataValid(Context) == EDataValidationResult::Invalid) Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
