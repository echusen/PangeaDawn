#include "Components/PangeaHunterSenseComponent.h"

#include "Actors/ACFCharacter.h"
#include "Actors/PangeaHuntClueActor.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/PangeaTrackEmitterComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataAssets/HuntSpeciesConfig.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UI/PangeaFootprintFocusWidget.h"

namespace PangeaHuntingDebug
{
	static int32 HunterDebugMessageKey = 450100;
	static const TCHAR* FallbackTrackMaterialPath = TEXT("/GF_PangeaHuntingSystem/Materials/M_HuntTrack_Visual.M_HuntTrack_Visual");
}

UPangeaHunterSenseComponent::UPangeaHunterSenseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

bool UPangeaHunterSenseComponent::IdentifyNearestVisibleFootprint()
{
	AActor* SourceActor = FocusedFootprintSourceActor.IsValid()
		? FocusedFootprintSourceActor.Get()
		: NearestVisibleFootprintSourceActor.Get();
	if (!SourceActor)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		if (UPangeaTrackEmitterComponent* TrackEmitter = SourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>())
		{
			if (TrackEmitter->IsClueTypeIdentified(EHuntClueType::Footprint))
			{
				return false;
			}

			TrackEmitter->MarkClueTypeIdentified(EHuntClueType::Footprint, Owner);
			return true;
		}
		return false;
	}

	ServerIdentifyFootprints(SourceActor);
	return true;
}

void UPangeaHunterSenseComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveOwnerState();
	EnsureFootprintFocusWidget();
	RefreshSenseState();
	PrintDebugMessage(FString::Printf(TEXT("Hunter sense ready on %s"), *GetNameSafe(GetOwner())), FColor::Cyan, true);
}

void UPangeaHunterSenseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AACFCharacter* OwnerCharacter = CachedOwnerCharacter.Get())
	{
		OwnerCharacter->OnCrouchStateChanged.RemoveDynamic(this, &UPangeaHunterSenseComponent::HandleCrouchChanged);
	}

	if (UACFCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get())
	{
		MovementComponent->OnAimChanged.RemoveDynamic(this, &UPangeaHunterSenseComponent::HandleAimChanged);
	}

	if (FootprintFocusWidget)
	{
		FootprintFocusWidget->RemoveFromParent();
		FootprintFocusWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UPangeaHunterSenseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bHuntingSenseActive || !IsLocallyControlledHunter())
	{
		if (bUsePlaceholderMeshVisuals)
		{
			ResetVisualAssignments();
			ReleaseUnusedVisuals();
		}
		ResetFootprintFocus();
		return;
	}

	UpdateFootprintFocus(DeltaTime);

	ScanAccumulator += DeltaTime;
	if (ScanAccumulator < ScanInterval)
	{
		return;
	}

	ScanAccumulator = 0.f;
	DrawNearbyTracks();
	UpdateTrackVisuals();
}

void UPangeaHunterSenseComponent::HandleAimChanged(const bool bIsNowAiming)
{
	bIsAiming = bIsNowAiming;
	PrintDebugMessage(FString::Printf(TEXT("Aim changed delegate=%s"), bIsAiming ? TEXT("true") : TEXT("false")), FColor::Silver);
	RefreshSenseState();
}

void UPangeaHunterSenseComponent::HandleCrouchChanged(const bool bIsNowCrouched)
{
	bIsCrouched = !bIsNowCrouched;
	if (AACFCharacter* OwnerCharacter = CachedOwnerCharacter.Get())
	{
		PrintDebugMessage(
			FString::Printf(TEXT("Crouch changed delegate=%s inverted=%s actualFlag=%s"),
				bIsNowCrouched ? TEXT("true") : TEXT("false"),
				bIsCrouched ? TEXT("true") : TEXT("false"),
				OwnerCharacter->bIsCrouched ? TEXT("true") : TEXT("false")),
			FColor::Silver,
			true);
	}
	RefreshSenseState();
}

