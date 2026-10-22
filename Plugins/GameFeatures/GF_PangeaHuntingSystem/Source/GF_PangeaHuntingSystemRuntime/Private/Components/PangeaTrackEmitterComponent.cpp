#include "Components/PangeaTrackEmitterComponent.h"

#include "Actors/ACFCharacter.h"
#include "Actors/PangeaHuntClueActor.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "DataAssets/HuntSpeciesConfig.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Definitions/PangeaHuntingFragment.h"
#include "Engine/Engine.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "ARSFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace PangeaHuntingDefaults
{
	static constexpr float MinDistanceBetweenTracks = 150.f;
	static constexpr float MinSpeedToLeaveTracks = 50.f;
	static constexpr float TrackLifetime = 120.f;
	static constexpr int32 MaxTrackPoints = 32;
	static constexpr float RevealRadius = 2500.f;
	static constexpr float TrackZOffset = 5.f;
}

namespace PangeaHuntingDebug
{
	static int32 TrackDebugMessageKey = 460100;
}

UPangeaTrackEmitterComponent::UPangeaTrackEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UPangeaTrackEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveConfigFromDefinition();
	if (HuntSpeciesConfig)
	{
		bEnableDebugMessages = HuntSpeciesConfig->bEnableDebugMessages;
	}

	LastTrackLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}

	PrintDebugMessage(FString::Printf(TEXT("Track emitter ready on %s. Config=%s"),
		*GetNameSafe(GetOwner()),
		HuntSpeciesConfig ? *GetNameSafe(HuntSpeciesConfig) : TEXT("FallbackDefaults")), FColor::Yellow, true);

	if (AActor* Owner = GetOwner())
	{
		PrintDebugMessage(FString::Printf(TEXT("Injection confirmed on %s. Authority=%s Replicates=%s"),
			*GetNameSafe(Owner),
			Owner->HasAuthority() ? TEXT("true") : TEXT("false"),
			GetIsReplicated() ? TEXT("true") : TEXT("false")), FColor::Cyan, true);
	}
}

void UPangeaTrackEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority())
	{
		return;
	}

	if (HuntSpeciesConfig && !HuntSpeciesConfig->bLeavesTracks)
	{
		return;
	}

	if (!bRuntimeTrackEmissionEnabled)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	RemoveExpiredTracks(CurrentTimeSeconds);

	const FVector CurrentLocation = Owner->GetActorLocation();
	const float CurrentSpeed = Owner->GetVelocity().Size2D();
	if (!ShouldEmitTrack(CurrentLocation, CurrentSpeed))
	{
		DebugWhyTrackWasSkipped(CurrentLocation, CurrentSpeed, CurrentTimeSeconds);
		return;
	}

	TrackPoints.Add(BuildTrackPoint(CurrentTimeSeconds));
	LastTrackLocation = CurrentLocation;
	bNextTrackIsLeft = !bNextTrackIsLeft;
	PrintDebugMessage(FString::Printf(TEXT("%s emitted track #%d at speed %.1f"),
		*GetNameSafe(Owner),
		TrackPoints.Num(),
		CurrentSpeed), FColor::Orange);

	if (TrackPoints.Last().ClueType != EHuntClueType::Footprint)
	{
		SpawnSpecialClueActor(TrackPoints.Last());
		LastSpecialClueTime = CurrentTimeSeconds;
	}

	const int32 MaxTrackPoints = HuntSpeciesConfig ? HuntSpeciesConfig->MaxTrackPoints : GetDefaultMaxTrackPoints();
	if (TrackPoints.Num() > MaxTrackPoints)
	{
		const int32 ExcessTrackCount = TrackPoints.Num() - MaxTrackPoints;
		TrackPoints.RemoveAt(0, ExcessTrackCount, EAllowShrinking::No);
	}

	Owner->ForceNetUpdate();
}

void UPangeaTrackEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Feature deactivation must remove this emitter's transient clues too, not
	// leave interactable clue actors alive until their independent lifespan expires.
	for (const TWeakObjectPtr<APangeaHuntClueActor>& Clue : SpawnedClues)
		if (Clue.IsValid()) Clue->Destroy();
	SpawnedClues.Empty();
	Super::EndPlay(EndPlayReason);
}

void UPangeaTrackEmitterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPangeaTrackEmitterComponent, HuntSpeciesConfig);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, TrackPoints);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bTrackSetIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bFootprintsIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bBloodIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bBrokenFoliageIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bEatenFoodIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bDroppingsIdentified);
	DOREPLIFETIME(UPangeaTrackEmitterComponent, bRuntimeTrackEmissionEnabled);
}

float UPangeaTrackEmitterComponent::GetRevealRadius() const
{
	return HuntSpeciesConfig ? HuntSpeciesConfig->RevealRadius : GetDefaultRevealRadius();
}

bool UPangeaTrackEmitterComponent::IsClueTypeIdentified(const EHuntClueType ClueType) const
{
	switch (ClueType)
	{
	case EHuntClueType::Footprint:
		return bFootprintsIdentified;
	case EHuntClueType::Blood:
		return bBloodIdentified;
	case EHuntClueType::BrokenFoliage:
		return bBrokenFoliageIdentified;
	case EHuntClueType::EatenFood:
		return bEatenFoodIdentified;
	case EHuntClueType::Droppings:
		return bDroppingsIdentified;
	default:
		return false;
	}
}

void UPangeaTrackEmitterComponent::MarkTrackSetIdentified(AActor* Identifier)
{
	const bool bFootprintChanged = HasTrackType(EHuntClueType::Footprint) && SetClueTypeIdentified(EHuntClueType::Footprint);
	const bool bBloodChanged = HasTrackType(EHuntClueType::Blood) && SetClueTypeIdentified(EHuntClueType::Blood);
	const bool bBrokenFoliageChanged = HasTrackType(EHuntClueType::BrokenFoliage) && SetClueTypeIdentified(EHuntClueType::BrokenFoliage);
	const bool bEatenFoodChanged = HasTrackType(EHuntClueType::EatenFood) && SetClueTypeIdentified(EHuntClueType::EatenFood);
	const bool bDroppingsChanged = HasTrackType(EHuntClueType::Droppings) && SetClueTypeIdentified(EHuntClueType::Droppings);
	const bool bChanged = bFootprintChanged || bBloodChanged || bBrokenFoliageChanged || bEatenFoodChanged || bDroppingsChanged;

	if (!bChanged && bTrackSetIdentified)
	{
		return;
	}

	bTrackSetIdentified = true;

	if (bFootprintChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Footprint, Identifier);
	}
	if (bBloodChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Blood, Identifier);
	}
	if (bBrokenFoliageChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::BrokenFoliage, Identifier);
	}
	if (bEatenFoodChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::EatenFood, Identifier);
	}
	if (bDroppingsChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Droppings, Identifier);
	}

	OnTrackSetIdentified.Broadcast(this, Identifier);

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UPangeaTrackEmitterComponent::MarkClueTypeIdentified(const EHuntClueType ClueType, AActor* Identifier)
{
	const bool bPrimaryChanged = SetClueTypeIdentified(ClueType);
	const bool bFootprintChanged = (ClueType == EHuntClueType::Blood || ClueType == EHuntClueType::BrokenFoliage)
		&& HasTrackType(EHuntClueType::Footprint)
		&& SetClueTypeIdentified(EHuntClueType::Footprint);
	const bool bChanged = bPrimaryChanged || bFootprintChanged;

	if (!bChanged)
	{
		return;
	}

	bTrackSetIdentified = bFootprintsIdentified || bBloodIdentified || bBrokenFoliageIdentified || bEatenFoodIdentified || bDroppingsIdentified;
	if (bPrimaryChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, ClueType, Identifier);
	}
	if (bFootprintChanged)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Footprint, Identifier);
	}
	if (bTrackSetIdentified)
	{
		OnTrackSetIdentified.Broadcast(this, Identifier);
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UPangeaTrackEmitterComponent::SetTrackEmissionEnabled(const bool bEnabled)
{
	if (bRuntimeTrackEmissionEnabled == bEnabled)
	{
		return;
	}

	bRuntimeTrackEmissionEnabled = bEnabled;
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

float UPangeaTrackEmitterComponent::GetDefaultMinDistanceBetweenTracks()
{
	return PangeaHuntingDefaults::MinDistanceBetweenTracks;
}

float UPangeaTrackEmitterComponent::GetDefaultMinSpeedToLeaveTracks()
{
	return PangeaHuntingDefaults::MinSpeedToLeaveTracks;
}

float UPangeaTrackEmitterComponent::GetDefaultTrackLifetime()
{
	return PangeaHuntingDefaults::TrackLifetime;
}

int32 UPangeaTrackEmitterComponent::GetDefaultMaxTrackPoints()
{
	return PangeaHuntingDefaults::MaxTrackPoints;
}

float UPangeaTrackEmitterComponent::GetDefaultRevealRadius()
{
	return PangeaHuntingDefaults::RevealRadius;
}

float UPangeaTrackEmitterComponent::GetDefaultTrackZOffset()
{
	return PangeaHuntingDefaults::TrackZOffset;
}

void UPangeaTrackEmitterComponent::OnRep_TrackPoints()
{
}

void UPangeaTrackEmitterComponent::OnRep_TrackSetIdentified()
{
	if (bTrackSetIdentified)
	{
		OnTrackSetIdentified.Broadcast(this, nullptr);
	}
}

void UPangeaTrackEmitterComponent::OnRep_TrackIdentificationState()
{
	if (bFootprintsIdentified)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Footprint, nullptr);
	}
	if (bBloodIdentified)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Blood, nullptr);
	}
	if (bBrokenFoliageIdentified)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::BrokenFoliage, nullptr);
	}
	if (bEatenFoodIdentified)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::EatenFood, nullptr);
	}
	if (bDroppingsIdentified)
	{
		OnTrackTypeIdentified.Broadcast(this, EHuntClueType::Droppings, nullptr);
	}
}

