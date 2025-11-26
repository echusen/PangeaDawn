// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Components/ACFGroupAIComponent.h"
#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFTeamComponent.h"
#include "Components/ACFThreatManagerComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFPlayerController.h"
#include "Game/ACFTypes.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include <Engine/AssetManager.h>
#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <Kismet/GameplayStatics.h>
#include <Kismet/KismetSystemLibrary.h>
#include <NavigationSystem.h>
#include <PrimitiveSceneProxy.h>
#include <GameFramework/Controller.h>
#include "ACFTeamManagerSubsystem.h"
#include <MeshElementCollector.h>
#include <SceneView.h>
#include <PrimitiveDrawingUtils.h>
#include <Logging.h>


// Sets default values for this component's properties
UACFGroupAIComponent::UACFGroupAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bInBattle = false;
	SetIsReplicatedByDefault(true);
	DefaultSpawnOffset = FVector2D(150.f, 150.f);
	AIToSpawn.Add(FAISpawnInfo());
}

void UACFGroupAIComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UACFGroupAIComponent, groupLead);
	DOREPLIFETIME(UACFGroupAIComponent, bInBattle);
	DOREPLIFETIME(UACFGroupAIComponent, AICharactersInfo);
}
// Called when the game starts
void UACFGroupAIComponent::BeginPlay()
{
	Super::BeginPlay();
	SetReferences();
}

void UACFGroupAIComponent::SetReferences()
{
	groupLead = Cast<AActor>(GetOwner());
}

void UACFGroupAIComponent::OnComponentLoaded_Implementation()
{
	for (int32 index = 0; index < AICharactersInfo.Num(); index++) {
		FAIAgentsInfo& agent = AICharactersInfo[index];
		TArray<AActor*> foundActors;
		UGameplayStatics::GetAllActorsOfClassWithTag(this, agent.characterClass, agent.Guid, foundActors);
		if (foundActors.Num() == 0) {
			UE_LOG(ACFAILog, Error, TEXT("Impossible to find actor"));
			continue;
		}
		agent.AICharacter = Cast<AACFCharacter>(foundActors[0]);
		InitAgent(agent, index);
	}
}

void UACFGroupAIComponent::SendCommandToCompanions_Implementation(FGameplayTag command)
{
	Internal_SendCommandToAgents(command);
}

void UACFGroupAIComponent::SpawnGroup_Implementation()
{
	if (bAlreadySpawned && !bCanSpawnMultitpleTimes) {
		return;
	}

	if (AICharactersInfo.Num() > 0) {
		// Already spawned!
		return;
	}

	Internal_SpawnGroup();
	bAlreadySpawned = true;
}

void UACFGroupAIComponent::Internal_SpawnGroup()
{
	if (AIToSpawn.Num() == 0) {
		UE_LOG(ACFAILog, Error, TEXT("%s NO AI to Spawn - AAIFGROUPPAWN::SpawnAGroup"), *this->GetName());
		return;
	}

	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FAISpawnInfo& info : AIToSpawn) {
		if (info.AIClassBP.ToSoftObjectPath().IsValid()) {
			AssetsToLoad.Add(info.AIClassBP.ToSoftObjectPath());
		}
	}

	StreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateUObject(this, &UACFGroupAIComponent::OnAIAssetsLoaded),
		FStreamableManager::AsyncLoadHighPriority);
}

void UACFGroupAIComponent::OnAIAssetsLoaded()
{
	const UWorld* world = GetWorld();
	if (!world) {
		return;
	}

	for (const FAISpawnInfo& aiSpawn : AIToSpawn) {
		// safety check for the AIClassBP to ensure it is valid before adding to the load list

		if (UClass* loadedClass = aiSpawn.AIClassBP.LoadSynchronous()) {
			AddAgentToGroup(aiSpawn);
		}
		else {
			UE_LOG(ACFAILog, Warning, TEXT("Failed to load class for spawn info!"));
		}
	}

	bAlreadySpawned = true;
	OnAgentsSpawned.Broadcast();
}

