#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "AbilitySystemComponent.h"
#include "ACFGASTypes.h"
#include "ACFPrimaryAttributeSet.h"
#include "ACFStatisticsSet.h"
#include "ALSFunctionLibrary.h"
#include "ALSSaveTypes.h"
#include "Characters/PDDinosaurBase.h"
#include "Components/PangeaCharacterInitializerComponent.h"
#include "Components/PangeaGASStatisticsComponent.h"
#include "Data/ACFCharacterDataAsset.h"
#include "Definitions/PangeaACFProfileFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
const TCHAR* AdultDefinitionPath = TEXT("/Game/_Game/Characters/Dinosaurs/Triceratops/CD_Triceratops.CD_Triceratops");

UWorld* FindProfileTestWorld()
{
	if (GEngine)
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() != NM_Client)
				return Context.World();
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaAdultBalanceDataTest, "Pangea.ACF.Profile.AdultBalanceDataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaAdultBalanceDataTest::RunTest(const FString& Parameters)
{
	const UPangeaCreatureDefinition* Definition = LoadObject<UPangeaCreatureDefinition>(nullptr, AdultDefinitionPath);
	if (!TestNotNull(TEXT("Adult definition"), Definition)) return false;
	const UPangeaACFProfileFragment* Fragment = Definition->GetFragment<UPangeaACFProfileFragment>();
	if (!TestNotNull(TEXT("Adult profile fragment"), Fragment) || !TestNotNull(TEXT("Adult ACF profile"), Fragment->CharacterProfile.Get())) return false;
	const FDataTableRowHandle& Handle = Fragment->CharacterProfile->CharacterRow;
	TestEqual(TEXT("Species row"), Handle.RowName, FName(TEXT("Triceratops_Adult")));
	if (!TestNotNull(TEXT("Explicit primary-attribute table"), Handle.DataTable.Get())) return false;
	const FACFAttributeInits* Row = Handle.DataTable->FindRow<FACFAttributeInits>(Handle.RowName, TEXT("AdultBalanceDataContract"));
	if (!TestNotNull(TEXT("Species row resolves"), Row)) return false;
	// Derived maxima/damage/regen are still produced by the existing live dependency GE.
	// Copying those final values into this base-attribute row would double-count them.
	const TArray<FGameplayAttribute> Expected = {
		GetDefault<UACFPrimaryAttributeSet>()->StrengthAttribute(), GetDefault<UACFPrimaryAttributeSet>()->EnduranceAttribute(),
		GetDefault<UACFPrimaryAttributeSet>()->IntelligenceAttribute(), GetDefault<UACFPrimaryAttributeSet>()->ConstitutionAttribute(),
		GetDefault<UACFPrimaryAttributeSet>()->DexterityAttribute() };
	TestEqual(TEXT("Exactly five primary inputs"), Row->PawnAttributesInit.Num(), Expected.Num());
	for (const FGameplayAttribute& Attribute : Expected)
	{
		int32 Matches = 0;
		for (const FAttributeInit& Entry : Row->PawnAttributesInit)
			if (Entry.Attribute == Attribute)
			{
				++Matches;
				TestEqual(Attribute.GetName() + TEXT(" preserves the approved tier-zero input"), Entry.InitValue, 0.f);
			}
		TestEqual(Attribute.GetName() + TEXT(" occurs exactly once"), Matches, 1);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaDeferredDefinitionTest, "Pangea.ACF.Profile.DeferredDefinitionIsSpawnOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaDeferredDefinitionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	APDDinosaurBase* Dinosaur = World->SpawnActorDeferred<APDDinosaurBase>(APDDinosaurBase::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Deferred dinosaur"), Dinosaur)) return false;
	UPangeaCreatureDefinition* Definition = NewObject<UPangeaCreatureDefinition>();
	Definition->SpeciesId = TEXT("DeferredTest");
	Definition->CreatureClass = APDDinosaurBase::StaticClass();
	TestFalse(TEXT("Null definition rejected"), Dinosaur->SetCreatureDefinition_Implementation(nullptr));
	TestTrue(TEXT("Definition accepted before component initialization"), Dinosaur->SetCreatureDefinition_Implementation(Definition));
	TestTrue(TEXT("Native early getter sees selected definition"), Dinosaur->GetCreatureDefinition_Implementation() == Definition);
	Dinosaur->FinishSpawning(FTransform::Identity);
	// This fixture deliberately has no world BeginPlay. FinishSpawning alone does
	// not initialize components in that world; exercise the real rejection boundary.
	static_cast<UActorComponent*>(Dinosaur->GetInitializerComponent())->InitializeComponent();
	TestFalse(TEXT("Cannot reset definition after initialization"), Dinosaur->SetCreatureDefinition_Implementation(Definition));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaAdultRuntimeSaveTest, "Pangea.ACF.Profile.SpawnedAdultAndSaveRoundTripInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaAdultRuntimeSaveTest::RunTest(const FString& Parameters)
{
	if (Parameters != TEXT("PIEChecks"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0]() mutable
		{
			if (!FindProfileTestWorld())
			{
				if (FPlatformTime::Seconds() < Deadline) return false;
				AddError(TEXT("Timed out waiting for an authoritative PIE world"));
				return true;
			}
			if (ReadyAt == 0.0) { ReadyAt = FPlatformTime::Seconds() + 1.0; return false; }
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			RunTest(TEXT("PIEChecks"));
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		return true;
	}
	UWorld* World = FindProfileTestWorld();
	if (!TestNotNull(TEXT("Authoritative PIE world"), World)) return false;
	const UPangeaCreatureDefinition* Definition = LoadObject<UPangeaCreatureDefinition>(nullptr, AdultDefinitionPath);
	if (!TestNotNull(TEXT("Adult definition"), Definition)) return false;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APDDinosaurBase* Dinosaur = World->SpawnActor<APDDinosaurBase>(Definition->CreatureClass.LoadSynchronous(),
		FVector(0, 0, 5000), FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Spawned adult"), Dinosaur)) return false;
	ON_SCOPE_EXIT { if (IsValid(Dinosaur)) Dinosaur->Destroy(); };
	UPangeaCharacterInitializerComponent* Initializer = Cast<UPangeaCharacterInitializerComponent>(Dinosaur->GetInitializerComponent());
	UAbilitySystemComponent* ASC = Dinosaur->FindComponentByClass<UAbilitySystemComponent>();
	UPangeaGASStatisticsComponent* Statistics = Dinosaur->FindComponentByClass<UPangeaGASStatisticsComponent>();
	if (!TestNotNull(TEXT("Profile initializer"), Initializer) || !TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Statistics"), Statistics)) return false;
	TestTrue(TEXT("Real ACF completion observed on spawn"), Initializer->IsCharacterProfileInitialized());
	TestTrue(TEXT("Spawn used definition default"), Initializer->GetCharacterProfileSource() == EPangeaCharacterProfileSource::DefinitionDefault);
	TestEqual(TEXT("Health"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->HealthAttribute()), 50.f);
	TestEqual(TEXT("MaxHealth"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->MaxHealthAttribute()), 50.f);
	TestEqual(TEXT("Stamina"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->StaminaAttribute()), 10.f);
	TestEqual(TEXT("Mana"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->ManaAttribute()), 20.f);
	TestEqual(TEXT("Equilibrium"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->EquilibriumAttribute()), 10.f);
	const int32 EffectCount = ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
	const int32 AbilityCount = ASC->GetActivatableAbilities().Num();
	TestEqual(TEXT("One dependency effect and one regen effect"), EffectCount, 2);
	TestEqual(TEXT("Attack and flee abilities granted once"), AbilityCount, 2);

	// Exercise ACF's real actor/component archive and callbacks, without touching disk slots.
	// This is a current-format round trip, not proof of compatibility with historical .sav files.
	ASC->SetNumericAttributeBase(GetDefault<UACFStatisticsSet>()->HealthAttribute(), 37.f);
	const FALSActorData Saved = UALSFunctionLibrary::SerializeActor(Dinosaur);
	TestTrue(TEXT("Statistics component present in archive"), Saved.HasComponent(Statistics));
	if (const FALSComponentData* Record = Saved.GetComponentData(Statistics))
		TestTrue(TEXT("Statistics archive is non-empty"), !Record->Data.IsEmpty());
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		ASC->SetNumericAttributeBase(GetDefault<UACFStatisticsSet>()->HealthAttribute(), 9.f);
		UALSFunctionLibrary::FullDeserializeActor(Dinosaur, Saved, false);
		if (!TestTrue(TEXT("Load keeps the living dinosaur"), IsValid(Dinosaur) && !Dinosaur->IsActorBeingDestroyed())) return false;
		TestEqual(TEXT("Load restores saved damage, not full profile health"), ASC->GetNumericAttribute(GetDefault<UACFStatisticsSet>()->HealthAttribute()), 37.f);
		TestEqual(TEXT("Load does not duplicate permanent effects"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), EffectCount);
		TestEqual(TEXT("Load does not regrant abilities"), ASC->GetActivatableAbilities().Num(), AbilityCount);
	}
	return !HasAnyErrors();
}
#endif