void UPangeaTrackEmitterComponent::ResolveConfigFromDefinition()
{
	if (HuntSpeciesConfig)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetClass()->ImplementsInterface(UPDDefinitionProviderInterface::StaticClass()))
	{
		return;
	}

	UPangeaCreatureDefinition* Definition = IPDDefinitionProviderInterface::Execute_GetCreatureDefinition(Owner);
	if (!Definition)
	{
		return;
	}

	if (const UPangeaHuntingFragment* HuntingFragment = Definition->GetFragment<UPangeaHuntingFragment>())
	{
		HuntSpeciesConfig = HuntingFragment->HuntSpeciesConfig;
	}
}

void UPangeaTrackEmitterComponent::RemoveExpiredTracks(const float CurrentTimeSeconds)
{
	const int32 RemovedCount = TrackPoints.RemoveAll([CurrentTimeSeconds](const FHuntTrackPoint& TrackPoint)
	{
		return TrackPoint.Lifetime > 0.f && CurrentTimeSeconds >= (TrackPoint.CreatedServerTime + TrackPoint.Lifetime);
	});

	if (RemovedCount > 0 && GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

bool UPangeaTrackEmitterComponent::ShouldEmitTrack(const FVector& CurrentLocation, const float CurrentSpeed) const
{
	const float MinSpeedToLeaveTracks = HuntSpeciesConfig ? HuntSpeciesConfig->MinSpeedToLeaveTracks : GetDefaultMinSpeedToLeaveTracks();
	const float MinDistanceBetweenTracks = HuntSpeciesConfig ? HuntSpeciesConfig->MinDistanceBetweenTracks : GetDefaultMinDistanceBetweenTracks();
	if (CurrentSpeed < MinSpeedToLeaveTracks)
	{
		return false;
	}

	return FVector::DistSquared2D(CurrentLocation, LastTrackLocation) >= FMath::Square(MinDistanceBetweenTracks);
}

FHuntTrackPoint UPangeaTrackEmitterComponent::BuildTrackPoint(const float CurrentTimeSeconds) const
{
	FHuntTrackPoint TrackPoint;
	AActor* Owner = GetOwner();

	if (!Owner)
	{
		return TrackPoint;
	}

	const float TrackZOffset = HuntSpeciesConfig ? HuntSpeciesConfig->TrackZOffset : GetDefaultTrackZOffset();
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector OwnerForward = Owner->GetActorForwardVector();

	TrackPoint.Location = OwnerLocation;
	TrackPoint.Location.Z += TrackZOffset;
	TrackPoint.Rotation = Owner->GetActorRotation();
	TrackPoint.CreatedServerTime = CurrentTimeSeconds;
	TrackPoint.ClueType = DetermineClueType(CurrentTimeSeconds);
	TrackPoint.Lifetime = GetTrackLifetimeForClueType(TrackPoint.ClueType);
	TrackPoint.Strength = 1.f;

	bool bResolvedGround = false;
	if (AACFCharacter* ACFCharacter = Cast<AACFCharacter>(Owner))
	{
		if (UACFCharacterMovementComponent* MovementComponent = ACFCharacter->GetACFCharacterMovementComponent())
		{
			if (MovementComponent->IsSprinting())
			{
				TrackPoint.Strength = 1.25f;
			}

			const FCharacterGroundInfo& GroundInfo = MovementComponent->GetGroundInfo();
			if (const UPhysicalMaterial* PhysicalMaterial = GroundInfo.GroundHitResult.PhysMaterial.Get())
			{
				TrackPoint.SurfaceTag = FGameplayTag::RequestGameplayTag(PhysicalMaterial->GetFName(), false);
			}

			if (GroundInfo.GroundHitResult.bBlockingHit)
			{
				const FVector GroundNormal = GroundInfo.GroundHitResult.ImpactNormal.GetSafeNormal();
				FVector ProjectedForward = FVector::VectorPlaneProject(OwnerForward, GroundNormal).GetSafeNormal();
				if (ProjectedForward.IsNearlyZero())
				{
					ProjectedForward = FVector::CrossProduct(GroundNormal, Owner->GetActorRightVector()).GetSafeNormal();
				}

				const FVector ProjectedRight = FVector::VectorPlaneProject(Owner->GetActorRightVector(), GroundNormal).GetSafeNormal();
				const float FootOffset = HuntSpeciesConfig ? HuntSpeciesConfig->FootLateralOffset : 28.f;
				const FVector FootLateralOffset = ProjectedRight * (bNextTrackIsLeft ? -FootOffset : FootOffset);
				TrackPoint.Location = GroundInfo.GroundHitResult.ImpactPoint + FootLateralOffset + (GroundNormal * TrackZOffset);
				TrackPoint.Rotation = FRotationMatrix::MakeFromXZ(ProjectedForward, GroundNormal).Rotator();
				bResolvedGround = true;
			}
		}
	}

	if (!bResolvedGround)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FHitResult GroundHit;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PangeaHuntingTrackSnap), false, Owner);
			const FVector TraceStart = OwnerLocation + FVector(0.f, 0.f, 150.f);
			const FVector TraceEnd = OwnerLocation - FVector(0.f, 0.f, 1000.f);

			if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) && GroundHit.bBlockingHit)
			{
				const FVector GroundNormal = GroundHit.ImpactNormal.GetSafeNormal();
				FVector ProjectedForward = FVector::VectorPlaneProject(OwnerForward, GroundNormal).GetSafeNormal();
				if (ProjectedForward.IsNearlyZero())
				{
					ProjectedForward = FVector::ForwardVector;
				}

				const FVector ProjectedRight = FVector::VectorPlaneProject(Owner->GetActorRightVector(), GroundNormal).GetSafeNormal();
				const float FootOffset = HuntSpeciesConfig ? HuntSpeciesConfig->FootLateralOffset : 28.f;
				const FVector FootLateralOffset = ProjectedRight * (bNextTrackIsLeft ? -FootOffset : FootOffset);
				TrackPoint.Location = GroundHit.ImpactPoint + FootLateralOffset + (GroundNormal * TrackZOffset);
				TrackPoint.Rotation = FRotationMatrix::MakeFromXZ(ProjectedForward, GroundNormal).Rotator();
			}
		}
	}

	return TrackPoint;
}