void UPangeaHunterSenseComponent::ResolveOwnerState()
{
	AACFCharacter* OwnerCharacter = Cast<AACFCharacter>(GetOwner());
	CachedOwnerCharacter = OwnerCharacter;

	if (!OwnerCharacter)
	{
		SetComponentTickEnabled(false);
		return;
	}

	UACFCharacterMovementComponent* MovementComponent = OwnerCharacter->GetACFCharacterMovementComponent();
	CachedMovementComponent = MovementComponent;

	if (MovementComponent)
	{
		MovementComponent->OnAimChanged.AddDynamic(this, &UPangeaHunterSenseComponent::HandleAimChanged);
		bIsAiming = MovementComponent->GetIsAiming();
	}

	OwnerCharacter->OnCrouchStateChanged.AddDynamic(this, &UPangeaHunterSenseComponent::HandleCrouchChanged);
	bIsCrouched = OwnerCharacter->bIsCrouched;
	PrintDebugMessage(FString::Printf(TEXT("Resolved owner state. Aim=%s Crouch=%s Local=%s"),
		bIsAiming ? TEXT("true") : TEXT("false"),
		bIsCrouched ? TEXT("true") : TEXT("false"),
		IsLocallyControlledHunter() ? TEXT("true") : TEXT("false")), FColor::Silver, true);
}

void UPangeaHunterSenseComponent::RefreshSenseState()
{
	const bool bWasActive = bHuntingSenseActive;
	bHuntingSenseActive = bRequireAimAndCrouch ? (bIsAiming && bIsCrouched) : (bIsAiming || bIsCrouched);
	ScanAccumulator = ScanInterval;

	if (bWasActive != bHuntingSenseActive)
	{
		PrintDebugMessage(
			FString::Printf(TEXT("Hunt mode %s (Aim=%s Crouch=%s)"),
				bHuntingSenseActive ? TEXT("ENTER") : TEXT("EXIT"),
				bIsAiming ? TEXT("true") : TEXT("false"),
				bIsCrouched ? TEXT("true") : TEXT("false")),
			bHuntingSenseActive ? FColor::Green : FColor::Red,
			true);
	}

	if (!bHuntingSenseActive)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<APangeaHuntClueActor> ClueIterator(World); ClueIterator; ++ClueIterator)
			{
				ClueIterator->SetRevealState(false, 0.f);
			}
		}
		ResetVisualAssignments();
		ReleaseUnusedVisuals();
		ResetFootprintFocus();
	}
}

void UPangeaHunterSenseComponent::DrawNearbyTracks()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FVector HunterLocation = Owner->GetActorLocation();
	const float LocalTimeSeconds = World->GetTimeSeconds();
	int32 VisibleTrackCount = 0;

	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		UPangeaTrackEmitterComponent* TrackEmitter = ActorIterator->FindComponentByClass<UPangeaTrackEmitterComponent>();
		if (!TrackEmitter)
		{
			continue;
		}

		const UHuntSpeciesConfig* HuntConfig = TrackEmitter->GetHuntSpeciesConfig();
		const float RevealRadius = GetModifiedRevealRadius(TrackEmitter->GetRevealRadius());
		const float RevealRadiusSq = FMath::Square(RevealRadius);
		for (const FHuntTrackPoint& TrackPoint : TrackEmitter->GetTrackPoints())
		{
			const float DistanceSq = FVector::DistSquared(HunterLocation, TrackPoint.Location);
			if (DistanceSq > RevealRadiusSq)
			{
				continue;
			}

			const float AgeSeconds = FMath::Max(0.f, LocalTimeSeconds - TrackPoint.CreatedServerTime);
			if (TrackPoint.Lifetime > 0.f && AgeSeconds > TrackPoint.Lifetime)
			{
				continue;
			}

			float FreshnessAlpha = TrackPoint.Lifetime > 0.f ? 1.f - (AgeSeconds / TrackPoint.Lifetime) : 1.f;
			FreshnessAlpha = FMath::Clamp(FreshnessAlpha, 0.f, 1.f);
			if (HuntConfig && HuntConfig->FreshnessVisibilityCurve)
			{
				FreshnessAlpha = HuntConfig->FreshnessVisibilityCurve->GetFloatValue(FreshnessAlpha);
			}

			const FLinearColor FreshColor = FLinearColor::LerpUsingHSV(FLinearColor(0.15f, 0.35f, 1.f), FLinearColor(0.1f, 1.f, 0.35f), FreshnessAlpha);
			const FColor DebugColor = FreshColor.ToFColor(true);
			const FVector ArrowEnd = TrackPoint.Location + TrackPoint.Rotation.Vector() * 70.f;
			++VisibleTrackCount;

			if (bDebugDrawTracks)
			{
				DrawDebugSphere(World, TrackPoint.Location, DebugSphereRadius, 10, DebugColor, false, DebugDrawDuration, 0, 1.25f);
				DrawDebugDirectionalArrow(World, TrackPoint.Location, ArrowEnd, 24.f, DebugColor, false, DebugDrawDuration, 0, 1.75f);
			}
		}
	}

	PrintDebugMessage(FString::Printf(TEXT("Reveal scan found %d visible tracks"), VisibleTrackCount), FColor::Emerald);
}