void UACFGroupAIComponent::DespawnGroup_Implementation(const bool bUpdateAIToSpawn /*= true*/, FGameplayTag actionToTriggerOnDyingAgent, float lifespawn /*= 1.f*/)
{
	if (bAlreadySpawned) {
		if (bUpdateAIToSpawn) {
			//  TArray<FAISpawnInfo> aicopy = AIToSpawn;
			AIToSpawn.Empty();
			for (FAIAgentsInfo agent : AICharactersInfo) {
				if (agent.AICharacter && agent.AICharacter->IsAlive()) {
					const TSubclassOf<AACFCharacter> charClass = agent.AICharacter->GetClass();
					AddAIToSpawn(FAISpawnInfo(charClass));
				}
			}
		}
		for (FAIAgentsInfo& agent : AICharactersInfo) {
			if (agent.AICharacter && agent.AICharacter->IsAlive()) {
				agent.AICharacter->DestroyCharacter(lifespawn);
				agent.AICharacter->TriggerAction(actionToTriggerOnDyingAgent, EActionPriority::EHigh);
			}
		}
		AICharactersInfo.Empty();
		bAlreadySpawned = false;
		OnAgentsDespawned.Broadcast();
	}
}

void UACFGroupAIComponent::InitAgents()
{
	for (int32 index = 0; index < AICharactersInfo.Num(); index++) {
		if (AICharactersInfo.IsValidIndex(index)) {
			InitAgent(AICharactersInfo[index], index);
		}
	}
}

void UACFGroupAIComponent::ServerAddCharacterToGroup_Implementation(AACFCharacter* character)
{
	AddExistingCharacterToGroup(character);
}

void UACFGroupAIComponent::ServerRemoveCharacterToGroup_Implementation(AACFCharacter* character)
{
	RemoveAgentFromGroup(character);
}

void UACFGroupAIComponent::ServerAddAIToSpawn_Implementation(const FAISpawnInfo& spawnInfo)
{
	AddAIToSpawn(spawnInfo);
}




void UACFGroupAIComponent::InitAgent(FAIAgentsInfo& agent, int32 childIndex)
{
	if (!agent.AICharacter) {
		ensure(false);
		return;
	}

	if (!agent.AICharacter->GetController()) {
		agent.AICharacter->SpawnDefaultController();
	}

	agent.characterClass = (agent.AICharacter->GetClass());

	if (!AIToSpawn.IsValidIndex(childIndex)) {
		UE_LOG(ACFAILog, Error, TEXT("invalid AI Index! - UACFGroupAIComponent::InitAgent"), *this->GetName());

		return;
	}

	if (agent.GetController()) {
		if (!groupLead) {
			SetReferences();
		}
		agent.GetController()->SetLeadActorBK(groupLead);
		agent.GetController()->SetDefaultState(DefaultAiState);
		agent.GetController()->SetCurrentAIState(DefaultAiState);
		if (AIToSpawn[childIndex].PatrolPath) {
			agent.GetController()->SetPatrolPath(AIToSpawn[childIndex].PatrolPath, true);
		}
	//	agent.AICharacter->GetEquipmentComponent()->InitializeStartingItems();

		if (bOverrideAgentTeam) {
			if (AController* Controller = agent.GetController()) {
				// Try ACF interface first
				if (Controller->Implements<UACFEntityInterface>()) {
					IACFEntityInterface::Execute_AssignTeamToEntity(Controller, CombatTeam);
				}
				// Fallback to Unreal's GenericTeamAgent
				else if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Controller)) {
					if (UWorld* World = Controller->GetWorld()) {
						if (const UACFTeamManagerSubsystem* TeamSubsystem = World->GetSubsystem<UACFTeamManagerSubsystem>()) {
							const FGenericTeamId GenericTeamId = TeamSubsystem->FromTagToTeamId(CombatTeam);
							TeamAgent->SetGenericTeamId(GenericTeamId);
						}
					}
				}
			}
		}

		check(agent.characterClass);
		const FName newGuid = agent.AICharacter->GetFName();
		agent.Guid = newGuid;
		if (!agent.AICharacter->Tags.Contains(newGuid)) {
			agent.AICharacter->Tags.Add(newGuid);
		}
		agent.GetController()->SetGroupOwner(this, childIndex, bOverrideAgentPerception, bOverrideAgentTeam);
		if (!agent.AICharacter->OnDeath.IsAlreadyBound(this, &UACFGroupAIComponent::HandleAgentDeath)) {
			agent.AICharacter->OnDeath.AddDynamic(this, &UACFGroupAIComponent::HandleAgentDeath);
		}
	}
}