EHuntClueType UPangeaTrackEmitterComponent::DetermineClueType(const float CurrentTimeSeconds) const
{
	const float MinSpecialClueInterval = HuntSpeciesConfig ? HuntSpeciesConfig->MinSpecialClueInterval : 4.0f;
	const float MaxSpecialClueInterval = HuntSpeciesConfig && HuntSpeciesConfig->bUseRandomSpecialClueInterval
		? FMath::Max(MinSpecialClueInterval, HuntSpeciesConfig->MaxSpecialClueInterval)
		: MinSpecialClueInterval;
	const float RequiredSpecialClueInterval = MaxSpecialClueInterval > MinSpecialClueInterval
		? FMath::FRandRange(MinSpecialClueInterval, MaxSpecialClueInterval)
		: MinSpecialClueInterval;
	if ((CurrentTimeSeconds - LastSpecialClueTime) < RequiredSpecialClueInterval)
	{
		return EHuntClueType::Footprint;
	}

	const AACFCharacter* ACFCharacter = Cast<AACFCharacter>(GetOwner());
	const UACFCharacterMovementComponent* MovementComponent = ACFCharacter ? ACFCharacter->GetACFCharacterMovementComponent() : nullptr;
	const UACFGASStatisticsComponent* StatisticsComponent = GetOwner() ? GetOwner()->FindComponentByClass<UACFGASStatisticsComponent>() : nullptr;

	if (StatisticsComponent && HuntSpeciesConfig && HuntSpeciesConfig->bSpawnBloodClues)
	{
		const float NormalizedHealth = StatisticsComponent->GetNormalizedValueForStatitstic(UARSFunctionLibrary::GetHealthTag());
		if (NormalizedHealth > 0.f && NormalizedHealth <= HuntSpeciesConfig->BloodHealthThreshold)
		{
			return EHuntClueType::Blood;
		}
	}

	if (MovementComponent && HuntSpeciesConfig && HuntSpeciesConfig->bSpawnBrokenFoliageClues && MovementComponent->IsSprinting())
	{
		return EHuntClueType::BrokenFoliage;
	}

	if (const EHuntClueType AdditionalClueType = DetermineAdditionalClueType(CurrentTimeSeconds); AdditionalClueType != EHuntClueType::Footprint)
	{
		return AdditionalClueType;
	}

	return EHuntClueType::Footprint;
}