void UPangeaHunterSenseComponent::UpdateTrackVisuals()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	ResetVisualAssignments();
	NearestVisibleFootprintSourceActor.Reset();
	float NearestVisibleFootprintDistanceSq = TNumericLimits<float>::Max();

	const FVector HunterLocation = Owner->GetActorLocation();
	const float LocalTimeSeconds = World->GetTimeSeconds();

	if (bUsePlaceholderMeshVisuals)
	{
		for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
		{
			AActor* SourceActor = *ActorIterator;
			UPangeaTrackEmitterComponent* TrackEmitter = SourceActor ? SourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() : nullptr;
			if (!TrackEmitter)
			{
				continue;
			}

			const UHuntSpeciesConfig* HuntConfig = TrackEmitter->GetHuntSpeciesConfig();
			const float RevealRadius = GetModifiedRevealRadius(TrackEmitter->GetRevealRadius());
			const float RevealRadiusSq = FMath::Square(RevealRadius);
			const float VisualScale = HuntConfig && HuntConfig->FootprintVisualScale > 0.f ? HuntConfig->FootprintVisualScale : 0.35f;
			UStaticMesh* VisualMesh = HuntConfig && HuntConfig->FootprintStaticMesh ? HuntConfig->FootprintStaticMesh.Get() : ResolveFallbackMesh();
			UMaterialInterface* VisualMaterial = HuntConfig && HuntConfig->FootprintStaticMeshMaterial ? HuntConfig->FootprintStaticMeshMaterial.Get() : ResolveFallbackMaterial();
			if (!VisualMesh)
			{
				continue;
			}

			for (const FHuntTrackPoint& TrackPoint : TrackEmitter->GetTrackPoints())
			{
				const float DistanceSq = FVector::DistSquared(HunterLocation, TrackPoint.Location);
				if (DistanceSq > RevealRadiusSq)
				{
					continue;
				}

				const float AgeSeconds = FMath::Max(0.f, LocalTimeSeconds - TrackPoint.CreatedServerTime);
				if (TrackPoint.Lifetime > 0.f && AgeSeconds > TrackPoint.Lifetime)
				{
					continue;
				}

				if (TrackPoint.ClueType == EHuntClueType::Footprint && DistanceSq < NearestVisibleFootprintDistanceSq)
				{
					NearestVisibleFootprintDistanceSq = DistanceSq;
					NearestVisibleFootprintSourceActor = SourceActor;
				}

				float FreshnessAlpha = TrackPoint.Lifetime > 0.f ? 1.f - (AgeSeconds / TrackPoint.Lifetime) : 1.f;
				FreshnessAlpha = FMath::Clamp(FreshnessAlpha, 0.15f, 1.f);
				if (HuntConfig && HuntConfig->FreshnessVisibilityCurve)
				{
					FreshnessAlpha = HuntConfig->FreshnessVisibilityCurve->GetFloatValue(FreshnessAlpha);
				}

				float ProximityAlpha = 1.f;
				if (SourceActor)
				{
					const float DistanceToDino = FVector::Dist(SourceActor->GetActorLocation(), TrackPoint.Location);
					const float MaxDistance = FMath::Max(1.f, RevealRadius);
					const float NormalizedDistance = FMath::Clamp(DistanceToDino / MaxDistance, 0.f, 1.f);
					ProximityAlpha = FMath::Pow(1.f - NormalizedDistance, 0.65f);
				}

				const float FinalVisibility = FMath::Clamp(FreshnessAlpha * FMath::Lerp(0.35f, 1.0f, ProximityAlpha), 0.08f, 1.f);

				UStaticMeshComponent* VisualComponent = AcquireVisualSlot(BuildTrackKey(SourceActor, TrackPoint));
				if (!VisualComponent)
				{
					continue;
				}

				FPangeaHuntVisualSlot* VisualSlot = nullptr;
				for (FPangeaHuntVisualSlot& Slot : VisualPool)
				{
					if (Slot.VisualComponent == VisualComponent)
					{
						VisualSlot = &Slot;
						break;
					}
				}

				if (!VisualSlot)
				{
					continue;
				}

				VisualSlot->SourceActor = SourceActor;
				VisualSlot->ClueType = TrackPoint.ClueType;

				VisualComponent->SetStaticMesh(VisualMesh);
				if (VisualMaterial)
				{
					if (!VisualSlot->MaterialInstance || VisualSlot->MaterialInstance->Parent != VisualMaterial)
					{
						VisualSlot->MaterialInstance = UMaterialInstanceDynamic::Create(VisualMaterial, this);
					}

					if (VisualSlot->MaterialInstance)
					{
						const FLinearColor TrackTint = UKismetMathLibrary::LinearColorLerp(
							FLinearColor(0.14f, 0.95f, 0.45f, 1.f),
							FLinearColor(0.05f, 0.35f, 1.0f, 1.f),
							1.f - FinalVisibility);
						VisualSlot->MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), FinalVisibility);
						VisualSlot->MaterialInstance->SetScalarParameterValue(TEXT("EmissiveStrength"), FMath::Lerp(1.5f, 8.0f, FinalVisibility));
						VisualSlot->MaterialInstance->SetVectorParameterValue(TEXT("Tint"), TrackTint);
						VisualComponent->SetMaterial(0, VisualSlot->MaterialInstance);
					}
					else
					{
						VisualComponent->SetMaterial(0, VisualMaterial);
					}
				}

				VisualComponent->SetHiddenInGame(false);
				VisualComponent->SetWorldLocation(TrackPoint.Location);
				VisualComponent->SetWorldRotation(TrackPoint.Rotation);
				VisualComponent->SetWorldScale3D(FVector(VisualScale * TrackPoint.Strength, VisualScale * TrackPoint.Strength, 1.f));
			}
		}

		ReleaseUnusedVisuals();
	}

	for (TActorIterator<APangeaHuntClueActor> ClueIterator(World); ClueIterator; ++ClueIterator)
	{
		APangeaHuntClueActor* ClueActor = *ClueIterator;
		if (!ClueActor)
		{
			continue;
		}

		const float MaxRevealRadius = GetModifiedRevealRadius(2500.f);
		const float DistanceSq = FVector::DistSquared(HunterLocation, ClueActor->GetActorLocation());
		const float MaxRevealRadiusSq = FMath::Square(MaxRevealRadius);
		if (DistanceSq > MaxRevealRadiusSq)
		{
			ClueActor->SetRevealState(false, 0.f);
			continue;
		}

		const float AgeSeconds = FMath::Max(0.f, LocalTimeSeconds - ClueActor->GetCreatedServerTime());
		const float Lifetime = ClueActor->GetLifetime();
		if (Lifetime > 0.f && AgeSeconds > Lifetime)
		{
			ClueActor->SetRevealState(false, 0.f);
			continue;
		}

		const float AgeAlpha = Lifetime > 0.f ? FMath::Clamp(1.f - (AgeSeconds / Lifetime), 0.f, 1.f) : 1.f;
		const float DistanceAlpha = 1.f - FMath::Clamp(FMath::Sqrt(DistanceSq) / MaxRevealRadius, 0.f, 0.92f);
		const float FinalAlpha = FMath::Clamp(AgeAlpha * FMath::Lerp(0.35f, 1.f, DistanceAlpha), 0.f, 1.f);
		ClueActor->SetRevealState(FinalAlpha > 0.02f, FinalAlpha);
	}
}

