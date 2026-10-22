#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ACFAIController.h"
#include "ACFTeamsConfigDataAsset.h"
#include "Actors/ACFCharacter.h"
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/MiningSiteActor.h"
#include "Animation/ACFAnimInstance.h"
#include "Components/MiningPresentationAgentComponent.h"
#include "Components/ACFTeamManagerComponent.h"
#include "Components/MiningRoutineJobComponent.h"
#include "Components/MiningSiteComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/ACFGameMode.h"
#include "Game/ACFGameState.h"
#include "LevelEditor.h"
#include "NavigationSystem.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "HAL/PlatformStackWalk.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include <atomic>

namespace
{
/** Diagnostic only: preserve the original error and capture its native/Blueprint origin once. */
struct FPlayerStartupErrorCapture : FOutputDevice
{
	std::atomic<uint32> Captured { 0 };
	std::atomic<bool> HitWarpDiagnostic { false };
	FPlayerStartupErrorCapture() { GLog->AddOutputDevice(this); }
	~FPlayerStartupErrorCapture() { GLog->RemoveOutputDevice(this); }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; } // Capture at the origin, not the log-flush thread.
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		// The engine logs the NaN error only once per process, but dumps this
		// callstack every time. Warm PIE must not falsely pass on subsequent NaNs.
		if (FCString::Strstr(Message, TEXT("UACFAdvancedHitAction::GetWarpTransform")) ||
			FCString::Strstr(Message, TEXT("UPangeaAdvancedHitAction::GetWarpTransform"))) HitWarpDiagnostic.store(true);
		const uint32 Flag = FCString::Strstr(Message, TEXT("Invalid input P=")) ? 1u : FCString::Strstr(Message, TEXT("Invalid ExpToGiveOnDeathByCurrentLevel")) ? 2u : 0u;
		if (!Flag || (Captured.fetch_or(Flag) & Flag)) return;
		ANSICHAR Stack[65536] = {};
		FPlatformStackWalk::StackWalkAndDump(Stack, UE_ARRAY_COUNT(Stack), 2);
		const FString ScriptStack = IsInGameThread() ? FFrame::GetScriptCallstack() : TEXT("Not on game thread");
		const FString Report = FString(Message) + TEXT("\n\nBlueprint:\n") + ScriptStack + TEXT("\n\nNative:\n") + ANSI_TO_TCHAR(Stack);
		FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("Yarin") / (Flag == 1u ? TEXT("PlayerStartup-NaN-Stack.log") : TEXT("PlayerStartup-XP-Stack.log"))));
	}
};

struct FMiningMultiplayerProbe
{
	FAutomationTestBase& Test;
	TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	TStrongObjectPtr<UACFTeamsConfigDataAsset> TeamConfig;
	bool bProjectPlayers = false;
	bool bProjectPreviewsChecked = false;
	TUniquePtr<FPlayerStartupErrorCapture> StartupErrorCapture;
	FDelegateHandle WorldInitializedHandle;
	FDelegateHandle WorldCreatedHandle;
	TArray<TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>> ActorSpawnHandles;
	TWeakObjectPtr<AMiningSiteActor> Site;
	TWeakObjectPtr<AACFCharacter> Agents[3];
	FVector PreviousPosition[3] = {};
	bool bHavePosition[3] = {};
	bool bTravel[3] = {};
	bool bStation[3] = {};
	FName RetiredNames[3];
	int32 Phase = 0;
	double Deadline = FPlatformTime::Seconds() + 75.0;
	double ReadyAt = 0;