FString UPangeaTrackEmitterComponent::ResolveSourceCreatureName() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return TEXT("Unknown Creature");
	}

	if (Owner->GetClass()->ImplementsInterface(UPDDefinitionProviderInterface::StaticClass()))
	{
		if (UPangeaCreatureDefinition* Definition = IPDDefinitionProviderInterface::Execute_GetCreatureDefinition(Owner))
		{
			if (!Definition->SpeciesId.IsNone())
			{
				return Definition->SpeciesId.ToString();
			}
		}
	}

	return Owner->GetName();
}

void UPangeaTrackEmitterComponent::SpawnSpecialClueActor(const FHuntTrackPoint& TrackPoint)
{
	if (TrackPoint.ClueType == EHuntClueType::Footprint)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TSubclassOf<APangeaHuntClueActor> ClueActorClass = HuntSpeciesConfig && HuntSpeciesConfig->SpecialClueActorClass
		? HuntSpeciesConfig->SpecialClueActorClass
		: TSubclassOf<APangeaHuntClueActor>(APangeaHuntClueActor::StaticClass());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (APangeaHuntClueActor* ClueActor = World->SpawnActor<APangeaHuntClueActor>(ClueActorClass, TrackPoint.Location, TrackPoint.Rotation, SpawnParameters))
	{
		ClueActor->InitializeClue(TrackPoint, ResolveSourceCreatureName());
		SpawnedClues.RemoveAll([](const TWeakObjectPtr<APangeaHuntClueActor>& Clue) { return !Clue.IsValid(); });
		SpawnedClues.Add(ClueActor);

		if (TrackPoint.ClueType == EHuntClueType::Blood && HuntSpeciesConfig)
		{
			ClueActor->SetBloodDecalMaterial(HuntSpeciesConfig->BloodDecalMaterial.Get());
			ClueActor->SetBloodDecalSize(HuntSpeciesConfig->BloodDecalSize);
			ClueActor->SetInteractionRadius(HuntSpeciesConfig->BloodInteractionRadius);
		}
		else if (TrackPoint.ClueType == EHuntClueType::BrokenFoliage && HuntSpeciesConfig)
		{
			ClueActor->SetBrokenFoliageDecalMaterial(HuntSpeciesConfig->BrokenFoliageDecalMaterial.Get());
			ClueActor->SetBrokenFoliageDecalSize(HuntSpeciesConfig->BrokenFoliageDecalSize);
			ClueActor->SetInteractionRadius(HuntSpeciesConfig->BrokenFoliageInteractionRadius);
		}
		else if (const FHuntSpecialClueConfig* SpecialClueConfig = FindAdditionalClueConfig(TrackPoint.ClueType))
		{
			ClueActor->SetGenericDecalMaterial(SpecialClueConfig->DecalMaterial.Get());
			ClueActor->SetGenericDecalSize(SpecialClueConfig->DecalSize);
			ClueActor->SetInteractionRadius(SpecialClueConfig->InteractionRadius);
		}
	}
}