void UPangeaHunterSenseComponent::UpdateFootprintFocus(const float DeltaTime)
{
	const UHuntSpeciesConfig* FocusConfig = FocusedFootprintSourceActor.IsValid()
		? (FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() ? FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>()->GetHuntSpeciesConfig() : nullptr)
		: nullptr;
	const bool bEnableFocusIdentification = FocusConfig ? FocusConfig->bEnableFootprintFocusIdentification : bEnableFootprintFocusIdentification;
	if (!bEnableFocusIdentification || !bUsePlaceholderMeshVisuals)
	{
		ResetFootprintFocus();
		return;
	}

	AActor* FocusTarget = FindFocusedFootprintSource();
	if (!FocusTarget)
	{
		ResetFootprintFocus();
		return;
	}

	if (FocusedFootprintSourceActor.Get() != FocusTarget)
	{
		FocusedFootprintSourceActor = FocusTarget;
		FocusConfig = FocusTarget->FindComponentByClass<UPangeaTrackEmitterComponent>() ? FocusTarget->FindComponentByClass<UPangeaTrackEmitterComponent>()->GetHuntSpeciesConfig() : nullptr;
		FootprintFocusProgress = 0.f;
		bFocusedFootprintAlreadyIdentified = false;
		OnFootprintFocusChanged.Broadcast(true, FootprintFocusProgress, FocusTarget);
		UpdateFootprintFocusWidget(true, FootprintFocusProgress);
	}

	const float FocusDuration = FocusConfig ? FocusConfig->FootprintFocusDuration : FootprintFocusDuration;
	FootprintFocusProgress = FMath::Clamp(FootprintFocusProgress + (DeltaTime / FMath::Max(0.05f, FocusDuration)), 0.f, 1.f);

	OnFootprintFocusChanged.Broadcast(true, FootprintFocusProgress, FocusTarget);
	UpdateFootprintFocusWidget(true, FootprintFocusProgress);

	const bool bAutoIdentify = FocusConfig ? FocusConfig->bAutoIdentifyFocusedFootprints : bAutoIdentifyFocusedFootprints;
	if (bAutoIdentify && !bFocusedFootprintAlreadyIdentified && FootprintFocusProgress >= 1.f)
	{
		bFocusedFootprintAlreadyIdentified = true;
		FootprintFocusProgress = 1.f;
		OnFootprintFocusChanged.Broadcast(true, FootprintFocusProgress, FocusTarget);
		UpdateFootprintFocusWidget(true, FootprintFocusProgress);

		if (IdentifyNearestVisibleFootprint())
		{
			ShowTracksIdentifiedMessage();
			PrintDebugMessage(FString::Printf(TEXT("Identified footprints for %s"), *GetNameSafe(FocusTarget)), FColor::Green, true);
		}
	}
}

