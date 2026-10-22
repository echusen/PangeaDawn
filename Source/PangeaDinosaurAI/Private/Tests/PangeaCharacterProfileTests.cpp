#include "Characters/PDDinosaurBase.h"
#include "Components/PangeaCharacterInitializerComponent.h"
#include "Components/PangeaGASStatisticsComponent.h"
#include "Data/ACFCharacterDataAsset.h"
#include "Definitions/PangeaACFProfileFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/DataValidation.h"

namespace
{
// Private test fixtures deliberately do not start a world or initialize GAS.
// Reflection here configures protected serialized inputs, not production behavior.
struct FProfileFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	APDDinosaurBase* Dinosaur = World ? World->SpawnActor<APDDinosaurBase>() : nullptr;
	UPangeaCharacterInitializerComponent* Initializer = Dinosaur
		? Cast<UPangeaCharacterInitializerComponent>(Dinosaur->GetInitializerComponent()) : nullptr;
	UPangeaCreatureDefinition* Definition = NewObject<UPangeaCreatureDefinition>();
	UACFCharacterDataAsset* DefaultProfile = NewObject<UACFCharacterDataAsset>(Definition);

	FProfileFixture()
	{
		Definition->SpeciesId = TEXT("ProfileTest");
		Definition->CreatureClass = APDDinosaurBase::StaticClass();
		DefaultProfile->MeshComponents.Reset();
		UPangeaACFProfileFragment* Fragment = NewObject<UPangeaACFProfileFragment>(Definition);
		Fragment->CharacterProfile = DefaultProfile;
		Definition->Fragments.Add(Fragment);
		if (Dinosaur)
			if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Dinosaur->GetClass(), TEXT("CreatureDefinition")))
				Property->SetObjectPropertyValue_InContainer(Dinosaur, Definition);
	}

	~FProfileFixture() { if (World) World->DestroyWorld(false); }

	bool SetInitializerInput(const FName Name, UACFCharacterDataAsset* Profile) const
	{
		FObjectPropertyBase* Property = Initializer ? FindFProperty<FObjectPropertyBase>(Initializer->GetClass(), Name) : nullptr;
		if (!Property) return false;
		Property->SetObjectPropertyValue_InContainer(Initializer, Profile);
		return true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaProfileSelectionTest, "Pangea.ACF.Profile.SelectionAndExplicitOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaProfileSelectionTest::RunTest(const FString& Parameters)
{
	const APDDinosaurBase* CDO = GetDefault<APDDinosaurBase>();
	TestNotNull(TEXT("Native dinosaur replaces the existing initializer subobject with the Pangea adapter"),
		Cast<UPangeaCharacterInitializerComponent>(CDO->GetInitializerComponent()));
	TestNotNull(TEXT("Native dinosaur uses the profile-aware statistics component"), CDO->FindComponentByClass<UPangeaGASStatisticsComponent>());
	TestEqual(TEXT("Initializer keeps its inherited subobject identity"), CDO->GetInitializerComponent()->GetFName(), FName(TEXT("Character Initializer")));
	if (const UPangeaGASStatisticsComponent* Statistics = CDO->FindComponentByClass<UPangeaGASStatisticsComponent>())
		TestEqual(TEXT("Statistics retain the save-record lookup name"), Statistics->GetFName(), FName(TEXT("Statistic Component")));
	FProfileFixture Fixture;
	if (!TestNotNull(TEXT("Fixture initializer"), Fixture.Initializer)) return false;
	TestTrue(TEXT("Definition supplies the default"), Fixture.Initializer->GetEffectiveCharacterProfile() == Fixture.DefaultProfile);
	TestTrue(TEXT("Default provenance"), Fixture.Initializer->GetCharacterProfileSource() == EPangeaCharacterProfileSource::DefinitionDefault);
	TestNull(TEXT("Queries do not write the auto-init slot"), Fixture.Initializer->GetAutoInitDataAsset());
	TestNull(TEXT("Queries do not initialize or mutate applied profile"), Fixture.Initializer->GetCharacterDataAsset());
	TestFalse(TEXT("Selection is not readiness"), Fixture.Initializer->IsCharacterProfileInitialized());

	UACFCharacterDataAsset* Explicit = NewObject<UACFCharacterDataAsset>(Fixture.Definition);
	if (!TestTrue(TEXT("Configure explicit auto-init input"), Fixture.SetInitializerInput(TEXT("CharacterInitDataAsset"), Explicit))) return false;
	TestTrue(TEXT("Explicit auto-init beats definition default"), Fixture.Initializer->GetEffectiveCharacterProfile() == Explicit);
	TestTrue(TEXT("Explicit provenance"), Fixture.Initializer->GetCharacterProfileSource() == EPangeaCharacterProfileSource::ExplicitOverride);
	UACFCharacterDataAsset* Applied = NewObject<UACFCharacterDataAsset>(Fixture.Definition);
	if (!TestTrue(TEXT("Configure applied/spawner input"), Fixture.SetInitializerInput(TEXT("CharacterDataAsset"), Applied))) return false;
	TestTrue(TEXT("Applied override beats both auto-init and definition"), Fixture.Initializer->GetEffectiveCharacterProfile() == Applied);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaProfileLifecycleTest, "Pangea.ACF.Profile.PreparesBeforeACFAndObservesCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaProfileLifecycleTest::RunTest(const FString& Parameters)
{
	FProfileFixture Fixture;
	if (!TestNotNull(TEXT("Fixture initializer"), Fixture.Initializer)) return false;
	// Dispatch the public component lifecycle through UActorComponent. No BeginPlay,
	// stat initialization or ability grant is requested by the adapter itself.
	static_cast<UActorComponent*>(Fixture.Initializer)->InitializeComponent();
	TestTrue(TEXT("Default is supplied before ACF appearance initialization"), Fixture.Initializer->GetAutoInitDataAsset() == Fixture.DefaultProfile);
	TestTrue(TEXT("ACF InitializeComponent sees the selected profile"), Fixture.Initializer->GetCharacterDataAsset() == Fixture.DefaultProfile);
	TestTrue(TEXT("Selecting the default does not masquerade as an explicit override"), Fixture.Initializer->GetCharacterProfileSource() == EPangeaCharacterProfileSource::DefinitionDefault);
	TestFalse(TEXT("Appearance preparation alone is not gameplay readiness"), Fixture.Initializer->IsCharacterProfileInitialized());
	// Tests only observation of ACF's completion signal; not a full GAS/save round trip.
	Fixture.Initializer->OnInitializationCompleted.Broadcast(Fixture.DefaultProfile);
	TestTrue(TEXT("Adapter observes the existing completion signal"), Fixture.Initializer->IsCharacterProfileInitialized());
	TestTrue(TEXT("Repeated read-only queries preserve the selection"), Fixture.Initializer->GetEffectiveCharacterProfile() == Fixture.DefaultProfile);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaProfileAmbiguityTest, "Pangea.ACF.Profile.RejectsAmbiguousDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaProfileAmbiguityTest::RunTest(const FString& Parameters)
{
	FProfileFixture Fixture;
	if (!TestNotNull(TEXT("Fixture initializer"), Fixture.Initializer)) return false;
	Fixture.Definition->Fragments.Add(NewObject<UPangeaACFProfileFragment>(Fixture.Definition));
	TestNull(TEXT("Multiple profile fragments never choose an arbitrary default"), Fixture.Initializer->GetEffectiveCharacterProfile());
	TestTrue(TEXT("Ambiguous source is unconfigured"), Fixture.Initializer->GetCharacterProfileSource() == EPangeaCharacterProfileSource::Unconfigured);
	Fixture.Definition->Fragments.Reset();
	TestNull(TEXT("Unmigrated definitions remain unconfigured"), Fixture.Initializer->GetEffectiveCharacterProfile());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaProfileValidationTest, "Pangea.ACF.Profile.ValidatesDefinitionsAndProfileReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaProfileValidationTest::RunTest(const FString& Parameters)
{
	UPangeaCreatureDefinition* Definition = NewObject<UPangeaCreatureDefinition>();
	FDataValidationContext EmptyContext;
	TestTrue(TEXT("Missing identity/class is invalid"), Definition->IsDataValid(EmptyContext) == EDataValidationResult::Invalid);
	Definition->SpeciesId = TEXT("ValidationTest");
	Definition->CreatureClass = APDDinosaurBase::StaticClass();
	FDataValidationContext LegacyContext;
	TestTrue(TEXT("Legacy definitions do not require an ACF fragment"), Definition->IsDataValid(LegacyContext) == EDataValidationResult::Valid);

	UPangeaACFProfileFragment* Fragment = NewObject<UPangeaACFProfileFragment>(Definition);
	Definition->Fragments.Add(Fragment);
	FDataValidationContext MissingProfileContext;
	TestTrue(TEXT("Present but unassigned profile fragment is invalid"), Definition->IsDataValid(MissingProfileContext) == EDataValidationResult::Invalid);
	Fragment->CharacterProfile = NewObject<UACFCharacterDataAsset>(Definition);
	FDataValidationContext MissingTeamContext;
	TestTrue(TEXT("Profile requires an explicit valid team"), Definition->IsDataValid(MissingTeamContext) == EDataValidationResult::Invalid);
	Fragment->CharacterProfile->Team = FGameplayTag::RequestGameplayTag(TEXT("Teams.Dinosaurs"));
	FDataValidationContext ValidContext;
	TestTrue(TEXT("Profile can intentionally use the shared Blueprint's StartingEffects"), Definition->IsDataValid(ValidContext) == EDataValidationResult::Valid);
	Fragment->CharacterProfile->CapsuleRadius = 0.f;
	FDataValidationContext InvalidCapsuleContext;
	TestTrue(TEXT("Zero-radius capsule is invalid"), Definition->IsDataValid(InvalidCapsuleContext) == EDataValidationResult::Invalid);
	Fragment->CharacterProfile->CapsuleRadius = 34.f;
	Fragment->CharacterProfile->CharacterRow.RowName = TEXT("MissingTable");
	FDataValidationContext BrokenRowContext;
	TestTrue(TEXT("Dangling row name is invalid"), Definition->IsDataValid(BrokenRowContext) == EDataValidationResult::Invalid);
	Fragment->CharacterProfile->CharacterRow.RowName = NAME_None;
	Definition->Fragments.Add(NewObject<UPangeaACFProfileFragment>(Definition));
	FDataValidationContext DuplicateContext;
	TestTrue(TEXT("Duplicate profile fragments are invalid"), Definition->IsDataValid(DuplicateContext) == EDataValidationResult::Invalid);
	Definition->Fragments.Reset();
	Definition->Fragments.Add(nullptr);
	FDataValidationContext NullContext;
	TestTrue(TEXT("Null fragments are invalid"), Definition->IsDataValid(NullContext) == EDataValidationResult::Invalid);
	return !HasAnyErrors();
}
#endif