const FHuntSpecialClueConfig* UPangeaTrackEmitterComponent::FindAdditionalClueConfig(const EHuntClueType ClueType) const
{
	if (!HuntSpeciesConfig)
	{
		return nullptr;
	}

	return HuntSpeciesConfig->AdditionalSpecialClues.FindByPredicate([ClueType](const FHuntSpecialClueConfig& ClueConfig)
	{
		return ClueConfig.ClueType == ClueType;
	});
}

float UPangeaTrackEmitterComponent::GetTrackLifetimeForClueType(const EHuntClueType ClueType) const
{
	if (!HuntSpeciesConfig)
	{
		return GetDefaultTrackLifetime();
	}

	switch (ClueType)
	{
	case EHuntClueType::Blood:
		return HuntSpeciesConfig->BloodClueLifetime;
	case EHuntClueType::BrokenFoliage:
		return HuntSpeciesConfig->BrokenFoliageClueLifetime;
	case EHuntClueType::Footprint:
		return HuntSpeciesConfig->TrackLifetime;
	default:
		if (const FHuntSpecialClueConfig* ClueConfig = FindAdditionalClueConfig(ClueType))
		{
			return ClueConfig->Lifetime;
		}
		return HuntSpeciesConfig->TrackLifetime;
	}
}

EHuntClueType UPangeaTrackEmitterComponent::DetermineAdditionalClueType(const float CurrentTimeSeconds) const
{
	if (!HuntSpeciesConfig)
	{
		return EHuntClueType::Footprint;
	}

	for (const FHuntSpecialClueConfig& ClueConfig : HuntSpeciesConfig->AdditionalSpecialClues)
	{
		if (!ClueConfig.bEnabled || ClueConfig.ClueType == EHuntClueType::Footprint || ClueConfig.ClueType == EHuntClueType::Blood || ClueConfig.ClueType == EHuntClueType::BrokenFoliage)
		{
			continue;
		}

		const float MinInterval = FMath::Max(0.f, ClueConfig.MinInterval);
		const float MaxInterval = FMath::Max(MinInterval, ClueConfig.MaxInterval);
		const float RequiredInterval = MaxInterval > MinInterval ? FMath::FRandRange(MinInterval, MaxInterval) : MinInterval;
		if ((CurrentTimeSeconds - LastSpecialClueTime) < RequiredInterval)
		{
			continue;
		}

		if (!ClueConfig.bUseRandomChance || FMath::FRand() <= ClueConfig.ChancePerEligibleTrack)
		{
			return ClueConfig.ClueType;
		}
	}

	return EHuntClueType::Footprint;
}

