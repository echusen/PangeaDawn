#include "Actors/MiningSiteActor.h"
#include "Actors/MiningScreenHost.h"

#include "Components/ACFInteractionComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ACFStorageComponent.h"
#include "Actors/MiningDiscoveryNodeActor.h"
#include "Actors/MiningSettlementStockpileActor.h"
#include "Components/MiningSiteComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Interfaces/MiningSitePresentationCoordinatorInterface.h"
#include "SmartObjectComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningSiteActor, Log, All);

AMiningSiteActor::AMiningSiteActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SiteMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SiteMarkerMesh"));
	SiteMarkerMesh->SetupAttachment(SceneRoot);
	SiteMarkerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	SiteChestMarker = CreateDefaultSubobject<USceneComponent>(TEXT("SiteChestMarker"));
	SiteChestMarker->SetupAttachment(SceneRoot);
	SiteChestMarker->SetRelativeLocation(FVector(300.0f, -220.0f, 0.0f));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(150.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 240.0f));
	StatusText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StatusText->SetWorldSize(28.0f);
	StatusText->SetText(FText::FromString(TEXT("Mining Site")));

	UStaticMeshComponent* FireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugCampfireMesh"));
	FireMesh->SetupAttachment(SceneRoot);
	FireMesh->SetRelativeLocation(FVector(-100.0f, 80.0f, 0.0f));
	FireMesh->SetRelativeScale3D(FVector(1.0f));
	FireMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	UStaticMeshComponent* WorkstationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugWorkstationMesh"));
	WorkstationMesh->SetupAttachment(SceneRoot);
	WorkstationMesh->SetRelativeLocation(FVector(-120.0f, -90.0f, 0.0f));
	WorkstationMesh->SetRelativeScale3D(FVector(0.6f));
	WorkstationMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FireAsset(TEXT("/Game/Environment/Oppidam/Decoration/SM_Oppi_Deco_fire_camp_AB.SM_Oppi_Deco_fire_camp_AB"));
	if (FireAsset.Succeeded())
	{
		FireMesh->SetStaticMesh(FireAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> WorkstationAsset(TEXT("/Game/FullSample/Assets/Environment/Meshes/Crops/SM_Stone_03.SM_Stone_03"));
	if (WorkstationAsset.Succeeded())
	{
		WorkstationMesh->SetStaticMesh(WorkstationAsset.Object);
	}

	MiningSiteComponent = CreateDefaultSubobject<UMiningSiteComponent>(TEXT("MiningSiteComponent"));
}

void AMiningSiteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMiningSiteActor, SettlementResourceActor);
}

void AMiningSiteActor::BeginPlay()
{
	Super::BeginPlay();
	if (StatusText) StatusText->SetVisibility(false);

	if (MiningSiteComponent)
	{
		MiningSiteComponent->OnSiteLevelChanged.AddDynamic(this, &ThisClass::HandleSiteLevelChanged);
		MiningSiteComponent->OnStorageChanged.AddDynamic(this, &ThisClass::UpdateStatusText);
		MiningSiteComponent->OnShipmentResolved.AddDynamic(this, &ThisClass::HandleShipmentResolved);
	}

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEnd);
	RefreshLocalInteractionRegistration();

	UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("Mining site actor BeginPlay. Actor=%s HasAuthority=%s Component=%s Definition=%s Settlement=%s Established=%s Level=%d Location=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(MiningSiteComponent),
		MiningSiteComponent ? *GetNameSafe(MiningSiteComponent->SiteDefinition) : TEXT("None"),
		*GetNameSafe(SettlementResourceActor),
		MiningSiteComponent && MiningSiteComponent->IsEstablished() ? TEXT("true") : TEXT("false"),
		MiningSiteComponent ? MiningSiteComponent->GetCurrentLevel() : INDEX_NONE,
		*GetActorLocation().ToString());

	RefreshLevelVisuals();
	DestroySiteChest();
	RefreshSiteChest();
	EnsurePresentationCoordinator();
	ResolveSettlementResourceActor();
	ConfigureSmartObjectComponents();
	DestroyOwnedPresentationActors();
	RefreshPresentationActors();
	UpdatePresentationActorMovement();
	UpdateStatusText();

	if (HasAuthority())
	{
		const IMiningSitePresentationCoordinatorInterface* Coordinator = Cast<IMiningSitePresentationCoordinatorInterface>(PresentationCoordinator);
		const float RefreshInterval = Coordinator ? Coordinator->GetPresentationRefreshInterval() : 1.0f;
		const float MovementInterval = Coordinator ? Coordinator->GetPresentationMovementInterval() : 0.033f;
		GetWorldTimerManager().SetTimer(PresentationRefreshTimerHandle, this, &ThisClass::RefreshPresentationActors, RefreshInterval, true);
		GetWorldTimerManager().SetTimer(PresentationMovementTimerHandle, this, &ThisClass::UpdatePresentationActorMovement, MovementInterval, true);
	}
}

void AMiningSiteActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MiningScreenHost::Close(ActiveMenuWidget.Get());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
		World->GetTimerManager().ClearTimer(PresentationMovementTimerHandle);
	}

	ClearPresentationActors();

	for (const TWeakObjectPtr<UACFInteractionComponent>& InteractionComponent : RegisteredInteractionComponents)
	{
		if (InteractionComponent.IsValid())
		{
			InteractionComponent->UnregisterInteractable(this);
		}
	}

	RegisteredInteractionComponents.Reset();
	Super::EndPlay(EndPlayReason);
}

TArray<UActorComponent*> AMiningSiteActor::GetComponentsToSave_Implementation() const
{
	TArray<UActorComponent*> ComponentsToSave;

	if (MiningSiteComponent)
	{
		ComponentsToSave.Add(MiningSiteComponent);
	}

	return ComponentsToSave;
}

void AMiningSiteActor::OnLoaded_Implementation()
{
	UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("Mining site OnLoaded start. Actor=%s Established=%s Level=%d ExistingChest=%s"),
		*GetNameSafe(this),
		MiningSiteComponent && MiningSiteComponent->IsEstablished() ? TEXT("true") : TEXT("false"),
		MiningSiteComponent ? MiningSiteComponent->GetCurrentLevel() : INDEX_NONE,
		*GetNameSafe(SpawnedSiteChest));

	RefreshLocalInteractionRegistration();
	RefreshLevelVisuals();
	DestroySiteChest();
	RefreshSiteChest();
	EnsurePresentationCoordinator();
	ResolveSettlementResourceActor();
	ConfigureSmartObjectComponents();
	DestroyOwnedPresentationActors();
	ClearPresentationActors();
	RefreshPresentationActors();
	UpdatePresentationActorMovement();
	UpdateStatusText();

	UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("Mining site OnLoaded end. Actor=%s Chest=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedSiteChest));
}

void AMiningSiteActor::DestroySiteChest()
{
	UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("DestroySiteChest called. Actor=%s ExistingChest=%s World=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedSiteChest),
		*GetNameSafe(GetWorld()));

	if (!GetWorld())
	{
		SpawnedSiteChestInteractionSphere = nullptr;
		SpawnedSiteChest = nullptr;
		return;
	}

	if (MiningSiteComponent)
	{
		MiningSiteComponent->SetLinkedStorageComponent(nullptr);
	}

	UClass* ChestClass = nullptr;
	if (MiningSiteComponent && MiningSiteComponent->SiteDefinition && !MiningSiteComponent->SiteDefinition->SiteChestActorClass.IsNull())
	{
		ChestClass = MiningSiteComponent->SiteDefinition->SiteChestActorClass.LoadSynchronous();
	}

	const FName ChestSiteTag = FName(*FString::Printf(TEXT("Mining.SiteChest.%s"), *GetName()));
	TArray<AActor*> ChestsToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* CandidateChest = *It;
		if (!CandidateChest || CandidateChest == this)
		{
			continue;
		}

		const bool bOwnedBySite = CandidateChest->GetOwner() == this;
		const bool bTaggedForSite = CandidateChest->ActorHasTag(ChestSiteTag);
		const bool bMatchesChestClass = ChestClass && CandidateChest->IsA(ChestClass);
		const bool bNearSite = FVector::DistSquared2D(CandidateChest->GetActorLocation(), GetActorLocation()) <= FMath::Square(500.0f);
		if (bOwnedBySite || bTaggedForSite || (SpawnedSiteChest && CandidateChest == SpawnedSiteChest) || (bMatchesChestClass && bNearSite))
		{
			if (!ChestClass || bMatchesChestClass)
			{
				ChestsToDestroy.Add(CandidateChest);
			}
		}
	}

	for (AActor* ChestActor : ChestsToDestroy)
	{
		if (ChestActor)
		{
			UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("DestroySiteChest removing chest actor. Site=%s Chest=%s"),
				*GetNameSafe(this),
				*GetNameSafe(ChestActor));
			ChestActor->Destroy();
		}
	}

	SpawnedSiteChestInteractionSphere = nullptr;
	SpawnedSiteChest = nullptr;
}

