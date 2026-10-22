#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actors/PangeaEggActor.h"
#include "Components/PangeaBreedableComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "ACFStatisticsSet.h"
#include "ALSFunctionLibrary.h"
#include "ALSSaveTypes.h"
#include "Actors/ACFCharacter.h"
#include "Data/ACFCharacterInitializerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Objects/PangeaGeneticStrategy.h"

namespace
{
UWorld* FindHatchingTestWorld()
{
	if (GEngine)
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() != NM_Client)
				return Context.World();
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaHatchVisualSmokeTest, "Pangea.ACF.Breeding.HatchedChildProfileAndPersistenceInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaHatchVisualSmokeTest::RunTest(const FString& Parameters)
{
	if (Parameters != TEXT("PIEChecks"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0]() mutable
		{
			if (!FindHatchingTestWorld())
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
	UWorld* World = FindHatchingTestWorld();
	if (!TestNotNull(TEXT("Authoritative PIE world"), World)) return false;
	const UPangeaCreatureDefinition* Asset = LoadObject<UPangeaCreatureDefinition>(nullptr,
		TEXT("/Game/_Game/Characters/Dinosaurs/Triceratops/CD_Triceratops.CD_Triceratops"));
	if (!TestNotNull(TEXT("Adult definition"), Asset)) return false;
	// Only the transient fixture disables mutation. Never modify authored balancing/genetics.
	UPangeaCreatureDefinition* Fixture = DuplicateObject<UPangeaCreatureDefinition>(Asset, GetTransientPackage());
	UPangeaBreedingFragment* Breeding = const_cast<UPangeaBreedingFragment*>(Fixture->GetFragment<UPangeaBreedingFragment>());
	if (!TestNotNull(TEXT("Adult breeding fragment"), Breeding) || !TestNotNull(TEXT("Offspring definition"), Breeding->OffspringCreatureDefinition.Get())) return false;
	Breeding->VisualMutationChance = 0.f;
	for (FPangeaInheritedStatRule& Rule : Breeding->InheritedStatRules) Rule.MutationChance = 0.f;
	FParentSnapshot ParentA, ParentB;
	ParentA.SpeciesID = ParentB.SpeciesID = Fixture->SpeciesId;
	ParentA.CreatureId = FGuid::NewGuid();
	ParentB.CreatureId = FGuid::NewGuid();
	ParentA.Traits.SetValue(TEXT("Weight"), 20.f);
	ParentB.Traits.SetValue(TEXT("Weight"), 40.f);
	const FLinearColor InheritedColor(0.2f, 0.4f, 0.6f, 1.f);
	for (const FMaterialColorGeneticGroup& Group : Breeding->MaterialColorGroups)
		for (const FName Parameter : Group.ParameterNames)
		{
			ParentA.MaterialParams.Add(Parameter, InheritedColor);
			ParentB.MaterialParams.Add(Parameter, InheritedColor);
		}
	for (const FPangeaInheritedStatRule& Rule : Breeding->InheritedStatRules)
	{
		// The pilot's authored rules are current statistics, not primary/secondary attributes.
		TestTrue(TEXT("Pilot inheritance rule uses a statistic"), Rule.StatType == EPangeaInheritedStatType::Statistic);
		ParentA.InheritedStatistics.Add(Rule.StatTag, 30.f);
		ParentB.InheritedStatistics.Add(Rule.StatTag, 30.f);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APangeaEggActor* Egg = World->SpawnActor<APangeaEggActor>(Breeding->EggActorClass,
		FVector(0, 0, 5000), FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Configured egg Blueprint spawns"), Egg)) return false;
	AActor* Child = nullptr;
	ON_SCOPE_EXIT
	{
		if (IsValid(Child)) Child->Destroy();
		if (IsValid(Egg) && !Egg->IsActorBeingDestroyed()) Egg->Destroy();
	};
	Egg->InitializeEgg(ParentA, ParentB, NewObject<UPangeaGeneticStrategy>(Egg), Fixture);
	const float CachedTrait = Egg->ChildTraits.GetValue(TEXT("Weight"));
	const ECreatureGender CachedGender = Egg->ChildGender;
	// Exercise the actual hatch path immediately; this smoke test does not time incubation.
	Child = Egg->Hatch();
	if (!TestNotNull(TEXT("Hatched child"), Child)) return false;
	TestTrue(TEXT("Egg consumed by hatching"), Egg->IsActorBeingDestroyed());
	TestNull(TEXT("Consumed egg cannot produce a second child"), Egg->Hatch());
	TestEqual(TEXT("Uses configured child class, not the adult"), Child->GetClass(), Breeding->OffspringCreatureDefinition->CreatureClass.LoadSynchronous());
	UPangeaBreedableComponent* Breedable = Child->FindComponentByClass<UPangeaBreedableComponent>();
	if (TestNotNull(TEXT("Child has the breeding feature component"), Breedable))
	{
		TestEqual(TEXT("Generated inherited profile reaches child"), Breedable->InheritedStatProfile.Values.Num(), Breeding->InheritedStatRules.Num());
		for (const FPangeaInheritedStatValue& Value : Breedable->InheritedStatProfile.Values)
			TestEqual(TEXT("Mutation-free equal parents generate their shared value"), Value.Value, 30.f);
	}
	USkeletalMeshComponent* Mesh = Child->FindComponentByClass<USkeletalMeshComponent>();
	if (TestNotNull(TEXT("Child skeletal mesh"), Mesh))
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Breeding->InheritanceMaterialSlot);
		if (TestNotNull(TEXT("Inherited material slot"), Material))
			for (const TPair<FName, FLinearColor>& Parameter : ParentA.MaterialParams)
			{
				FLinearColor Actual;
				TestTrue(TEXT("Inherited color parameter exists"), Material->GetVectorParameterValue(Parameter.Key, Actual));
				TestTrue(Parameter.Key.ToString() + TEXT(" preserves mutation-free parent color"), Actual.Equals(InheritedColor));
			}
	}
	const IPDDefinitionProviderInterface* Provider = Cast<IPDDefinitionProviderInterface>(Child);
	const UPangeaCreatureDefinition* ChildDefinition = Provider ? Provider->GetCreatureDefinition_Implementation() : nullptr;
	TestTrue(TEXT("Child owns the selected offspring definition"), ChildDefinition == Breeding->OffspringCreatureDefinition);
	// Inheritance must not require giving a juvenile the mother's reproduction configuration.
	if (TestNotNull(TEXT("Child definition"), ChildDefinition))
		TestNull(TEXT("No juvenile reproduction fragment needed"), ChildDefinition->GetFragment<UPangeaBreedingFragment>());
	AACFCharacter* Character = Cast<AACFCharacter>(Child);
	UAbilitySystemComponent* ASC = Child->FindComponentByClass<UAbilitySystemComponent>();
	if (!TestNotNull(TEXT("ACF child"), Character) || !TestNotNull(TEXT("Child ASC"), ASC) || !Breedable) return false;
	const FGuid ChildId = Breedable->GetCreatureId();
	TestTrue(TEXT("Hatchling has durable actor identity"), ChildId.IsValid());
	TestTrue(TEXT("Birth retains both lineage IDs"), Breedable->ParentAId == ParentA.CreatureId && Breedable->ParentBId == ParentB.CreatureId);
	TestEqual(TEXT("Hatch applies cached traits"), Breedable->GeneticTraits.GetValue(TEXT("Weight")), CachedTrait);
	TestTrue(TEXT("Hatch applies cached gender"), Breedable->Gender == CachedGender);
	TestNotNull(TEXT("Child has an applied ACF profile"), Character->GetInitializerComponent()->GetCharacterDataAsset());
	TestTrue(TEXT("Child mesh keeps authored scale"), Mesh && Mesh->GetRelativeScale3D().Equals(FVector(0.3)));
	TestTrue(TEXT("Child capsule, not adult capsule"), FMath::IsNearlyEqual(Character->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), 37.084999f, 0.001f));
	const UACFStatisticsSet* Stats = GetDefault<UACFStatisticsSet>();
	TestEqual(TEXT("Inherited health is a target, not base plus parent health"), ASC->GetNumericAttribute(Stats->MaxHealthAttribute()), 30.f);
	TestEqual(TEXT("Inherited equilibrium is a target"), ASC->GetNumericAttribute(Stats->MaxEquilibriumAttribute()), 30.f);
	const int32 EffectCount = ASC->GetActiveEffects(FGameplayEffectQuery()).Num();
	TestEqual(TEXT("Exactly one inherited effect plus dependencies and regen"), EffectCount, 3);
	Breedable->ApplyInheritedStatProfile();
	Breedable->ApplyInheritedStatProfile();
	TestEqual(TEXT("Repeated inheritance does not stack"), ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), EffectCount);
	TestEqual(TEXT("Repeated inheritance preserves target"), ASC->GetNumericAttribute(Stats->MaxHealthAttribute()), 30.f);
	ASC->SetNumericAttributeBase(Stats->HealthAttribute(), 13.f);
	Breedable->SetFertile_Implementation(false);
	Breedable->StartFertilityCooldown(4.5f);
	const FALSActorData Record = UALSFunctionLibrary::SerializeActor(Child);
	TestTrue(TEXT("Inherited application data participates in actor save"), Record.HasComponent(Breedable));
	// Restore into a NEW actor, not just an already-configured child. No mother/egg lookup.
	AActor* Restored = World->SpawnActor<AActor>(Child->GetClass(), FVector(0, 1000, 5000), FRotator::ZeroRotator, SpawnParameters);
	if (!TestNotNull(TEXT("Direct child spawn for restore"), Restored)) return false;
	ON_SCOPE_EXIT { if (IsValid(Restored)) Restored->Destroy(); };
	UAbilitySystemComponent* RestoredASC = Restored->FindComponentByClass<UAbilitySystemComponent>();
	if (!TestNotNull(TEXT("Restored ASC"), RestoredASC)) return false;
	UPangeaBreedableComponent* RestoredBreedable = Restored->FindComponentByClass<UPangeaBreedableComponent>();
	if (!TestNotNull(TEXT("Restored breedable"), RestoredBreedable)) return false;
	TestTrue(TEXT("Freshly spawned child starts with distinct identity"), RestoredBreedable->GetCreatureId() != ChildId);
	TestEqual(TEXT("Direct child uses corrected baseline before inherited save"), RestoredASC->GetNumericAttribute(Stats->MaxHealthAttribute()), 50.f);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		UALSFunctionLibrary::FullDeserializeActor(Restored, Record, false);
		TestEqual(TEXT("Saved inherited target restored independently"), RestoredASC->GetNumericAttribute(Stats->MaxHealthAttribute()), 30.f);
		TestTrue(TEXT("Saved damage is not ratio-scaled against baseline during restore"), FMath::IsNearlyEqual(RestoredASC->GetNumericAttribute(Stats->HealthAttribute()), 13.f));
		TestEqual(TEXT("Restore does not duplicate inherited effects"), RestoredASC->GetActiveEffects(FGameplayEffectQuery()).Num(), EffectCount);
		TestTrue(TEXT("Actor identity survives full ALS restore"), RestoredBreedable->GetCreatureId() == ChildId);
		TestTrue(TEXT("Lineage survives full ALS restore"), RestoredBreedable->ParentAId == ParentA.CreatureId && RestoredBreedable->ParentBId == ParentB.CreatureId);
		TestEqual(TEXT("Genetics survive full ALS restore"), RestoredBreedable->GeneticTraits.GetValue(TEXT("Weight")), CachedTrait);
		TestTrue(TEXT("Gender survives full ALS restore"), RestoredBreedable->Gender == CachedGender);
		TestFalse(TEXT("Explicit infertility survives full ALS restore"), RestoredBreedable->bIsFertile);
		TestTrue(TEXT("Cooldown survives full ALS restore"), FMath::IsNearlyEqual(RestoredBreedable->GetFertilityCooldownRemaining(), 4.5f));
		AACFCharacter* RestoredCharacter = Cast<AACFCharacter>(Restored);
		RestoredCharacter->GetInitializerComponent()->ApplyAppearanceFromDataAsset(RestoredCharacter->GetInitializerComponent()->GetCharacterDataAsset(), false);
		USkeletalMeshComponent* RestoredMesh = Restored->FindComponentByClass<USkeletalMeshComponent>();
		UMaterialInterface* RestoredMaterial = RestoredMesh ? RestoredMesh->GetMaterial(Breeding->InheritanceMaterialSlot) : nullptr;
		if (TestNotNull(TEXT("Restored material after ACF appearance refresh"), RestoredMaterial))
			for (const TPair<FName, FLinearColor>& Parameter : ParentA.MaterialParams)
			{
				FLinearColor Actual;
				TestTrue(TEXT("Saved color parameter exists after refresh"), RestoredMaterial->GetVectorParameterValue(Parameter.Key, Actual));
				TestTrue(TEXT("Saved appearance survives ACF profile refresh"), Actual.Equals(InheritedColor));
			}
	}
	return !HasAnyErrors();
}
#endif