bool UACFGroupAIComponent::AddAIToSpawnFromClass(const TSubclassOf<AACFCharacter>& charClass)
{
	if (!GetOwner()->HasAuthority()) {
		UE_LOG(ACFAILog, Warning, TEXT("Trying to manipulate AI from client!- UACFGroupAIComponentn"), *this->GetName());

		return false;
	}

	if (GetMaxSimultaneousAgents() <= GetTotalAIToSpawnCount()) {
		UE_LOG(ACFAILog, Warning, TEXT("Your Group Already Reach The Maximum! - UACFGroupAIComponent::AddAIToSpawn"), *this->GetName());
		return false;
	}

	AIToSpawn.Add(FAISpawnInfo(charClass));

	OnAgentsChanged.Broadcast();
	return true;
}

bool UACFGroupAIComponent::AddAIToSpawn(const FAISpawnInfo& spawnInfo)
{
	if (!GetOwner()->HasAuthority()) {
		UE_LOG(ACFAILog, Warning, TEXT("Trying to manipulate AI from client!- UACFGroupAIComponentn"), *this->GetName());
		return false;
	}

	if (GetMaxSimultaneousAgents() < GetTotalAIToSpawnCount()) {
		UE_LOG(ACFAILog, Warning, TEXT("Your Group Already Reach The Maximum! - UACFGroupAIComponent::AddAIToSpawn"), *this->GetName());
		return false;
	}

	AIToSpawn.Add(spawnInfo);

	OnAgentsChanged.Broadcast();
	return true;
}

bool UACFGroupAIComponent::RemoveAIToSpawn(const TSubclassOf<AACFCharacter>& charClass)
{
	if (AIToSpawn.Contains(charClass)) {

		AIToSpawn.Remove(charClass);

		OnAgentsChanged.Broadcast();
		return true;
	}
	return false;
}

void UACFGroupAIComponent::ReplaceAIToSpawn(const TArray<FAISpawnInfo>& newAIs)
{
	AIToSpawn.Empty();
	AIToSpawn = newAIs;
}

bool UACFGroupAIComponent::GetAgentByIndex(int32 index, FAIAgentsInfo& outAgent) const
{
	if (AICharactersInfo.IsValidIndex(index)) {
		outAgent = AICharactersInfo[index];
		return true;
	}
	return false;
}

void UACFGroupAIComponent::Internal_SendCommandToAgents(FGameplayTag command)
{
	for (FAIAgentsInfo& achar : AICharactersInfo) {
		if (achar.GetController()) {
			achar.GetController()->TriggerCommand(command);
		}
		else {

			ensure(false);
		}
	}
}

void UACFGroupAIComponent::SetEnemyGroup(UACFGroupAIComponent* inEnemyGroup)
{
	if (inEnemyGroup && UACFFunctionLibrary::AreEnemyTeams(GetWorld(), GetCombatTeam(), inEnemyGroup->GetCombatTeam())) {
		enemyGroup = inEnemyGroup;
	}
}

void UACFGroupAIComponent::HandleAgentDeath(class AACFCharacter* agent)
{
	OnChildDeath(agent);
}

FVector UACFGroupAIComponent::GetGroupCentroid() const
{
	TArray<AActor*> actors;
	for (const auto& agent : AICharactersInfo) {
		if (agent.AICharacter) {
			actors.Add(agent.AICharacter);
		}
	}
	return UGameplayStatics::GetActorArrayAverageLocation(actors);
}