void AMiningSiteActor::DestroyOwnedPresentationActors()
{
	if (!GetWorld())
	{
		return;
	}

	FMiningSiteLevelDefinition LevelDefinition;
	const bool bHasLevelDefinition = MiningSiteComponent && MiningSiteComponent->GetCurrentLevelDefinition(LevelDefinition);
	UClass* WorkerClass = bHasLevelDefinition ? LevelDefinition.WorkerClass.LoadSynchronous() : nullptr;
	UClass* GuardClass = bHasLevelDefinition ? LevelDefinition.GuardClass.LoadSynchronous() : nullptr;
	UClass* CourierClass = bHasLevelDefinition ? LevelDefinition.CourierClass.LoadSynchronous() : nullptr;
	const FName SiteTag = FName(*FString::Printf(TEXT("Mining.Site.%s"), *GetName()));

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this)
		{
			continue;
		}

		const bool bIsPresentationActor =
			Candidate->ActorHasTag(TEXT("Mining.Worker")) ||
			Candidate->ActorHasTag(TEXT("Mining.Guard")) ||
			Candidate->ActorHasTag(TEXT("Mining.Courier"));

		const bool bMatchesSiteTag = Candidate->ActorHasTag(SiteTag);
		const bool bOwnedBySite = Candidate->GetOwner() == this && bIsPresentationActor;
		const bool bMatchesConfiguredClass =
			(WorkerClass && Candidate->IsA(WorkerClass)) ||
			(GuardClass && Candidate->IsA(GuardClass)) ||
			(CourierClass && Candidate->IsA(CourierClass));
		const bool bNearSite = FVector::DistSquared2D(Candidate->GetActorLocation(), GetActorLocation()) <= FMath::Square(2500.0f);

		if (!(bOwnedBySite || bMatchesSiteTag || (bMatchesConfiguredClass && bNearSite) || (bIsPresentationActor && bNearSite)))
		{
			continue;
		}

		Candidate->Destroy();
	}

	SpawnedWorkerActors.Reset();
	SpawnedGuardActors.Reset();
	SpawnedCourierActor = nullptr;
}

void AMiningSiteActor::ResolveSettlementResourceActor()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AMiningDiscoveryNodeActor> It(GetWorld()); It; ++It)
	{
		AMiningDiscoveryNodeActor* DiscoveryNode = *It;
		if (!DiscoveryNode || DiscoveryNode->EstablishedSite != this || !DiscoveryNode->SettlementResourceActor)
		{
			continue;
		}

		SettlementResourceActor = DiscoveryNode->SettlementResourceActor;
		return;
	}

	if (SettlementResourceActor && IsValid(SettlementResourceActor))
	{
		return;
	}

	AMiningSettlementStockpileActor* NearestStockpile = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	for (TActorIterator<AMiningSettlementStockpileActor> It(GetWorld()); It; ++It)
	{
		AMiningSettlementStockpileActor* Stockpile = *It;
		if (!Stockpile)
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared2D(Stockpile->GetActorLocation(), GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			NearestStockpile = Stockpile;
		}
	}

	if (NearestStockpile)
	{
		SettlementResourceActor = NearestStockpile;
	}
}

void AMiningSiteActor::RefreshLevelVisuals()
{
	if (!HasAuthority() || !MiningSiteComponent || !MiningSiteComponent->IsEstablished())
	{
		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("RefreshLevelVisuals skipped. Actor=%s HasAuthority=%s Component=%s Established=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(MiningSiteComponent),
			MiningSiteComponent && MiningSiteComponent->IsEstablished() ? TEXT("true") : TEXT("false"));
		return;
	}

	if (SpawnedLevelVisuals)
	{
		SpawnedLevelVisuals->Destroy();
		SpawnedLevelVisuals = nullptr;
	}

	FMiningSiteLevelDefinition LevelDefinition;
	if (!MiningSiteComponent->GetCurrentLevelDefinition(LevelDefinition) || LevelDefinition.VisualSetClass.IsNull())
	{
		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("No visual set configured. Actor=%s Level=%d"),
			*GetNameSafe(this),
			MiningSiteComponent->GetCurrentLevel());
		return;
	}

	UClass* VisualClass = LevelDefinition.VisualSetClass.LoadSynchronous();
	if (!VisualClass)
	{
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("Failed to load visual set. Actor=%s Level=%d"),
			*GetNameSafe(this),
			MiningSiteComponent->GetCurrentLevel());
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedLevelVisuals = GetWorld()->SpawnActor<AActor>(VisualClass, GetActorTransform(), SpawnParameters);
	if (SpawnedLevelVisuals)
	{
		SpawnedLevelVisuals->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("Spawned mining visual set. Actor=%s Visual=%s Level=%d"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedLevelVisuals),
			MiningSiteComponent->GetCurrentLevel());
	}

	UpdateStatusText();
}