AActor* UPangeaHunterSenseComponent::FindFocusedFootprintSource() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return nullptr;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return nullptr;
	}

	const FVector2D CrosshairPosition(ViewportX * 0.5f, ViewportY * 0.5f);
	AActor* BestSourceActor = nullptr;
	float BestScreenDistanceSq = FMath::Square(FootprintFocusScreenRadius);

	for (const FPangeaHuntVisualSlot& Slot : VisualPool)
	{
		if (Slot.ClueType != EHuntClueType::Footprint || !Slot.SourceActor.IsValid() || !Slot.VisualComponent)
		{
			continue;
		}

		const UPangeaTrackEmitterComponent* TrackEmitter = Slot.SourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>();
		if (TrackEmitter && TrackEmitter->IsClueTypeIdentified(EHuntClueType::Footprint))
		{
			continue;
		}

		const UHuntSpeciesConfig* HuntConfig = TrackEmitter ? TrackEmitter->GetHuntSpeciesConfig() : nullptr;
		const float FocusRadius = HuntConfig ? HuntConfig->FootprintFocusScreenRadius : FootprintFocusScreenRadius;
		const float FocusRadiusSq = FMath::Square(FocusRadius);

		if (Slot.VisualComponent->bHiddenInGame)
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Slot.VisualComponent->GetComponentLocation(), ScreenPosition, true))
		{
			continue;
		}

		const float ScreenDistanceSq = FVector2D::DistSquared(ScreenPosition, CrosshairPosition);
		if (ScreenDistanceSq <= FocusRadiusSq && ScreenDistanceSq <= BestScreenDistanceSq)
		{
			BestScreenDistanceSq = ScreenDistanceSq;
			BestSourceActor = Slot.SourceActor.Get();
		}
	}

	return BestSourceActor;
}

