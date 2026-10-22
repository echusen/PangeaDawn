#include "Definitions/PangeaACFProfileFragment.h"

#if WITH_EDITOR
#include "Data/ACFCharacterDataAsset.h"
#include "Components/PangeaCharacterInitializerComponent.h"
#include "Components/PangeaGASStatisticsComponent.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "PangeaACFProfile"

EDataValidationResult UPangeaACFProfileFragment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result != EDataValidationResult::Invalid) Result = EDataValidationResult::Valid;

	const auto Error = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (const UPangeaCreatureDefinition* Definition = GetOwningDefinition())
	{
		int32 ProfileCount = 0;
		for (const UPangeaDefinitionFragment* Fragment : Definition->Fragments)
			if (Fragment && Fragment->IsA<UPangeaACFProfileFragment>()) ++ProfileCount;
		if (ProfileCount > 1)
			Error(LOCTEXT("DuplicateProfile", "A creature definition may contain only one ACF Character Profile fragment, including subclasses."));
		if (const UClass* SpawnClass = Definition->CreatureClass.LoadSynchronous())
		{
			const AActor* DefaultActor = Cast<AActor>(SpawnClass->GetDefaultObject());
			if (!DefaultActor || !Cast<IPDDefinitionProviderInterface>(DefaultActor) ||
				!DefaultActor->FindComponentByClass<UPangeaCharacterInitializerComponent>() ||
				!DefaultActor->FindComponentByClass<UPangeaGASStatisticsComponent>())
				Error(LOCTEXT("UnsupportedOwner", "Creature Class must expose a native definition provider and use the Pangea profile initializer/statistics components. Check inherited component types after rebuilding."));
		}
	}

	if (!CharacterProfile)
	{
		Error(LOCTEXT("MissingProfile", "Assign a Character Profile on the ACF Character Profile fragment."));
		return Result;
	}
	if (!CharacterProfile->Team.IsValid())
		Error(LOCTEXT("MissingTeam", "The ACF character profile must specify a valid team tag."));
	if (!FMath::IsFinite(CharacterProfile->CapsuleRadius) || !FMath::IsFinite(CharacterProfile->CapsuleHalfHeight) ||
		CharacterProfile->CapsuleRadius <= 0.f || CharacterProfile->CapsuleHalfHeight < CharacterProfile->CapsuleRadius)
		Error(LOCTEXT("InvalidCapsule", "The ACF profile capsule needs a positive radius and a finite half-height at least as large as its radius."));

	if (CharacterProfile->LevelingType == ELevelingType::EGenerateNewStatsFromCurves)
	{
		if (!CharacterProfile->AttributesByLevelCurve)
			Error(LOCTEXT("MissingCurve", "Curve-based ACF leveling requires Attributes By Level Curve."));
	}
	else if (CharacterProfile->CharacterRow.DataTable || !CharacterProfile->CharacterRow.RowName.IsNone())
	{
		const FDataTableRowHandle& Row = CharacterProfile->CharacterRow;
		if (!Row.DataTable || Row.RowName.IsNone() || !Row.DataTable->GetRowStruct() ||
			!Row.DataTable->GetRowStruct()->IsChildOf(FACFAttributeInits::StaticStruct()) || !Row.DataTable->FindRowUnchecked(Row.RowName))
			Error(LOCTEXT("InvalidStatRow", "The ACF profile Character Row must resolve to an existing ACFAttributeInits row; a row name without a table is not valid."));
	}
	else
	{
		// Empty is supported by ACF when the shared Blueprint initializes attributes through StartingEffects.
		Context.AddWarning(LOCTEXT("EffectBasedStats", "No ACF baseline attribute row is assigned. Verify that the shared character's Starting Effects intentionally initialize all required attributes."));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