void AMiningSiteActor::RefreshSiteChest()
{
	UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("RefreshSiteChest called. Actor=%s Established=%s Level=%d ExistingChest=%s World=%s"),
		*GetNameSafe(this),
		MiningSiteComponent && MiningSiteComponent->IsEstablished() ? TEXT("true") : TEXT("false"),
		MiningSiteComponent ? MiningSiteComponent->GetCurrentLevel() : INDEX_NONE,
		*GetNameSafe(SpawnedSiteChest),
		*GetNameSafe(GetWorld()));

	if (!MiningSiteComponent || !MiningSiteComponent->IsEstablished() || !MiningSiteComponent->SiteDefinition)
	{
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("RefreshSiteChest early-out missing site state. Actor=%s"), *GetNameSafe(this));
		return;
	}

	if (MiningSiteComponent->SiteDefinition->SiteChestActorClass.IsNull())
	{
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("RefreshSiteChest early-out no chest class. Actor=%s Definition=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MiningSiteComponent->SiteDefinition));
		return;
	}

	UClass* ChestClass = MiningSiteComponent->SiteDefinition->SiteChestActorClass.LoadSynchronous();
	const FTransform RelativeTransform = MiningSiteComponent->SiteDefinition->SiteChestRelativeTransform;

	if (!ChestClass)
	{
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("Failed to load site chest class. Actor=%s Definition=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MiningSiteComponent->SiteDefinition));
		return;
	}

	if (IsValid(SpawnedSiteChest) && SpawnedSiteChest->IsA(ChestClass))
	{
		if (MiningSiteComponent)
		{
			MiningSiteComponent->SetLinkedStorageComponent(SpawnedSiteChest->FindComponentByClass<UACFStorageComponent>());
		}
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("RefreshSiteChest reusing existing chest. Actor=%s Chest=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedSiteChest));
		return;
	}

	if (IsValid(SpawnedSiteChest))
	{
		DestroySiteChest();
	}

	const FTransform SpawnTransform = SiteChestMarker
		? SiteChestMarker->GetComponentTransform()
		: RelativeTransform * GetActorTransform();
	const FName ChestSiteTag = FName(*FString::Printf(TEXT("Mining.SiteChest.%s"), *GetName()));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	SpawnedSiteChest = GetWorld()->SpawnActor<AActor>(ChestClass, SpawnTransform, SpawnParameters);
	if (SpawnedSiteChest)
	{
		SpawnedSiteChest->SetReplicates(true);
		SpawnedSiteChest->SetReplicateMovement(true);
		SpawnedSiteChest->SetNetDormancy(DORM_Awake);
		SpawnedSiteChest->bAlwaysRelevant = true;
		SpawnedSiteChest->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		SpawnedSiteChest->Tags.AddUnique(ChestSiteTag);

		TArray<UStaticMeshComponent*> ExistingStaticMeshComponents;
		SpawnedSiteChest->GetComponents<UStaticMeshComponent>(ExistingStaticMeshComponents);
		if (ExistingStaticMeshComponents.IsEmpty())
		{
			UStaticMeshComponent* ChestVisualMesh = NewObject<UStaticMeshComponent>(SpawnedSiteChest, TEXT("MiningSiteChestVisualMesh"));
			if (ChestVisualMesh)
			{
				ChestVisualMesh->SetupAttachment(SpawnedSiteChest->GetRootComponent());
				ChestVisualMesh->SetRelativeLocation(FVector::ZeroVector);
				ChestVisualMesh->SetRelativeRotation(FRotator::ZeroRotator);
				ChestVisualMesh->SetRelativeScale3D(FVector(0.9f));
				ChestVisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

				if (UStaticMesh* ChestMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Environment/Oppidam/Decoration/SM_Oppi_Deco_Chest_AD.SM_Oppi_Deco_Chest_AD")))
				{
					ChestVisualMesh->SetStaticMesh(ChestMeshAsset);
				}

				ChestVisualMesh->RegisterComponent();
			}
		}

		if (MiningSiteComponent)
		{
			ClearSpawnedSiteChestInventory();
			MiningSiteComponent->SetLinkedStorageComponent(SpawnedSiteChest->FindComponentByClass<UACFStorageComponent>());
		}

		SpawnedSiteChestInteractionSphere = NewObject<USphereComponent>(SpawnedSiteChest, TEXT("MiningSiteChestInteractionSphere"));
		if (SpawnedSiteChestInteractionSphere)
		{
			SpawnedSiteChestInteractionSphere->SetupAttachment(SpawnedSiteChest->GetRootComponent());
			SpawnedSiteChestInteractionSphere->SetSphereRadius(140.0f);
			SpawnedSiteChestInteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SpawnedSiteChestInteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			SpawnedSiteChestInteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			SpawnedSiteChestInteractionSphere->RegisterComponent();
			SpawnedSiteChestInteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleSiteChestInteractionBegin);
			SpawnedSiteChestInteractionSphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleSiteChestInteractionEnd);
		}

		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("Spawned mining site chest. Actor=%s Chest=%s ChestClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedSiteChest),
			*GetNameSafe(ChestClass));
		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("Mining site chest transform. Actor=%s MarkerUsed=%s ChestLocation=%s RelativeFallback=%s"),
			*GetNameSafe(this),
			SiteChestMarker ? TEXT("true") : TEXT("false"),
			*SpawnedSiteChest->GetActorLocation().ToString(),
			*RelativeTransform.GetLocation().ToString());
	}
}

