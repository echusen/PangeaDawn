#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Actors/PangeaEggActor.h"
#include "ALSFunctionLibrary.h"
#include "ALSSaveTypes.h"
#include "Components/PangeaBreedableComponent.h"
#include "Engine/World.h"
#include "Objects/PangeaGeneticStrategy.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/AutomationEditorCommon.h"
#include "TimerManager.h"
#include "UObject/CoreNet.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Interfaces/PDCreatureIdentityInterface.h"

namespace
{
// Private, isolated fixtures; never advance the live PIE world's clock to test offline behavior.
struct FBreedingPersistenceFixture
{
    // No player/input lifecycle is exercised here. Game worlds register Enhanced Input
    // preprocessors; across latent fixtures those can visit a deinitialized world awaiting GC.
    // A preview world still supplies actors/authority/TimerManager without game-only subsystems.
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    UPangeaBreedableComponent* Parent = nullptr;
    UPangeaBreedableComponent* Restored = nullptr;
    APangeaEggActor* Egg = nullptr;
    APangeaEggActor* RestoredEgg = nullptr;
    TArray<uint8> Data;
    FALSActorData EggRecord;
    float Remaining = 0.f;
    FGuid Id;
    int32 Step = 0;
    uint64 LastFrame = GFrameCounter;

    ~FBreedingPersistenceFixture() { if (World) World->DestroyWorld(false); }
    UPangeaBreedableComponent* AddParent()
    {
        AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
        if (!Owner) return nullptr;
        UPangeaBreedableComponent* Component = NewObject<UPangeaBreedableComponent>(Owner);
        Owner->AddInstanceComponent(Component);
        return Component;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaBreedingStateMetadataTest, "Pangea.ACF.Breeding.PersistentReplicationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaBreedingStateMetadataTest::RunTest(const FString& Parameters)
{
    const auto Check = [this](UStruct* Type, const TCHAR* Name, bool bNet)
    {
        const FProperty* Property = FindFProperty<FProperty>(Type, Name);
        if (!TestNotNull(FString(Type->GetName()) + TEXT(".") + Name, Property)) return;
        TestTrue(FString(Name) + TEXT(" is SaveGame"), Property->HasAnyPropertyFlags(CPF_SaveGame));
        if (bNet) TestTrue(FString(Name) + TEXT(" is replicated"), Property->HasAnyPropertyFlags(CPF_Net));
    };
    for (const TCHAR* Name : { TEXT("Gender"), TEXT("bIsFertile"), TEXT("bIsOnFertilityCooldown"), TEXT("FertilityCooldownRemaining"),
        TEXT("GeneticTraits"), TEXT("InheritedStatProfile"), TEXT("InheritedAppearance"), TEXT("ParentAId"), TEXT("ParentBId") })
        Check(UPangeaBreedableComponent::StaticClass(), Name, true);
    for (const TCHAR* Name : { TEXT("CreatureDefinition"), TEXT("ParentA"), TEXT("ParentB"), TEXT("ChildTraits"), TEXT("EggState"),
        TEXT("ChildGender"), TEXT("ChildStatProfile"), TEXT("ChildAppearance"), TEXT("IncubationRemaining"), TEXT("TotalIncubationTime") })
        Check(APangeaEggActor::StaticClass(), Name, true);
    for (const TCHAR* Name : { TEXT("SpeciesID"), TEXT("CreatureId"), TEXT("Traits"), TEXT("MaterialParams"), TEXT("ScalarParams"),
        TEXT("InheritedStatistics"), TEXT("InheritedPrimaryAttributes"), TEXT("InheritedAttributes") })
        Check(FParentSnapshot::StaticStruct(), Name, false);
    Check(FGeneticTraitSet::StaticStruct(), TEXT("Traits"), false);
    Check(FGeneticTrait::StaticStruct(), TEXT("Name"), false);
    Check(FGeneticTrait::StaticStruct(), TEXT("Value"), false);
    for (const TCHAR* Name : { TEXT("VisualData"), TEXT("MaterialParams"), TEXT("ScalarParams"),
        TEXT("InheritedStatistics"), TEXT("InheritedPrimaryAttributes"), TEXT("InheritedAttributes") })
    {
        Check(FParentSnapshot::StaticStruct(), Name, false);
        const FProperty* Property = FindFProperty<FProperty>(FParentSnapshot::StaticStruct(), Name);
        if (Property) TestTrue(FString(Name) + TEXT(" bypasses default member replication (native serializer sends it)"), Property->HasAnyPropertyFlags(CPF_RepSkip));
    }
    const UFunction* IdentityGetter = UPDCreatureIdentityInterface::StaticClass()->FindFunctionByName(TEXT("GetCreatureId"));
    if (TestNotNull(TEXT("Identity interface getter"), IdentityGetter))
        TestTrue(TEXT("Identity getter remains accessible to Blueprint"), IdentityGetter->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    TestTrue(TEXT("Snapshot supplies a native NetSerialize for map data"), FParentSnapshot::StaticStruct()->GetCppStructOps()->HasNetSerializer());
    TestTrue(TEXT("Snapshot selects the engine's native net-serialization path"), (FParentSnapshot::StaticStruct()->StructFlags & STRUCT_NetSerializeNative) != 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaSnapshotWireTest, "Pangea.ACF.Breeding.ParentSnapshotWireAndSaveRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaSnapshotWireTest::RunTest(const FString& Parameters)
{
    FParentSnapshot Original;
    Original.SpeciesID = TEXT("SnapshotTest");
    Original.CreatureId = FGuid::NewGuid();
    Original.Traits.SetValue(TEXT("Weight"), 37.5f);
    Original.MaterialParams.Add(TEXT("Skin"), FLinearColor(0.1f, 0.2f, 0.3f));
    Original.VisualData = Original.MaterialParams;
    Original.ScalarParams.Add(TEXT("Roughness"), 0.7f);
    const FGameplayTag Health = FGameplayTag::RequestGameplayTag(TEXT("RPG.Statistics.Health"));
    Original.InheritedStatistics.Add(Health, 30.f);
    Original.InheritedPrimaryAttributes.Add(Health, 12.f); // wire format accepts any registered tag, independent of rule validation
    Original.InheritedAttributes.Add(Health, 9.f);
    FNetBitWriter Writer(nullptr, 65536);
    const FStructProperty* ParentProperty = FindFProperty<FStructProperty>(APangeaEggActor::StaticClass(), TEXT("ParentA"));
    if (!TestNotNull(TEXT("Replicated parent snapshot property"), ParentProperty)) return false;
    // Exercise the actual reflected property path, not only a direct call to our serializer.
    bool bSuccess = ParentProperty->NetSerializeItem(Writer, nullptr, &Original, nullptr);
    if (!TestTrue(TEXT("Snapshot wire write"), bSuccess && !Writer.IsError())) return false;
    FParentSnapshot Wire;
    FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
    bSuccess = ParentProperty->NetSerializeItem(Reader, nullptr, &Wire, nullptr);
    TestTrue(TEXT("Snapshot wire read"), bSuccess && !Reader.IsError());
    const auto Check = [this, &Original, Health](const FParentSnapshot& Value)
    {
        TestEqual(TEXT("Species persists"), Value.SpeciesID, Original.SpeciesID);
        TestTrue(TEXT("Stable lineage GUID persists"), Value.CreatureId == Original.CreatureId);
        TestEqual(TEXT("Nested trait value persists"), Value.Traits.GetValue(TEXT("Weight")), 37.5f);
        TestTrue(TEXT("Color map persists"), Value.MaterialParams.FindRef(TEXT("Skin")).Equals(Original.MaterialParams.FindRef(TEXT("Skin"))));
        TestTrue(TEXT("Visual map persists despite default replication skip"), Value.VisualData.FindRef(TEXT("Skin")).Equals(Original.VisualData.FindRef(TEXT("Skin"))));
        TestEqual(TEXT("Scalar map persists"), Value.ScalarParams.FindRef(TEXT("Roughness")), 0.7f);
        TestEqual(TEXT("Statistics map persists"), Value.InheritedStatistics.FindRef(Health), 30.f);
        TestEqual(TEXT("Primary map persists"), Value.InheritedPrimaryAttributes.FindRef(Health), 12.f);
        TestEqual(TEXT("Secondary map persists"), Value.InheritedAttributes.FindRef(Health), 9.f);
    };
    Check(Wire);
    TArray<uint8> Bytes;
    FMemoryWriter MemoryWriter(Bytes);
    FALSSaveGameArchive Save(MemoryWriter, true);
    FParentSnapshot::StaticStruct()->SerializeItem(Save, &Original, nullptr);
    FParentSnapshot Saved;
    FMemoryReader MemoryReader(Bytes);
    FALSSaveGameArchive Load(MemoryReader, true);
    FParentSnapshot::StaticStruct()->SerializeItem(Load, &Saved, nullptr);
    TestFalse(TEXT("Snapshot archive read has no error"), Load.IsError());
    Check(Saved);
    Original.Traits.Traits.SetNum(257);
    FNetBitWriter Oversized(nullptr, 65536);
    Original.NetSerialize(Oversized, nullptr, bSuccess);
    TestFalse(TEXT("Unbounded snapshots are rejected"), bSuccess);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaCooldownPersistenceTest, "Pangea.ACF.Breeding.CooldownPausesOffline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaCooldownPersistenceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBreedingPersistenceFixture> Fixture = MakeShared<FBreedingPersistenceFixture>();
    if (!TestNotNull(TEXT("Isolated world"), Fixture->World)) return false;
    Fixture->Parent = Fixture->AddParent();
    if (!TestNotNull(TEXT("Authority parent"), Fixture->Parent)) return false;
    // Prime TimerManager before starting timers. Each latent step runs on a later frame.
    Fixture->World->GetTimerManager().Tick(0.f);
    Fixture->Parent->SetGender(ECreatureGender::Male);
    Fixture->Parent->GeneticTraits.SetValue(TEXT("Weight"), 42.f);
    Fixture->Parent->StartFertilityCooldown(4.f);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture]()
    {
        if (Fixture->LastFrame == GFrameCounter) return false;
        Fixture->LastFrame = GFrameCounter;
        FTimerManager& Timers = Fixture->World->GetTimerManager();
        if (Fixture->Step == 0)
        {
            Timers.Tick(1.25f);
            Fixture->Parent->OnComponentSaved();
            Fixture->Remaining = Fixture->Parent->GetFertilityCooldownRemaining();
            TestTrue(TEXT("Uses exact timer remaining, not interval subtraction"), FMath::IsNearlyEqual(Fixture->Remaining, 2.75f));
            Fixture->Id = Fixture->Parent->GetCreatureId();
            TestTrue(TEXT("Authority generates identity"), Fixture->Id.IsValid());
            TestTrue(TEXT("Snapshot queries do not reroll identity"), Fixture->Parent->BuildParentSnapshot().CreatureId == Fixture->Id && Fixture->Parent->BuildParentSnapshot().CreatureId == Fixture->Id);
            FMemoryWriter Writer(Fixture->Data);
            FALSSaveGameArchive Save(Writer, true);
            Fixture->Parent->Serialize(Save);
        }
        else if (Fixture->Step == 1)
        {
            Timers.Tick(100.f); // unrelated game time after the snapshot; not charged to the restored record
            Fixture->Restored = Fixture->AddParent();
            if (!TestNotNull(TEXT("Fresh restore component"), Fixture->Restored)) return true;
            FMemoryReader Reader(Fixture->Data);
            FALSSaveGameArchive Load(Reader, true);
            Fixture->Restored->Serialize(Load);
            Fixture->Restored->OnComponentLoaded();
            TestTrue(TEXT("Saved identity restored independently"), Fixture->Restored->GetCreatureId() == Fixture->Id);
            TestTrue(TEXT("Saved gender retained"), Fixture->Restored->Gender == ECreatureGender::Male);
            TestEqual(TEXT("Saved nested genetics retained"), Fixture->Restored->GeneticTraits.GetValue(TEXT("Weight")), 42.f);
            TestTrue(TEXT("Offline time does not expire saved cooldown"), Fixture->Restored->bIsOnFertilityCooldown);
            TestTrue(TEXT("Remaining time rebased exactly"), FMath::IsNearlyEqual(Fixture->Restored->GetFertilityCooldownRemaining(), Fixture->Remaining));
            Fixture->Restored->OnComponentLoaded();
            TestTrue(TEXT("Repeated load hooks don't double timers"), FMath::IsNearlyEqual(Fixture->Restored->GetFertilityCooldownRemaining(), Fixture->Remaining));
        }
        else if (Fixture->Step == 2)
        {
            Timers.Tick(2.7f);
            TestFalse(TEXT("Still infertile just before expiry"), Fixture->Restored->IsFertile_Implementation());
        }
        else
        {
            Timers.Tick(0.1f);
            TestTrue(TEXT("Fertility resumes after remaining game time"), Fixture->Restored->IsFertile_Implementation());
            TestEqual(TEXT("Completed cooldown is zero"), Fixture->Restored->GetFertilityCooldownRemaining(), 0.f);
            return true;
        }
        ++Fixture->Step;
        return false;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaEggPersistenceTest, "Pangea.ACF.Breeding.EggPausesOfflineAndNeverRerolls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaEggPersistenceTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FBreedingPersistenceFixture> Fixture = MakeShared<FBreedingPersistenceFixture>();
    if (!TestNotNull(TEXT("Isolated world"), Fixture->World)) return false;
    Fixture->World->GetTimerManager().Tick(0.f);
    Fixture->Egg = Fixture->World->SpawnActor<APangeaEggActor>();
    if (!TestNotNull(TEXT("Egg"), Fixture->Egg)) return false;
    UPangeaCreatureDefinition* Definition = NewObject<UPangeaCreatureDefinition>(Fixture->Egg);
    Definition->SpeciesId = TEXT("EggPersistenceTest");
    // Intentionally no hatch class: a failed hatch must not consume the egg or reroll its outcome.
    UPangeaBreedingFragment* Fragment = NewObject<UPangeaBreedingFragment>(Definition);
    Fragment->Incubation.IncubationSeconds = 3.f;
    FMaterialColorGeneticGroup Group;
    Group.ParameterNames.Add(TEXT("Skin"));
    Fragment->MaterialColorGroups.Add(Group);
    Definition->Fragments.Add(Fragment);
    FParentSnapshot A, B;
    A.SpeciesID = B.SpeciesID = Definition->SpeciesId;
    A.CreatureId = FGuid::NewGuid(); B.CreatureId = FGuid::NewGuid();
    A.Traits.SetValue(TEXT("Weight"), 10.f); B.Traits.SetValue(TEXT("Weight"), 20.f);
    A.MaterialParams.Add(TEXT("Skin"), FLinearColor::Red); B.MaterialParams.Add(TEXT("Skin"), FLinearColor::Blue);
    UPangeaGeneticStrategy* Strategy = NewObject<UPangeaGeneticStrategy>(Fixture->Egg);
    Fixture->Egg->InitializeEgg(A, B, Strategy, Definition);
    if (!TestTrue(TEXT("Valid egg initialized"), Fixture->Egg->IsInitialized())) return false;
    const float Trait = Fixture->Egg->ChildTraits.GetValue(TEXT("Weight"));
    const FLinearColor Color = Fixture->Egg->ChildAppearance.Colors[0].Value;
    const ECreatureGender Gender = Fixture->Egg->ChildGender;
    Fixture->Egg->InitializeEgg(B, A, Strategy, Definition);
    TestEqual(TEXT("Second initialization cannot reroll traits"), Fixture->Egg->ChildTraits.GetValue(TEXT("Weight")), Trait);
    TestTrue(TEXT("Second initialization cannot exchange lineage"), Fixture->Egg->ParentA.CreatureId == A.CreatureId);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture, Trait, Color, Gender]()
    {
        if (Fixture->LastFrame == GFrameCounter) return false;
        Fixture->LastFrame = GFrameCounter;
        // This isolated world deliberately has no BeginPlay; permit ALS's reflected native actor hooks.
        FEditorScriptExecutionGuard ScriptGuard;
        FTimerManager& Timers = Fixture->World->GetTimerManager();
        if (Fixture->Step == 0)
        {
            Timers.Tick(1.25f);
            Fixture->Remaining = Fixture->Egg->GetIncubationRemaining();
            TestTrue(TEXT("Incubation uses actual timer remaining"), FMath::IsNearlyEqual(Fixture->Remaining, 1.75f));
            Fixture->EggRecord = UALSFunctionLibrary::SerializeActor(Fixture->Egg);
            // Keep transient fixture definitions alive while stopping the old session's timers.
            Timers.ClearAllTimersForObject(Fixture->Egg);
        }
        else if (Fixture->Step == 1)
        {
            Timers.Tick(100.f);
            Fixture->RestoredEgg = Fixture->World->SpawnActor<APangeaEggActor>();
            if (!TestNotNull(TEXT("New egg for load"), Fixture->RestoredEgg)) return true;
            UALSFunctionLibrary::FullDeserializeActor(Fixture->RestoredEgg, Fixture->EggRecord, false);
            TestTrue(TEXT("Incubation pauses offline"), FMath::IsNearlyEqual(Fixture->RestoredEgg->GetIncubationRemaining(), Fixture->Remaining));
            TestEqual(TEXT("Traits not rerolled on load"), Fixture->RestoredEgg->ChildTraits.GetValue(TEXT("Weight")), Trait);
            TestTrue(TEXT("Appearance not rerolled on load"), Fixture->RestoredEgg->ChildAppearance.Colors.Num() == 1 && Fixture->RestoredEgg->ChildAppearance.Colors[0].Value.Equals(Color));
            TestTrue(TEXT("Gender not rerolled on load"), Fixture->RestoredEgg->ChildGender == Gender);
        }
        else
        {
            Timers.Tick(2.f);
            TestTrue(TEXT("Failed automatic hatch remains ready"), Fixture->RestoredEgg->EggState == EPangeaEggState::ReadyToHatch);
            TestNull(TEXT("Missing class fails safely"), Fixture->RestoredEgg->Hatch());
            TestFalse(TEXT("Failed hatch preserves egg"), Fixture->RestoredEgg->IsActorBeingDestroyed());
            TestEqual(TEXT("Failed hatch preserves outcome"), Fixture->RestoredEgg->ChildTraits.GetValue(TEXT("Weight")), Trait);
            return true;
        }
        ++Fixture->Step;
        return false;
    }));
    return true;
}
#endif
