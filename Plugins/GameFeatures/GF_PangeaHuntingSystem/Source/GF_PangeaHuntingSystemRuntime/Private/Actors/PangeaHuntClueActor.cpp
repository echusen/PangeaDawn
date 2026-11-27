#include "Actors/PangeaHuntClueActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

#include "Actors/ACFCharacter.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/PangeaTrackEmitterComponent.h"
#include "Game/ACFFunctionLibrary.h"

APangeaHuntClueActor::APangeaHuntClueActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);
	VisualMesh->SetCastShadow(false);
	VisualMesh->SetReceivesDecals(false);
	VisualMesh->SetHiddenInGame(true);

	BloodDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BloodDecal"));
	BloodDecal->SetupAttachment(RootComponent);
	BloodDecal->SetHiddenInGame(true);
	BloodDecal->SetFadeScreenSize(0.0001f);
	BloodDecal->SetSortOrder(10);
	BloodDecal->DecalSize = BloodDecalSize;
}

void APangeaHuntClueActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisualStyle();
	SetActorHiddenInGame(true);
}

void APangeaHuntClueActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RegisterForLocalInteraction(false);
	Super::EndPlay(EndPlayReason);
}

void APangeaHuntClueActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APangeaHuntClueActor, ClueType);
	DOREPLIFETIME(APangeaHuntClueActor, SourceCreatureName);
	DOREPLIFETIME(APangeaHuntClueActor, CreatedServerTime);
	DOREPLIFETIME(APangeaHuntClueActor, Lifetime);
	DOREPLIFETIME(APangeaHuntClueActor, bClueIdentified);
}

void APangeaHuntClueActor::InitializeClue(const FHuntTrackPoint& InTrackPoint, const FString& InSourceCreatureName)
{
	ClueType = InTrackPoint.ClueType;
	SourceCreatureName = InSourceCreatureName;
	CreatedServerTime = InTrackPoint.CreatedServerTime;
	Lifetime = InTrackPoint.Lifetime;

	SetActorLocationAndRotation(InTrackPoint.Location, InTrackPoint.Rotation);
	SetLifeSpan(FMath::Max(1.0f, Lifetime));
	ApplyVisualStyle();
}

void APangeaHuntClueActor::SetBloodDecalMaterial(UMaterialInterface* InMaterial)
{
	BloodDecalMaterialOverride = InMaterial;
	DecalMaterialInstance = nullptr;
	ApplyVisualStyle();
}

void APangeaHuntClueActor::SetBloodDecalSize(const FVector& InDecalSize)
{
	BloodDecalSize = InDecalSize;
	InteractionRadius = FMath::Max(InteractionRadius, FMath::Max(BloodDecalSize.Y, BloodDecalSize.Z));

	if (BloodDecal != nullptr)
	{
		BloodDecal->DecalSize = BloodDecalSize;
	}
}

void APangeaHuntClueActor::SetBrokenFoliageDecalMaterial(UMaterialInterface* InMaterial)
{
	BrokenFoliageDecalMaterialOverride = InMaterial;
	DecalMaterialInstance = nullptr;
	ApplyVisualStyle();
}

void APangeaHuntClueActor::SetBrokenFoliageDecalSize(const FVector& InDecalSize)
{
	BrokenFoliageDecalSize = InDecalSize;
	InteractionRadius = FMath::Max(InteractionRadius, FMath::Max(BrokenFoliageDecalSize.Y, BrokenFoliageDecalSize.Z));

	if (BloodDecal != nullptr && ClueType == EHuntClueType::BrokenFoliage)
	{
		BloodDecal->DecalSize = BrokenFoliageDecalSize;
	}
}

void APangeaHuntClueActor::SetGenericDecalMaterial(UMaterialInterface* InMaterial)
{
	GenericDecalMaterialOverride = InMaterial;
	DecalMaterialInstance = nullptr;
	ApplyVisualStyle();
}

void APangeaHuntClueActor::SetGenericDecalSize(const FVector& InDecalSize)
{
	GenericDecalSize = InDecalSize;
	InteractionRadius = FMath::Max(InteractionRadius, FMath::Max(GenericDecalSize.Y, GenericDecalSize.Z));

	if (BloodDecal != nullptr && ClueType != EHuntClueType::Blood && ClueType != EHuntClueType::BrokenFoliage)
	{
		BloodDecal->DecalSize = GenericDecalSize;
	}
}

void APangeaHuntClueActor::SetInteractionRadius(const float InInteractionRadius)
{
	const FVector* ActiveDecalSize = &GenericDecalSize;

	if (ClueType == EHuntClueType::Blood)
	{
		ActiveDecalSize = &BloodDecalSize;
	}
	else if (ClueType == EHuntClueType::BrokenFoliage)
	{
		ActiveDecalSize = &BrokenFoliageDecalSize;
	}

	const float MinimumVisualRadius = FMath::Max(ActiveDecalSize->Y, ActiveDecalSize->Z);
	InteractionRadius = FMath::Max3(1.f, InInteractionRadius, MinimumVisualRadius);
}