void UPangeaHunterSenseComponent::ResetFootprintFocus()
{
	const bool bHadTargetOrProgress = FocusedFootprintSourceActor.IsValid() || FootprintFocusProgress > 0.f;
	FocusedFootprintSourceActor.Reset();
	FootprintFocusProgress = 0.f;
	bFocusedFootprintAlreadyIdentified = false;

	if (bHadTargetOrProgress)
	{
		OnFootprintFocusChanged.Broadcast(false, 0.f, nullptr);
		UpdateFootprintFocusWidget(false, 0.f);
	}
}

void UPangeaHunterSenseComponent::EnsureFootprintFocusWidget()
{
	const UHuntSpeciesConfig* FocusConfig = FocusedFootprintSourceActor.IsValid()
		? (FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() ? FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>()->GetHuntSpeciesConfig() : nullptr)
		: nullptr;
	const bool bShouldShowFocusWidget = FocusConfig ? FocusConfig->bShowFootprintFocusWidget : bShowFootprintFocusWidget;
	if (!bShouldShowFocusWidget || FootprintFocusWidget || !IsLocallyControlledHunter())
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return;
	}

	TSubclassOf<UPangeaFootprintFocusWidget> WidgetClass = FootprintFocusWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UPangeaFootprintFocusWidget::StaticClass();
	}

	FootprintFocusWidget = CreateWidget<UPangeaFootprintFocusWidget>(PlayerController, WidgetClass);
	if (FootprintFocusWidget)
	{
		int32 ViewportX = 0;
		int32 ViewportY = 0;
		PlayerController->GetViewportSize(ViewportX, ViewportY);
		const FVector2D CrosshairPosition(ViewportX * 0.5f, ViewportY * 0.5f);
		const FVector2D WidgetOffset = FocusConfig ? FocusConfig->FootprintFocusWidgetScreenOffset : FootprintFocusWidgetScreenOffset;
		const FVector2D WidgetSize = FocusConfig ? FocusConfig->FootprintFocusWidgetSize : FootprintFocusWidgetSize;
		FootprintFocusWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		FootprintFocusWidget->SetPositionInViewport(CrosshairPosition + WidgetOffset, true);
		FootprintFocusWidget->SetDesiredSizeInViewport(FVector2D(FMath::Max(1.f, WidgetSize.X), FMath::Max(1.f, WidgetSize.Y)));
		FootprintFocusWidget->AddToViewport();
		FootprintFocusWidget->SetRingCenter(FVector2D::ZeroVector);
		FootprintFocusWidget->SetFocusState(false, 0.f);
	}
}

void UPangeaHunterSenseComponent::UpdateFootprintFocusWidget(const bool bHasTarget, const float Progress)
{
	EnsureFootprintFocusWidget();
	if (FootprintFocusWidget)
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				int32 ViewportX = 0;
				int32 ViewportY = 0;
				PlayerController->GetViewportSize(ViewportX, ViewportY);
				const FVector2D CrosshairPosition(ViewportX * 0.5f, ViewportY * 0.5f);
				const UHuntSpeciesConfig* FocusConfig = FocusedFootprintSourceActor.IsValid()
					? (FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() ? FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>()->GetHuntSpeciesConfig() : nullptr)
					: nullptr;
				const FVector2D WidgetOffset = FocusConfig ? FocusConfig->FootprintFocusWidgetScreenOffset : FootprintFocusWidgetScreenOffset;
				const FVector2D WidgetSize = FocusConfig ? FocusConfig->FootprintFocusWidgetSize : FootprintFocusWidgetSize;
				FootprintFocusWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
				FootprintFocusWidget->SetPositionInViewport(CrosshairPosition + WidgetOffset, true);
				FootprintFocusWidget->SetDesiredSizeInViewport(FVector2D(FMath::Max(1.f, WidgetSize.X), FMath::Max(1.f, WidgetSize.Y)));
				FootprintFocusWidget->SetRingCenter(FVector2D::ZeroVector);
			}
		}

		FootprintFocusWidget->SetFocusState(bHasTarget, Progress);
	}
}