	explicit FMiningMultiplayerProbe(FAutomationTestBase& InTest, bool bInProjectPlayers = false) : Test(InTest),
		Settings(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage())),
		TeamConfig(LoadObject<UACFTeamsConfigDataAsset>(nullptr, TEXT("/Game/_Game/DA_PangeaTeamConfig.DA_PangeaTeamConfig")))
	{
		bProjectPlayers = bInProjectPlayers;
		Settings->SetPlayNetMode(PIE_ListenServer);
		Settings->SetRunUnderOneProcess(true);
		Settings->SetPlayNumberOfClients(2);
		Settings->bLaunchSeparateServer = false;
		if (bProjectPlayers)
		{
			StartupErrorCapture = MakeUnique<FPlayerStartupErrorCapture>();
			return; // Leave project GameMode/GameState/player/controller initialization untouched.
		}
		WorldCreatedHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([this](UWorld* World, const UWorld::InitializationValues Values)
		{
			if (!World || World->WorldType != EWorldType::PIE) return;
			const FDelegateHandle Handle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([this](AActor* Actor)
			{
				if (AGameModeBase* Mode = Cast<AGameModeBase>(Actor)) Mode->GameStateClass = AACFGameState::StaticClass();
				if (AGameStateBase* State = Cast<AGameStateBase>(Actor)) ConfigureFixtureGameState(State);
			}));
			ActorSpawnHandles.Emplace(World, Handle);
		});
		WorldInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda([this](const FActorsInitializedParams& Params)
		{
			if (Params.World && Params.World->WorldType == EWorldType::PIE) EnsureFixtureTeams(Params.World);
		});
	}
	~FMiningMultiplayerProbe()
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedHandle);
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldCreatedHandle);
		for (const auto& Entry : ActorSpawnHandles) if (Entry.Key.IsValid()) Entry.Key->RemoveOnActorSpawnedHandler(Entry.Value);
	}
	void EnsureFixtureTeams(UWorld* World)
	{
		ConfigureFixtureGameState(World->GetGameState());
	}
	void ConfigureFixtureGameState(AGameStateBase* GameState)
	{
		if (!GameState || !TeamConfig.IsValid()) return;
		// Native test players bypass the unrelated project player/appearance startup.
		// Supply ACF's real local team-data prerequisite on both peers. No runtime
		// team changes are exercised here, and no config asset is changed or saved.
		UACFTeamManagerComponent* Teams = GameState->FindComponentByClass<UACFTeamManagerComponent>();
		if (Teams && Teams->GetTeamConfigDataAsset()) return;
		if (!Teams)
		{
			Teams = NewObject<UACFTeamManagerComponent>(GameState);
			GameState->AddInstanceComponent(Teams);
			Teams->SetIsReplicated(false);
		}
		const FObjectPropertyBase* Config = FindFProperty<FObjectPropertyBase>(Teams->GetClass(), TEXT("TeamConfigDataAsset"));
		if (Config) Config->SetObjectPropertyValue_InContainer(Teams, TeamConfig.Get());
		Teams->RegisterComponent();
	}
	bool Fail(const FString& Message)
	{
		Test.AddError(Message);
		if (Site.IsValid()) Site->Destroy();
		return true;
	}
	static AActor* FindNamed(UWorld* World, FName Name)
	{
		for (TActorIterator<AActor> It(World); It; ++It) if (It->GetFName() == Name) return *It;
		return nullptr;
	}
	bool CheckProjectPreviews(UWorld* Server, UWorld* Client, FString& Failure)
	{
		const UClass* ProjectManager = LoadClass<UActorComponent>(nullptr, TEXT("/Script/Pangea_Dawn.PangeaItemsManagerComponent"));
		UClass* ProjectSetup = LoadClass<UActorComponent>(nullptr, TEXT("/Script/Pangea_Dawn.PangeaCharacterSetupComponent"));
		if (!ProjectManager || !ProjectSetup) { Failure = TEXT("Pangea integration classes missing; close Editor and rebuild"); return false; }
		bool bReady = true;
		for (UWorld* World : { Server, Client })
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (!PC || !PC->GetPawn()) continue;
				if (!PC->GetPawn()->GetComponentByClass(ProjectSetup)) { Failure = TEXT("Project player lacks Pangea leveling setup component"); return false; }
				int32 PreviewManagers = 0;
				TInlineComponentArray<UActorComponent*> Components(PC);
				for (UActorComponent* Component : Components)
				{
					const FObjectPropertyBase* PreviewProperty = FindFProperty<FObjectPropertyBase>(Component->GetClass(), TEXT("PreviewCharacter"));
					const FObjectPropertyBase* StageProperty = FindFProperty<FObjectPropertyBase>(Component->GetClass(), TEXT("ItemPreviewStage"));
					UFunction* Request = Component->FindFunction(TEXT("SpawnItemPreviewStage"));
					if (!PreviewProperty || !StageProperty || !Request) continue;
					++PreviewManagers;
					if (!Component->IsA(ProjectManager)) { Failure = TEXT("Project controller still uses the vendor preview factory instead of the Pangea adapter"); return false; }
					AACFCharacter* Preview = Cast<AACFCharacter>(PreviewProperty->GetObjectPropertyValue_InContainer(Component));
					AActor* Stage = Cast<AActor>(StageProperty->GetObjectPropertyValue_InContainer(Component));
					// Exercise opening/recovering a preview after possession, including the
					// warm-cache path where the initial stage was made before the pawn existed.
					if (!Preview && IsValid(Stage)) Stage->Destroy();
					struct { AActor* ReturnValue = nullptr; } Params;
					Component->ProcessEvent(Request, &Params);
					Preview = Cast<AACFCharacter>(PreviewProperty->GetObjectPropertyValue_InContainer(Component));
					Stage = Cast<AActor>(StageProperty->GetObjectPropertyValue_InContainer(Component));
					if (!PC->IsLocalController())
					{
						if (IsValid(Preview) || IsValid(Stage)) { Failure = TEXT("Remote server controller created local-only item preview actors"); return false; }
						continue;
					}
					if (!IsValid(Preview)) { bReady = false; continue; }
					if (Preview->GetIsReplicated() || (Stage && Stage->GetIsReplicated())) { Failure = TEXT("UI preview incorrectly participates in gameplay actor replication"); return false; }
					if (Preview->GetController() || Preview->AutoPossessPlayer != EAutoReceiveInput::Disabled || Preview->GetActorEnableCollision() || Preview->GetCharacterMovement()->MovementMode != MOVE_None)
					{ Failure = TEXT("UI preview is not inert/unpossessed"); return false; }
				}
				if (PreviewManagers != 1) { Failure = TEXT("Expected exactly one Pangea item manager per player controller"); return false; }
			}
		return bReady;
	}
	bool Tick()
	{
		if (FPlatformTime::Seconds() > Deadline)
			return Fail(FString::Printf(TEXT("Multiplayer phase %d timed out. Remote travel W/G/C=%d/%d/%d Station=%d/%d/%d"), Phase, bTravel[0], bTravel[1], bTravel[2], bStation[0], bStation[1], bStation[2]));
		UWorld* Server = nullptr;
		UWorld* Client = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !World) continue;
			if (World->GetNetMode() == NM_ListenServer) Server = World;
			if (World->GetNetMode() == NM_Client) Client = World;
		}
		if (!Server || !Client || !Client->GetFirstPlayerController() || !Client->GetFirstPlayerController()->GetPawn()) return false;
		if (!bProjectPlayers)
		{
			EnsureFixtureTeams(Server);
			EnsureFixtureTeams(Client); // A replicated client GameState can arrive after world initialization.
		}
		if (Phase == 0)
		{
			int32 Players = 0;
			for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It) if (It->IsValid() && It->Get()->GetPawn()) ++Players;
			if (Players != 2) return false;
			if (ReadyAt == 0) { ReadyAt = FPlatformTime::Seconds() + 2; return false; }
			if (FPlatformTime::Seconds() < ReadyAt) return false;
			if (bProjectPlayers && !bProjectPreviewsChecked)
			{
				FString Failure;
				const bool bReady = CheckProjectPreviews(Server, Client, Failure);
				if (!Failure.IsEmpty()) return Fail(Failure);
				if (!bReady) return false;
				bProjectPreviewsChecked = true;
			}
			AMiningDiscoveryNodeActor* Node = nullptr;
			for (TActorIterator<AMiningDiscoveryNodeActor> It(Server); It; ++It)
				if (!It->ActorHasTag(TEXT("Pangea.Mining.Prototype"))) { Node = *It; break; }
			if (!Node || !Node->SiteDefinition || !Node->MiningSiteActorClass) return Fail(TEXT("SC_Demo mining discovery configuration required"));
			const FTransform Transform = Node->SiteSpawnTransform * Node->GetActorTransform();
			for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
				if (It->IsValid() && It->Get()->GetPawn())
				{
					APawn* Player = It->Get()->GetPawn();
					Player->SetActorLocation(Transform.GetLocation() + FVector(-600, -600, 100), false, nullptr, ETeleportType::TeleportPhysics);
					UNavigationSystemV1::RegisterNavigationInvoker(*Player, 5000, 5500);
				}
			AMiningSiteActor* Fixture = Server->SpawnActorDeferred<AMiningSiteActor>(Node->MiningSiteActorClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Fixture) return Fail(TEXT("Cannot spawn replicated mining fixture"));
			Site = Fixture;
			Fixture->SetFlags(RF_Transient);
			Fixture->MiningSiteComponent->SiteDefinition = Node->SiteDefinition; // Real asset reference, not an unreplicable transient definition.
			// Fixture state only: no upgrade purchase, definition mutation, inventory grant or save.
			const FBoolProperty* Established = FindFProperty<FBoolProperty>(Fixture->MiningSiteComponent->GetClass(), TEXT("bEstablished"));
			const FIntProperty* Level = FindFProperty<FIntProperty>(Fixture->MiningSiteComponent->GetClass(), TEXT("CurrentLevel"));
			if (!Established || !Level) return Fail(TEXT("Mining fixture progression properties unavailable"));
			Established->SetPropertyValue_InContainer(Fixture->MiningSiteComponent, true);
			Level->SetPropertyValue_InContainer(Fixture->MiningSiteComponent, 3);
			Fixture->SettlementResourceActor = Node->SettlementResourceActor;
			Fixture->FinishSpawning(Transform);
			Phase = 1;
			return false;
		}
		if (Phase == 3)
		{
			for (int32 I = 0; I < 3; ++I) if (FindNamed(Client, RetiredNames[I])) return false;
			if (StartupErrorCapture && StartupErrorCapture->HitWarpDiagnostic.load()) return Fail(TEXT("Hit warp emitted a diagnostic stack (engine NaN errors are once-per-process)"));
			Test.AddInfo(TEXT("Listen server + remote client: replicated roles, moving locomotion, station montages, rejected client authorship, server-only jobs and replicated teardown verified"));
			return true;
		}
		if (!Site.IsValid()) return Fail(TEXT("Replicated site fixture disappeared"));
		if (Phase == 1)
		{
			const FName SiteTag(*FString::Printf(TEXT("Mining.Site.%s"), *Site->GetName()));
			for (TActorIterator<AACFCharacter> It(Server); It; ++It)
			{
				if (!It->ActorHasTag(SiteTag)) continue;
				UMiningPresentationAgentComponent* Agent = It->FindComponentByClass<UMiningPresentationAgentComponent>();
				if (Agent) Agents[static_cast<int32>(Agent->GetCurrentRole())] = *It;
			}
			for (int32 I = 0; I < 3; ++I) if (!Agents[I].IsValid()) return false;
			FNavLocation Points[3];
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Server);
			for (int32 I = 0; I < 3; ++I)
				if (!Nav || !Nav->ProjectPointToNavigation(Agents[I]->GetActorLocation() + FVector(-650, 0, 0), Points[I], FVector(250, 250, 300))) return false; // Wait for invoker-driven tile generation.
			for (int32 I = 0; I < 3; ++I)
			{
				AACFAIController* AI = Cast<AACFAIController>(Agents[I]->GetController());
				if (!AI || !AI->FindComponentByClass<UMiningRoutineJobComponent>()) return Fail(TEXT("Server mining job missing"));
				AI->SetCurrentAIState(FGameplayTag::RequestGameplayTag(TEXT("AIState.Wait")));
				Agents[I]->SetActorLocation(Points[I].Location + FVector(0, 0, Agents[I]->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
				AI->ResetToDefaultState();
			}
			Phase = 2;
			return false;
		}
		for (int32 I = 0; I < 3; ++I)
		{
			if (!Agents[I].IsValid()) return Fail(TEXT("Server agent lost during network sampling"));
			AACFCharacter* Remote = Cast<AACFCharacter>(FindNamed(Client, Agents[I]->GetFName()));
			UMiningPresentationAgentComponent* Agent = Remote ? Remote->FindComponentByClass<UMiningPresentationAgentComponent>() : nullptr;
			if (!Agent) continue;
			if (Remote->HasAuthority() || Remote->GetController() || Remote->FindComponentByClass<UMiningRoutineJobComponent>()) return Fail(TEXT("Remote presentation unexpectedly owns authoritative AI"));
			TInlineComponentArray<UMiningPresentationAgentComponent*> Components(Remote);
			if (Components.Num() != 1) return Fail(TEXT("Remote agent has duplicate presentation components"));
			if (Agent->GetCurrentRole() != static_cast<EMiningPresentationRole>(I)) continue; // Wait for initial snapshot.
			const EMiningPresentationState State = Agent->GetCurrentState();
			const FVector Focus = Agent->GetPresentationFocusLocation();
			Agent->SetMiningPresentationRole_Implementation(static_cast<EMiningPresentationRole>((I + 1) % 3));
			Agent->SetMiningPresentationState_Implementation(State == EMiningPresentationState::Idle ? EMiningPresentationState::Working : EMiningPresentationState::Idle);
			Agent->SetMiningPresentationFocus_Implementation(Focus + FVector(1000, 0, 0));
			if (Agent->GetCurrentState() != State || Agent->GetCurrentRole() != static_cast<EMiningPresentationRole>(I) || !Agent->GetPresentationFocusLocation().Equals(Focus)) return Fail(TEXT("Remote setters authored mining presentation"));
			UACFAnimInstance* Anim = Remote->GetACFAnimInstance();
			if (!Anim || !Anim->GetCurrentMoveset()) continue;
			if (State == EMiningPresentationState::Traveling && Anim->GetSpeed() > 10)
				bTravel[I] |= bHavePosition[I] && FVector::Dist2D(PreviousPosition[I], Remote->GetActorLocation()) > 1;
			PreviousPosition[I] = Remote->GetActorLocation();
			bHavePosition[I] = true;
			const EMiningPresentationState Expected = I == 0 ? EMiningPresentationState::Working : I == 1 ? EMiningPresentationState::Guarding : EMiningPresentationState::Loading;
			bStation[I] |= State == Expected && Anim->GetCurrentActiveMontage() != nullptr && Agent->HasPresentationFocus();
		}
		for (int32 I = 0; I < 3; ++I) if (!bTravel[I] || !bStation[I]) return false;
		for (int32 I = 0; I < 3; ++I) RetiredNames[I] = Agents[I]->GetFName();
		Site->Destroy();
		Phase = 3;
		Deadline = FPlatformTime::Seconds() + 15;
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMultiplayerPIEAcceptanceTest, "Pangea.ACF.Mining.RemotePresentationInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMultiplayerPIEAcceptanceTest::RunTest(const FString& Parameters)
{
	// Fail before launching PIE instead of allowing GetReplicatedProperty to fatal
	// on known scene-wide replication-contract regressions (including hunting).
	const UClass* EmitterClass = LoadClass<UActorComponent>(nullptr, TEXT("/Script/GF_PangeaHuntingSystemRuntime.PangeaTrackEmitterComponent"));
	const FProperty* HuntConfig = EmitterClass ? FindFProperty<FProperty>(EmitterClass, TEXT("HuntSpeciesConfig")) : nullptr;
	const FProperty* Established = FindFProperty<FProperty>(AMiningDiscoveryNodeActor::StaticClass(), TEXT("EstablishedSite"));
	if (!TestTrue(TEXT("Rebuilt hunting config is tagged for replication before network PIE"), HuntConfig && HuntConfig->HasAnyPropertyFlags(CPF_Net)) ||
		!TestTrue(TEXT("Rebuilt discovery reference is tagged for replication before network PIE"), Established && Established->HasAnyPropertyFlags(CPF_Net))) return false;
	const TSharedRef<FMiningMultiplayerProbe> Probe = MakeShared<FMiningMultiplayerProbe>(*this);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]()
	{
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = Probe->Settings.Get(); // Per-session copy; do not change/save user multiplayer preferences.
		Params.DestinationSlateViewport = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetFirstActiveViewport();
		Params.bAllowOnlineSubsystem = false;
		Params.GameModeOverride = AACFGameMode::StaticClass(); // Native default pawns + ACF collision/ragdoll masters; preserve SC_Demo mining assets/navigation.
		GEditor->RequestPlaySession(Params);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaProjectPlayerNetworkTest, "Pangea.ACF.Mining.ProjectPlayerRemotePresentationInPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaProjectPlayerNetworkTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Rebuilt Pangea preview adapter before project-player PIE"),
		LoadClass<UActorComponent>(nullptr, TEXT("/Script/Pangea_Dawn.PangeaItemsManagerComponent")))) return false;
	const TSharedRef<FMiningMultiplayerProbe> Probe = MakeShared<FMiningMultiplayerProbe>(*this, true);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]()
	{
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = Probe->Settings.Get();
		Params.DestinationSlateViewport = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetFirstActiveViewport();
		Params.bAllowOnlineSubsystem = false;
		GEditor->RequestPlaySession(Params);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Probe]() { return Probe->Tick(); }));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