bool UPangeaTrackEmitterComponent::HasTrackType(const EHuntClueType ClueType) const
{
	return TrackPoints.ContainsByPredicate([ClueType](const FHuntTrackPoint& TrackPoint)
	{
		return TrackPoint.ClueType == ClueType;
	});
}

bool UPangeaTrackEmitterComponent::SetClueTypeIdentified(const EHuntClueType ClueType)
{
	bool* IdentificationFlag = nullptr;
	switch (ClueType)
	{
	case EHuntClueType::Footprint:
		IdentificationFlag = &bFootprintsIdentified;
		break;
	case EHuntClueType::Blood:
		IdentificationFlag = &bBloodIdentified;
		break;
	case EHuntClueType::BrokenFoliage:
		IdentificationFlag = &bBrokenFoliageIdentified;
		break;
	case EHuntClueType::EatenFood:
		IdentificationFlag = &bEatenFoodIdentified;
		break;
	case EHuntClueType::Droppings:
		IdentificationFlag = &bDroppingsIdentified;
		break;
	default:
		break;
	}

	if (!IdentificationFlag || *IdentificationFlag)
	{
		return false;
	}

	*IdentificationFlag = true;
	return true;
}

void UPangeaTrackEmitterComponent::DebugWhyTrackWasSkipped(const FVector& CurrentLocation, const float CurrentSpeed, const float CurrentTimeSeconds)
{
	if (!bEnableDebugMessages)
	{
		return;
	}

	if ((CurrentTimeSeconds - LastSkipDebugTime) < 1.0f)
	{
		return;
	}

	LastSkipDebugTime = CurrentTimeSeconds;

	const float MinSpeedToLeaveTracks = HuntSpeciesConfig ? HuntSpeciesConfig->MinSpeedToLeaveTracks : GetDefaultMinSpeedToLeaveTracks();
	const float MinDistanceBetweenTracks = HuntSpeciesConfig ? HuntSpeciesConfig->MinDistanceBetweenTracks : GetDefaultMinDistanceBetweenTracks();
	const float DistanceSinceLastTrack = FVector::Dist2D(CurrentLocation, LastTrackLocation);

	if (CurrentSpeed < MinSpeedToLeaveTracks)
	{
		PrintDebugMessage(
			FString::Printf(TEXT("%s skipping track: speed %.1f < %.1f"),
				*GetNameSafe(GetOwner()),
				CurrentSpeed,
				MinSpeedToLeaveTracks),
			FColor::Red);
		return;
	}

	if (DistanceSinceLastTrack < MinDistanceBetweenTracks)
	{
		PrintDebugMessage(
			FString::Printf(TEXT("%s skipping track: distance %.1f < %.1f"),
				*GetNameSafe(GetOwner()),
				DistanceSinceLastTrack,
				MinDistanceBetweenTracks),
			FColor::Red);
		return;
	}

	PrintDebugMessage(
		FString::Printf(TEXT("%s skipping track: unknown reason, speed %.1f distance %.1f"),
			*GetNameSafe(GetOwner()),
			CurrentSpeed,
			DistanceSinceLastTrack),
		FColor::Red);
}

void UPangeaTrackEmitterComponent::PrintDebugMessage(const FString& Message, const FColor& Color, const bool bLogAlso) const
{
	if (!bEnableDebugMessages)
	{
		return;
	}

	if (GEngine)
	{
		const uint64 Key = static_cast<uint64>(PangeaHuntingDebug::TrackDebugMessageKey++);
		GEngine->AddOnScreenDebugMessage(Key, DebugMessageDuration, Color, FString::Printf(TEXT("[Hunting] %s"), *Message));
	}

	if (bLogAlso)
	{
		UE_LOG(LogTemp, Log, TEXT("[Hunting] %s"), *Message);
	}
}