void APangeaHuntClueActor::SetRevealState(const bool bRevealed, const float VisibilityAlpha)
{
	bIsRevealedLocally = bRevealed;
	SetActorHiddenInGame(!bRevealed);
	const bool bUsesDecal = ClueType != EHuntClueType::Footprint;
	VisualMesh->SetHiddenInGame(!bRevealed || bUsesDecal);
	BloodDecal->SetHiddenInGame(!bRevealed || !bUsesDecal);
	RegisterForLocalInteraction(bRevealed);

	const float ClampedAlpha = FMath::Clamp(VisibilityAlpha, 0.f, 1.f);

	if (MeshMaterialInstance != nullptr)
	{
		MeshMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), ClampedAlpha);
		MeshMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Lerp(1.0f, 10.0f, ClampedAlpha));
	}

	if (DecalMaterialInstance != nullptr)
	{
		DecalMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), ClampedAlpha);
		DecalMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Lerp(0.0f, 0.45f, ClampedAlpha));
	}

	if (UACFInteractionComponent* InteractionComponent = RegisteredInteractionComponent.Get())
	{
		InteractionComponent->RefreshInteractions();
	}
}

void APangeaHuntClueActor::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	bClueIdentified = true;
	MarkOwningTrackSetIdentified(Pawn);

	UE_LOG(LogTemp, Log, TEXT("[Hunting] %s belongs to %s"), *ResolveClueDisplayName().ToString(), *SourceCreatureName);
}

void APangeaHuntClueActor::OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UE_LOG(LogTemp, Log, TEXT("[Hunting] %s belongs to %s"), *ResolveClueDisplayName().ToString(), *SourceCreatureName);
}

FText APangeaHuntClueActor::GetInteractableName_Implementation()
{
	if (ClueType == EHuntClueType::Blood)
	{
		return FText::FromString(TEXT("Inspect Blood"));
	}

	if (ClueType == EHuntClueType::BrokenFoliage)
	{
		return FText::FromString(TEXT("Inspect Broken Foliage"));
	}

	return FText::Format(FText::FromString(TEXT("Inspect {0}")), ResolveClueDisplayName());
}

bool APangeaHuntClueActor::CanBeInteracted_Implementation(APawn* Pawn)
{
	return bIsRevealedLocally && Pawn != nullptr && FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void APangeaHuntClueActor::OnRep_ClueData()
{
	ApplyVisualStyle();
}

void APangeaHuntClueActor::ApplyVisualStyle()
{
	if (VisualMesh == nullptr)
	{
		return;
	}

	VisualMesh->SetStaticMesh(ResolveFallbackMesh());
	VisualMesh->SetHiddenInGame(true);
	BloodDecal->SetHiddenInGame(true);

	const bool bIsBlood = ClueType == EHuntClueType::Blood;
	const bool bIsBrokenFoliage = ClueType == EHuntClueType::BrokenFoliage;
	const bool bUsesDecal = ClueType != EHuntClueType::Footprint;
	VisualMesh->SetWorldScale3D(ClueType == EHuntClueType::BrokenFoliage ? FVector(0.5f, 0.25f, 1.0f) : FVector(0.45f, 0.45f, 1.0f));

	if (BloodDecal != nullptr)
	{
		if (bIsBlood)
		{
			BloodDecal->DecalSize = BloodDecalSize;
		}
		else if (bIsBrokenFoliage)
		{
			BloodDecal->DecalSize = BrokenFoliageDecalSize;
		}
		else
		{
			BloodDecal->DecalSize = GenericDecalSize;
		}

		BloodDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

		UMaterialInterface* DecalMaterial = nullptr;

		if (bIsBrokenFoliage)
		{
			DecalMaterial =
				BrokenFoliageDecalMaterialOverride != nullptr ? BrokenFoliageDecalMaterialOverride.Get() : ResolveFallbackBrokenFoliageDecalMaterial();
		}
		else if (bIsBlood)
		{
			DecalMaterial = BloodDecalMaterialOverride != nullptr ? BloodDecalMaterialOverride.Get() : ResolveFallbackBloodDecalMaterial();
		}
		else
		{
			DecalMaterial = GenericDecalMaterialOverride != nullptr ? GenericDecalMaterialOverride.Get() : ResolveFallbackGenericDecalMaterial();
		}

		if (DecalMaterialInstance == nullptr && DecalMaterial != nullptr)
		{
			DecalMaterialInstance = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		}

		if (DecalMaterialInstance != nullptr)
		{
			FLinearColor DecalTint = FLinearColor(0.35f, 0.22f, 0.08f, 1.0f);

			if (bIsBrokenFoliage)
			{
				DecalTint = FLinearColor(0.22f, 0.16f, 0.06f, 1.0f);
			}
			else if (bIsBlood)
			{
				DecalTint = FLinearColor(0.42f, 0.015f, 0.01f, 1.0f);
			}

			DecalMaterialInstance->SetVectorParameterValue(TEXT("Tint"), DecalTint);
			DecalMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			DecalMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.0f);
			BloodDecal->SetDecalMaterial(DecalMaterialInstance);
		}
	}

	if (bUsesDecal)
	{
		return;
	}

	UMaterialInterface* const BaseMaterial = ResolveFallbackMaterial();
	if (MeshMaterialInstance == nullptr && BaseMaterial != nullptr)
	{
		MeshMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	if (MeshMaterialInstance != nullptr)
	{
		const FLinearColor Tint = ClueType == EHuntClueType::Blood ? FLinearColor(1.0f, 0.1f, 0.08f, 1.0f) : FLinearColor(0.45f, 0.95f, 0.18f, 1.0f);
		MeshMaterialInstance->SetVectorParameterValue(TEXT("Tint"), Tint);
		MeshMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		MeshMaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.0f);
		VisualMesh->SetMaterial(0, MeshMaterialInstance);
	}
}