class AACFCharacter* UACFGroupAIComponent::RequestNewTarget(const AACFAIController* requestSender)
{
	// First Try to help lead
	const AACFCharacter* lead = Cast<AACFCharacter>(requestSender->GetLeadActorBK());
	if (lead) {
		const AACFCharacter* newTarget = Cast<AACFCharacter>(lead->GetTarget());
		if (newTarget && newTarget->IsMyEnemy(requestSender->GetBaseAICharacter())) {
			return Cast<AACFCharacter>(lead->GetTarget());
		}
	}

	// Then Try to help other in  the group
	if (AICharactersInfo.IsValidIndex(0) && IsValid(AICharactersInfo[0].AICharacter) && IsValid(AICharactersInfo[0].GetController())) {
		for (FAIAgentsInfo achar : AICharactersInfo) {
			if (achar.GetController() && achar.GetController() != requestSender) {
				AACFCharacter* newTarget = Cast<AACFCharacter>(achar.GetController()->GetTargetActorBK());
				if (newTarget && newTarget->IsAlive() && achar.GetController()->IsInBattle()) {
					return newTarget;
				}
			}
		}
	}

	// Then Try to help other in  the group
	if (enemyGroup) {
		return enemyGroup->GetAgentNearestTo(requestSender->GetPawn()->GetActorLocation());
	}

	return nullptr;
}

FPrimitiveSceneProxy* UACFGroupAIComponent::CreateSceneProxy()
{
	class FSpawnPreviewSceneProxy final : public FPrimitiveSceneProxy {
	public:
		FSpawnPreviewSceneProxy(const UACFGroupAIComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
			, SpawnInfos(InComponent->GetAIToSpawn())
		{
		}

		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}
		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
			uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			const FMatrix& LocalToWorld = GetLocalToWorld();
			const int32 CapsuleSides = 24;
			const float CapsuleHalfHeight = 88.f;
			const float CapsuleRadius = 34.f;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++) {
				if (VisibilityMap & (1 << ViewIndex)) {
					const FSceneView* View = Views[ViewIndex];
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

					for (const FAISpawnInfo& Info : SpawnInfos) {
						const FTransform WorldTM = Info.SpawnTransform * FTransform(LocalToWorld);
						const FVector Location = WorldTM.GetLocation();
						const FVector X = WorldTM.GetUnitAxis(EAxis::X);
						const FVector Y = WorldTM.GetUnitAxis(EAxis::Y);
						const FVector Z = WorldTM.GetUnitAxis(EAxis::Z);

						DrawWireCapsule(
							PDI,
							Location,
							X,
							Y,
							Z,
							FColor::Green,
							CapsuleRadius,
							CapsuleHalfHeight,
							CapsuleSides,
							SDPG_World);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = true;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bEditorPrimitiveRelevance = true;
			return Result;
		}

		virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }
		uint32 GetAllocatedSize(void) const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

	private:
		TArray<FAISpawnInfo> SpawnInfos;
	};

	return new FSpawnPreviewSceneProxy(this);
}

FBoxSphereBounds UACFGroupAIComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FVector Origin = FVector::ZeroVector;

	for (const FAISpawnInfo& Info : AIToSpawn) {
		Origin += Info.SpawnTransform.GetLocation();
	}

	if (AIToSpawn.Num() > 0) {
		Origin /= AIToSpawn.Num();
	}

	const float MaxExtent = 300.f;
	return FBoxSphereBounds(Origin, FVector(MaxExtent), MaxExtent).TransformBy(LocalToWorld);
}

#if WITH_EDITOR
void UACFGroupAIComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkRenderStateDirty();
}

void UACFGroupAIComponent::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);
	MarkRenderStateDirty();
}
#endif

