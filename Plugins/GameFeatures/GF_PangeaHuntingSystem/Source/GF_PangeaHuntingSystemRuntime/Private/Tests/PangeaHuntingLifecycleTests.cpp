#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Characters/PDDinosaurBase.h"
#include "Components/PangeaHunterSenseComponent.h"
#include "Components/PangeaTrackEmitterComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFeatureAction_AddComponents.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
template <typename T> int32 CountFeatureComponents(AActor* Actor)
{
	TArray<T*> Components;
	if (Actor) Actor->GetComponents<T>(Components);
	return Components.FilterByPredicate([](const T* Component) { return IsValid(Component) && Component->IsRegistered(); }).Num();
}
UWorld* FindHuntingPIEWorld()
{
	if (GEngine)
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetFirstPlayerController()) return Context.World();
	return nullptr;
}
struct FHuntingLifecycleState
{
	FString URL, Error;
	TWeakObjectPtr<APDDinosaurBase> Probe, InactiveProbe;
	int32 Stage = 0;
	bool bComplete = false;
	double ReadyAt = 0.0, Deadline = FPlatformTime::Seconds() + 30.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaHuntingInjectionDataTest, "Pangea.ACF.Hunting.SingleInjectionDataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaHuntingInjectionDataTest::RunTest(const FString& Parameters)
{
	const UGameFeatureData* Data = LoadObject<UGameFeatureData>(nullptr, TEXT("/GF_PangeaHuntingSystem/GF_PangeaHuntingSystem.GF_PangeaHuntingSystem"));
	if (!TestNotNull(TEXT("Hunting feature data"), Data)) return false;
	int32 Emitters = 0, Hunters = 0;
	for (const UGameFeatureAction* Action : Data->GetActions())
		if (const UGameFeatureAction_AddComponents* Add = Cast<UGameFeatureAction_AddComponents>(Action))
			for (const FGameFeatureComponentEntry& Entry : Add->ComponentList)
			{
				if (Entry.ComponentClass.ToSoftObjectPath() == FSoftObjectPath(UPangeaTrackEmitterComponent::StaticClass()))
				{
					++Emitters;
					TestTrue(TEXT("Emitter targets the native extensible dinosaur family"), Entry.ActorClass.ToSoftObjectPath() == FSoftObjectPath(APDDinosaurBase::StaticClass()));
				}
				else if (Entry.ComponentClass.ToSoftObjectPath() == FSoftObjectPath(UPangeaHunterSenseComponent::StaticClass()))
				{
					++Hunters;
					TestEqual(TEXT("Hunter sense targets only the player family"), Entry.ActorClass.ToSoftObjectPath().ToString(), FString(TEXT("/Game/_Game/Characters/Player/Blueprints/BP_Pangea_Player.BP_Pangea_Player_C")));
				}
				else continue;
				TestTrue(TEXT("Server and client coverage retained"), Entry.bClientComponent && Entry.bServerComponent);
				TestTrue(TEXT("Requests are AddUnique"), (Entry.AdditionFlags & static_cast<uint8>(EGameFrameworkAddComponentFlags::AddUnique)) != 0);
			}
	TestEqual(TEXT("One emitter registration"), Emitters, 1);
	TestEqual(TEXT("One hunter registration"), Hunters, 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaHuntingLifecycleTest, "Pangea.ACF.Hunting.ActivateDeactivateReactivateInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaHuntingLifecycleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State = MakeShared<FHuntingLifecycleState>()]() mutable
	{
		UGameFeaturesSubsystem& Features = UGameFeaturesSubsystem::Get();
		if (FPlatformTime::Seconds() > State->Deadline)
		{
			AddError(TEXT("Timed out exercising hunting lifecycle; requesting original active state restoration"));
			if (!State->URL.IsEmpty()) Features.LoadAndActivateGameFeaturePlugin(State->URL, FGameFeaturePluginLoadComplete());
			if (State->Probe.IsValid()) State->Probe->Destroy();
			if (State->InactiveProbe.IsValid()) State->InactiveProbe->Destroy();
			return true;
		}
		UWorld* World = FindHuntingPIEWorld();
		if (!World) return false;
		if (State->ReadyAt == 0.0) { State->ReadyAt = FPlatformTime::Seconds() + 1.0; return false; }
		if (FPlatformTime::Seconds() < State->ReadyAt) return false;
		APawn* Player = World->GetFirstPlayerController()->GetPawn();
		const auto Completion = FGameFeaturePluginChangeStateComplete::CreateLambda([State](const UE::GameFeatures::FResult& Result)
		{
			State->bComplete = true;
			State->Error = Result.HasError() ? Result.GetError() : FString();
		});
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (State->Stage == 0)
		{
			if (!Features.GetPluginURLByName(TEXT("GF_PangeaHuntingSystem"), State->URL) || !Features.IsGameFeaturePluginActive(State->URL))
			{
				AddError(TEXT("Hunting feature must start active; no feature state was changed"));
				return true;
			}
			TestEqual(TEXT("Exactly one player hunter sense"), CountFeatureComponents<UPangeaHunterSenseComponent>(Player), 1);
			for (TActorIterator<APDDinosaurBase> It(World); It; ++It)
				TestEqual(TEXT("Exactly one emitter on each existing dinosaur"), CountFeatureComponents<UPangeaTrackEmitterComponent>(*It), 1);
			State->Probe = World->SpawnActor<APDDinosaurBase>(APDDinosaurBase::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Spawn);
			TestEqual(TEXT("Exactly one emitter on newly spawned native dinosaur"), CountFeatureComponents<UPangeaTrackEmitterComponent>(State->Probe.Get()), 1);
			State->Stage = 1; State->bComplete = false;
			Features.DeactivateGameFeaturePlugin(State->URL, Completion);
			return false;
		}
		if (!State->bComplete) return false;
		if (!State->Error.IsEmpty()) AddError(State->Error);
		if (State->Stage == 1)
		{
			TestEqual(TEXT("Deactivate removes hunter sense"), CountFeatureComponents<UPangeaHunterSenseComponent>(Player), 0);
			for (TActorIterator<APDDinosaurBase> It(World); It; ++It)
				TestEqual(TEXT("Deactivate removes emitters"), CountFeatureComponents<UPangeaTrackEmitterComponent>(*It), 0);
			State->InactiveProbe = World->SpawnActor<APDDinosaurBase>(APDDinosaurBase::StaticClass(), FVector(0, 1000, 5000), FRotator::ZeroRotator, Spawn);
			TestEqual(TEXT("Module load alone does not inject while feature inactive"), CountFeatureComponents<UPangeaTrackEmitterComponent>(State->InactiveProbe.Get()), 0);
			State->Stage = 2; State->bComplete = false;
			Features.LoadAndActivateGameFeaturePlugin(State->URL, Completion);
			return false;
		}
		TestTrue(TEXT("Original active state restored"), Features.IsGameFeaturePluginActive(State->URL));
		TestEqual(TEXT("Reactivate creates one hunter sense"), CountFeatureComponents<UPangeaHunterSenseComponent>(Player), 1);
		for (TActorIterator<APDDinosaurBase> It(World); It; ++It)
			TestEqual(TEXT("Reactivate creates exactly one emitter per dinosaur"), CountFeatureComponents<UPangeaTrackEmitterComponent>(*It), 1);
		if (State->Probe.IsValid()) State->Probe->Destroy();
		if (State->InactiveProbe.IsValid()) State->InactiveProbe->Destroy();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