void APangeaHuntClueActor::RegisterForLocalInteraction(const bool bShouldRegister)
{
	AACFCharacter* const LocalCharacter = UACFFunctionLibrary::GetLocalACFPlayerCharacter(this);
	UACFInteractionComponent* const InteractionComponent =
		LocalCharacter != nullptr ? LocalCharacter->GetComponentByClass<UACFInteractionComponent>() : nullptr;

	if (bShouldRegister)
	{
		if (InteractionComponent == nullptr || bRegisteredForLocalInteraction)
		{
			return;
		}

		InteractionComponent->RegisterInteractable(this);
		RegisteredInteractionComponent = InteractionComponent;
		bRegisteredForLocalInteraction = true;

		return;
	}

	UACFInteractionComponent* ComponentToUnregister = RegisteredInteractionComponent.Get();
	if (ComponentToUnregister == nullptr)
	{
		ComponentToUnregister = InteractionComponent;
	}

	if (ComponentToUnregister != nullptr)
	{
		ComponentToUnregister->UnregisterInteractable(this);
		if (ComponentToUnregister->GetCurrentBestInteractableActor() == this)
		{
			ComponentToUnregister->SetCurrentBestInteractable(nullptr);
		}
	}

	RegisteredInteractionComponent.Reset();
	bRegisteredForLocalInteraction = false;
}

void APangeaHuntClueActor::MarkOwningTrackSetIdentified(APawn* Pawn)
{
	AActor* const SourceActor = GetOwner();
	UPangeaTrackEmitterComponent* const TrackEmitter = SourceActor != nullptr ? SourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() : nullptr;
	if (TrackEmitter != nullptr)
	{
		TrackEmitter->MarkClueTypeIdentified(ClueType, Pawn);
	}
}

UStaticMesh* APangeaHuntClueActor::ResolveFallbackMesh() const
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
}

UMaterialInterface* APangeaHuntClueActor::ResolveFallbackMaterial() const
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/GF_PangeaHuntingSystem/Materials/M_HuntTrack_Visual.M_HuntTrack_Visual"));
}

UMaterialInterface* APangeaHuntClueActor::ResolveFallbackBloodDecalMaterial() const
{
	if (UMaterialInterface* BloodMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/_Game/HuntingSystem/M_HuntBlood_Decal.M_HuntBlood_Decal")))
	{
		return BloodMaterial;
	}

	return ResolveFallbackMaterial();
}

UMaterialInterface* APangeaHuntClueActor::ResolveFallbackBrokenFoliageDecalMaterial() const
{
	if (UMaterialInterface* BrokenFoliageMaterial =
			LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/_Game/HuntingSystem/M_HuntBrokenFoliage_Decal.M_HuntBrokenFoliage_Decal")))
	{
		return BrokenFoliageMaterial;
	}

	return ResolveFallbackMaterial();
}

UMaterialInterface* APangeaHuntClueActor::ResolveFallbackGenericDecalMaterial() const
{
	return ResolveFallbackBrokenFoliageDecalMaterial();
}

FText APangeaHuntClueActor::ResolveClueDisplayName() const
{
	switch (ClueType)
	{
		case EHuntClueType::Blood:
			return FText::FromString(TEXT("Blood"));
		case EHuntClueType::BrokenFoliage:
			return FText::FromString(TEXT("Broken Foliage"));
		case EHuntClueType::EatenFood:
			return FText::FromString(TEXT("Eaten Food"));
		case EHuntClueType::Droppings:
			return FText::FromString(TEXT("Droppings"));
		case EHuntClueType::Scent:
			return FText::FromString(TEXT("Scent"));
		default:
			return FText::FromString(TEXT("Clue"));
	}
}