uint8 UACFGroupAIComponent::AddAgentToGroup(const FAISpawnInfo& spawnInfo)
{
	UWorld* const world = GetWorld();

	ensure(GetOwner()->HasAuthority());

	if (!world)
		return -1;

	if (!groupLead) {
		SetReferences();
		if (!groupLead) {
			return -1;
		}
	}

	if (AICharactersInfo.Num() >= GetMaxSimultaneousAgents()) {
		return -1;
	}

	FAIAgentsInfo newCharacterInfo;

	const int32 localGroupIndex = AICharactersInfo.Num();
	const FVector myLocation = groupLead->GetActorLocation();
	FVector additivePos = FVector::ZeroVector;
	FActorSpawnParameters spawnParam;
	spawnParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	FTransform spawnTransform;
	if (spawnInfo.SpawnTransform.GetLocation() != FVector::ZeroVector) {
		additivePos = spawnInfo.SpawnTransform.GetLocation();
	}
	else {
		additivePos.X = UKismetMathLibrary::RandomFloatInRange(-DefaultSpawnOffset.X, DefaultSpawnOffset.X);
		additivePos.Y = UKismetMathLibrary::RandomFloatInRange(-DefaultSpawnOffset.Y, DefaultSpawnOffset.Y);
	}
	const FVector spawnLocation = myLocation + additivePos;
	FVector outPoint;

	if (UNavigationSystemV1::K2_ProjectPointToNavigation(this, spawnLocation, outPoint, nullptr, nullptr, FVector(500.f))) {
		spawnTransform.SetLocation(outPoint);

	}
	else {
		spawnTransform.SetLocation(spawnLocation);
	}
	spawnTransform.SetRotation(spawnInfo.SpawnTransform.GetRotation());
	newCharacterInfo.AICharacter = world->SpawnActorDeferred<AACFCharacter>(
		spawnInfo.AIClassBP.LoadSynchronous(), spawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (newCharacterInfo.AICharacter) {

		UGameplayStatics::FinishSpawningActor(newCharacterInfo.AICharacter, spawnTransform);

		// End Spawn
		if (!newCharacterInfo.AICharacter->GetController()) {
			newCharacterInfo.AICharacter->SpawnDefaultController();
		}
		InitAgent(newCharacterInfo, localGroupIndex);

		AICharactersInfo.Add(newCharacterInfo);
		return localGroupIndex;
	}
	return -1;
}

int32 UACFGroupAIComponent::GetTotalAIToSpawnCount() const
{

	return AIToSpawn.Num();
}

bool UACFGroupAIComponent::AddExistingCharacterToGroup(AACFCharacter* character)
{
	if (!GetOwner()->HasAuthority()) {
		UE_LOG(ACFAILog, Warning, TEXT("Trying to manipulate AI from client!- UACFGroupAIComponentn"), *this->GetName());

		return false;
	}

	const UWorld* world = GetWorld();

	if (!world) {
		return false;
	}

	if (!groupLead) {
		SetReferences();
	}

	if (AICharactersInfo.Contains(character)) {
		InitAgents();
		return true;
	}

	FAIAgentsInfo newCharacterInfo;
	newCharacterInfo.AICharacter = character;

	if (newCharacterInfo.AICharacter) {
		if (!newCharacterInfo.AICharacter->GetController()) {
			newCharacterInfo.AICharacter->SpawnDefaultController();
		}

		uint8 childIndex = AICharactersInfo.Num();
		newCharacterInfo.GetController() = Cast<AACFAIController>(newCharacterInfo.AICharacter->GetController());
		if (newCharacterInfo.GetController()) {
			InitAgent(newCharacterInfo, childIndex);
		}
		else {
			UE_LOG(ACFAILog, Error, TEXT("%s NO AI to Spawn - AAIFGROUPPAWN::SpawnAGroup"), *this->GetName());
		}

		AICharactersInfo.Add(newCharacterInfo);
		return true;
	}
	return false;
}

void UACFGroupAIComponent::ReInitAgent(AACFCharacter* character)
{
	if (AICharactersInfo.Contains(character)) {
		FAIAgentsInfo* newCharacterInfo = AICharactersInfo.FindByKey(character);
		const int32 index = AICharactersInfo.IndexOfByKey(character);
		InitAgent(*newCharacterInfo, index);
	}
}

AACFCharacter* UACFGroupAIComponent::GetAgentNearestTo(const FVector& location) const
{
	AACFCharacter* bestAgent = nullptr;
	float minDistance = 999999.f;
	for (FAIAgentsInfo achar : AICharactersInfo) {
		if (achar.AICharacter && achar.AICharacter->IsAlive()) {
			const float distance = FVector::Distance(location, achar.AICharacter->GetActorLocation());
			if (distance <= minDistance) {
				minDistance = distance;
				bestAgent = achar.AICharacter;
			}
		}
	}
	return bestAgent;
}

FGameplayTag UACFGroupAIComponent::GetCombatTeam() const
{
	const UACFTeamComponent* teamComp = GetOwner()->FindComponentByClass<UACFTeamComponent>();
	if (teamComp) {
		return teamComp->GetTeam();
	}
	return FGameplayTag();
}

bool UACFGroupAIComponent::RemoveAgentFromGroup(AACFCharacter* character)
{
	if (!character) {
		return false;
	}

	AACFAIController* contr = Cast<AACFAIController>(character->GetController());
	if (!contr) {
		return false;
	}

	const FAIAgentsInfo agentInfo(character);


	if (AICharactersInfo.Contains(agentInfo)) {
		AICharactersInfo.RemoveSingle(agentInfo);
		contr->ResetToDefaultState();
		contr->SetLeadActorBK(nullptr);
		contr->SetGroupOwner(nullptr, 0, false, false);
		return true;
	}

	return false;
}

void UACFGroupAIComponent::SetInBattle(bool inBattle, AActor* newTarget)
{
	bInBattle = inBattle;
	if (bInBattle && GetOwner()->HasAuthority()) {
		const APawn* aiTarget = Cast<APawn>(newTarget);
		if (aiTarget) {

			// Check if target is part of a group
			AController* targetCont = aiTarget->GetController();
			if (targetCont) {
				const bool bImplements = targetCont->GetClass()->ImplementsInterface(UACFGroupAgentInterface::StaticClass());
				if (bImplements && IACFGroupAgentInterface::Execute_IsPartOfGroup(targetCont)) {
					UACFGroupAIComponent* groupComp = IACFGroupAgentInterface::Execute_GetOwningGroup(targetCont);
					SetEnemyGroup(groupComp);
				}
				else {
					enemyGroup = nullptr;
				}
			}
		}

		int32 index = 0;
		for (const FAIAgentsInfo& achar : AICharactersInfo) {
			if (!achar.GetController() || achar.GetController()->IsInBattle() || !achar.AICharacter->IsAlive()) {
				continue;
			}

			// Trying to assign to every agent in the group that is not in battle an enemy in the enemy group
			AActor* nextTarget = newTarget;
			FAIAgentsInfo adversary;
			if (enemyGroup && enemyGroup->GetGroupSize() > 0) {
				if (achar.GetController() && !achar.GetController()->HasTarget()) {
					if (enemyGroup->GetGroupSize() > index) {
						enemyGroup->GetAgentByIndex(index, adversary);
						index++;
					}
					else {
						index = 0;
						enemyGroup->GetAgentByIndex(index, adversary);
						index++;
					}
					nextTarget = adversary.AICharacter;
				}
			}
			UACFThreatManagerComponent* threatComp = achar.GetController()->GetThreatManager();
			if (nextTarget) {
				const float newThreat = threatComp->GetDefaultThreatForActor(nextTarget);
				if (newThreat > 0.f) {
					// if the enemy we found is valid, we add aggro for that enemy
					threatComp->AddThreat(nextTarget, newThreat + 10.f);
				}
				else {
					// otherwise we go in the new target
					threatComp->AddThreat(newTarget, threatComp->GetDefaultThreatForActor(newTarget));
				}
			}
		}
	}
}

void UACFGroupAIComponent::OnChildDeath(const AACFCharacter* character)
{
	const int32 index = AICharactersInfo.IndexOfByKey(character);
	if (AICharactersInfo.IsValidIndex(index)) {
		AICharactersInfo.RemoveAt(index);
	}
	OnAgentDeath.Broadcast(character);
	if (AICharactersInfo.Num() == 0) {
		OnAllAgentDeath.Broadcast();
	}
}