void UPangeaHunterSenseComponent::ShowTracksIdentifiedMessage()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return;
	}

	const UHuntSpeciesConfig* FocusConfig = FocusedFootprintSourceActor.IsValid()
		? (FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() ? FocusedFootprintSourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>()->GetHuntSpeciesConfig() : nullptr)
		: nullptr;
	const TSoftClassPtr<UUserWidget>& NotificationWidgetClass = FocusConfig ? FocusConfig->ACFNotificationsListWidgetClass : ACFNotificationsListWidgetClass;
	const TSoftClassPtr<UUserWidget>& FallbackMessageClass = FocusConfig ? FocusConfig->FallbackOnScreenMessageWidgetClass : FallbackOnScreenMessageWidgetClass;
	const FText MessageText = FocusConfig ? FocusConfig->TracksIdentifiedMessage : TracksIdentifiedMessage;
	const float MessageDuration = FocusConfig ? FocusConfig->TracksIdentifiedMessageDuration : TracksIdentifiedMessageDuration;
	if (MessageText.IsEmpty())
	{
		return;
	}

	if (UClass* NotificationListClass = NotificationWidgetClass.LoadSynchronous())
	{
		TArray<UUserWidget*> NotificationWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, NotificationWidgets, NotificationListClass, false);
		for (UUserWidget* NotificationWidget : NotificationWidgets)
		{
			if (!NotificationWidget)
			{
				continue;
			}

			if (UFunction* AddNotificationFunction = NotificationWidget->FindFunction(TEXT("AddNotification")))
			{
				struct FAddNotificationParams
				{
					FText NotificationText;
					float Duration = 0.f;
				};

				FAddNotificationParams Params;
				Params.NotificationText = MessageText;
				Params.Duration = MessageDuration;
				NotificationWidget->ProcessEvent(AddNotificationFunction, &Params);
				return;
			}
		}
	}

	UClass* MessageWidgetClass = FallbackMessageClass.LoadSynchronous();
	if (!MessageWidgetClass)
	{
		return;
	}

	UUserWidget* MessageWidget = CreateWidget<UUserWidget>(PlayerController, MessageWidgetClass);
	if (!MessageWidget)
	{
		return;
	}

	if (FTextProperty* MessageProperty = FindFProperty<FTextProperty>(MessageWidget->GetClass(), TEXT("InMessage")))
	{
		MessageProperty->SetPropertyValue_InContainer(MessageWidget, MessageText);
	}

	MessageWidget->AddToViewport();
}

void UPangeaHunterSenseComponent::ResetVisualAssignments()
{
	for (FPangeaHuntVisualSlot& Slot : VisualPool)
	{
		Slot.bAssignedThisUpdate = false;
	}
}

UStaticMeshComponent* UPangeaHunterSenseComponent::AcquireVisualSlot(const FName& TrackKey)
{
	for (FPangeaHuntVisualSlot& Slot : VisualPool)
	{
		if (Slot.TrackKey == TrackKey && Slot.VisualComponent)
		{
			Slot.bAssignedThisUpdate = true;
			return Slot.VisualComponent;
		}
	}

	for (FPangeaHuntVisualSlot& Slot : VisualPool)
	{
		if (Slot.VisualComponent && !Slot.bAssignedThisUpdate)
		{
			Slot.TrackKey = TrackKey;
			Slot.SourceActor.Reset();
			Slot.ClueType = EHuntClueType::Custom;
			Slot.bAssignedThisUpdate = true;
			return Slot.VisualComponent;
		}
	}

	if (VisualPool.Num() >= MaxVisualPoolSize)
	{
		return nullptr;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UStaticMeshComponent* VisualComponent = NewObject<UStaticMeshComponent>(Owner);
	if (!VisualComponent)
	{
		return nullptr;
	}

	VisualComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualComponent->SetGenerateOverlapEvents(false);
	VisualComponent->SetCanEverAffectNavigation(false);
	VisualComponent->SetMobility(EComponentMobility::Movable);
	VisualComponent->SetCastShadow(false);
	VisualComponent->SetHiddenInGame(false);
	VisualComponent->SetReceivesDecals(false);
	VisualComponent->RegisterComponentWithWorld(GetWorld());

	FPangeaHuntVisualSlot& NewSlot = VisualPool.AddDefaulted_GetRef();
	NewSlot.VisualComponent = VisualComponent;
	NewSlot.MaterialInstance = nullptr;
	NewSlot.TrackKey = TrackKey;
	NewSlot.bAssignedThisUpdate = true;

	return VisualComponent;
}

void UPangeaHunterSenseComponent::ReleaseUnusedVisuals()
{
	for (FPangeaHuntVisualSlot& Slot : VisualPool)
	{
		if (Slot.VisualComponent && !Slot.bAssignedThisUpdate)
		{
			Slot.TrackKey = NAME_None;
			Slot.SourceActor.Reset();
			Slot.ClueType = EHuntClueType::Custom;
			Slot.VisualComponent->SetHiddenInGame(true);
		}
	}
}

FName UPangeaHunterSenseComponent::BuildTrackKey(const AActor* SourceActor, const FHuntTrackPoint& TrackPoint) const
{
	const FString Key = FString::Printf(
		TEXT("%s_%.0f_%.0f_%.0f_%.2f"),
		*GetNameSafe(SourceActor),
		TrackPoint.Location.X,
		TrackPoint.Location.Y,
		TrackPoint.Location.Z,
		TrackPoint.CreatedServerTime);
	return FName(*Key);
}

UStaticMesh* UPangeaHunterSenseComponent::ResolveFallbackMesh() const
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
}