void AMiningSiteActor::ClearSpawnedSiteChestInventory()
{
	UACFStorageComponent* StorageComponent = SpawnedSiteChest ? SpawnedSiteChest->FindComponentByClass<UACFStorageComponent>() : nullptr;
	if (!StorageComponent)
	{
		return;
	}

	const TArray<FInventoryItem> ExistingItems = StorageComponent->GetInventory();
	int32 ClearedUnits = 0;
	for (const FInventoryItem& ExistingItem : ExistingItems)
	{
		if (ExistingItem.Count <= 0)
		{
			continue;
		}

		StorageComponent->RemoveItem(ExistingItem, ExistingItem.Count);
		ClearedUnits += ExistingItem.Count;
	}

	if (ClearedUnits > 0)
	{
		UE_LOG(LogPangeaMiningSiteActor, Log, TEXT("Cleared inherited mining chest contents. Actor=%s Chest=%s ClearedUnits=%d"),
			*GetNameSafe(this),
			*GetNameSafe(SpawnedSiteChest),
			ClearedUnits);
	}
}

void AMiningSiteActor::RefreshPresentationActors()
{
	if (IMiningSitePresentationCoordinatorInterface* Coordinator = Cast<IMiningSitePresentationCoordinatorInterface>(PresentationCoordinator))
	{
		Coordinator->RefreshPresentationActors(*this, MiningSiteComponent, SpawnedSiteChest, SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor);
	}
}

void AMiningSiteActor::ClearPresentationActors()
{
	if (IMiningSitePresentationCoordinatorInterface* Coordinator = Cast<IMiningSitePresentationCoordinatorInterface>(PresentationCoordinator))
	{
		Coordinator->ClearPresentationActors(SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor);
	}
}

void AMiningSiteActor::UpdatePresentationActorMovement()
{
	if (IMiningSitePresentationCoordinatorInterface* Coordinator = Cast<IMiningSitePresentationCoordinatorInterface>(PresentationCoordinator))
	{
		Coordinator->UpdatePresentationActorMovement(*this, MiningSiteComponent, SpawnedSiteChest, SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor, WorkerSmartObjectComponents, GuardSmartObjectComponents);
	}
}

void AMiningSiteActor::ConfigureSmartObjectComponents()
{
	if (IMiningSitePresentationCoordinatorInterface* Coordinator = Cast<IMiningSitePresentationCoordinatorInterface>(PresentationCoordinator))
	{
		Coordinator->ConfigureSmartObjectComponents(*this, MiningSiteComponent, WorkerSmartObjectComponents, GuardSmartObjectComponents);
	}
}

void AMiningSiteActor::EnsurePresentationCoordinator()
{
	if (PresentationCoordinator)
	{
		return;
	}

	UClass* CoordinatorClass = LoadClass<UActorComponent>(nullptr, TEXT("/Script/GF_PangeaMiningSystemRuntime.MiningSitePresentationCoordinatorComponent"));
	if (!CoordinatorClass)
	{
		UE_LOG(LogPangeaMiningSiteActor, Warning, TEXT("Failed to load presentation coordinator class. Actor=%s"), *GetNameSafe(this));
		return;
	}

	PresentationCoordinator = NewObject<UActorComponent>(this, CoordinatorClass, TEXT("PresentationCoordinator"));
	if (!PresentationCoordinator)
	{
		return;
	}

	AddInstanceComponent(PresentationCoordinator);
	AddOwnedComponent(PresentationCoordinator);
	PresentationCoordinator->RegisterComponent();
}