UMaterialInterface* UPangeaHunterSenseComponent::ResolveFallbackMaterial() const
{
	if (UMaterialInterface* PluginMaterial = LoadObject<UMaterialInterface>(nullptr, PangeaHuntingDebug::FallbackTrackMaterialPath))
	{
		return PluginMaterial;
	}

	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

bool UPangeaHunterSenseComponent::IsLocallyControlledHunter() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

float UPangeaHunterSenseComponent::GetModifiedRevealRadius(const float BaseRevealRadius) const
{
	float BonusRadius = FlatRevealRadiusBonus;

	if (bUseStatModifiedRevealRadius && RevealRadiusStatTag.IsValid())
	{
		const AActor* Owner = GetOwner();
		const UACFGASStatisticsComponent* StatisticsComponent = Owner ? Owner->FindComponentByClass<UACFGASStatisticsComponent>() : nullptr;
		if (StatisticsComponent)
		{
			BonusRadius += StatisticsComponent->GetCurrentValueForStatitstic(RevealRadiusStatTag) * RevealRadiusPerStatPoint;
		}
	}

	BonusRadius = FMath::Clamp(BonusRadius, 0.f, MaxRevealRadiusBonus);
	return FMath::Max(1.f, BaseRevealRadius + BonusRadius);
}

void UPangeaHunterSenseComponent::ServerIdentifyFootprints_Implementation(AActor* SourceActor)
{
	AActor* Owner = GetOwner();
	UPangeaTrackEmitterComponent* TrackEmitter = SourceActor ? SourceActor->FindComponentByClass<UPangeaTrackEmitterComponent>() : nullptr;
	if (!Owner || !TrackEmitter)
	{
		return;
	}

	const float RevealRadius = GetModifiedRevealRadius(TrackEmitter->GetRevealRadius());
	const float RevealRadiusSq = FMath::Square(RevealRadius);
	for (const FHuntTrackPoint& TrackPoint : TrackEmitter->GetTrackPoints())
	{
		if (TrackEmitter->IsClueTypeIdentified(EHuntClueType::Footprint))
		{
			return;
		}

		if (TrackPoint.ClueType != EHuntClueType::Footprint)
		{
			continue;
		}

		if (FVector::DistSquared(Owner->GetActorLocation(), TrackPoint.Location) <= RevealRadiusSq)
		{
			TrackEmitter->MarkClueTypeIdentified(EHuntClueType::Footprint, Owner);
			return;
		}
	}
}

void UPangeaHunterSenseComponent::PrintDebugMessage(const FString& Message, const FColor& Color, const bool bLogAlso) const
{
	if (!bEnableDebugMessages)
	{
		return;
	}

	if (GEngine)
	{
		const uint64 Key = static_cast<uint64>(PangeaHuntingDebug::HunterDebugMessageKey++);
		GEngine->AddOnScreenDebugMessage(Key, DebugMessageDuration, Color, FString::Printf(TEXT("[Hunting] %s"), *Message));
	}

	if (bLogAlso)
	{
		UE_LOG(LogTemp, Log, TEXT("[Hunting] %s"), *Message);
	}
}
