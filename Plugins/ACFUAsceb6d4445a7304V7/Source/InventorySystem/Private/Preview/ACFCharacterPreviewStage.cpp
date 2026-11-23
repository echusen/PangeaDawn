// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Preview/ACFCharacterPreviewStage.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
AACFCharacterPreviewStage::AACFCharacterPreviewStage()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Root
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	// Sphere mesh (your preview room)
	PreviewSphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewSphereMesh"));
	PreviewSphereMesh->SetupAttachment(RootSceneComponent);
	PreviewSphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn point for preview character
	PreviewSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewSpawnPoint"));
	PreviewSpawnPoint->SetupAttachment(RootSceneComponent);
	PreviewSpawnPoint->SetRelativeLocation(FVector::ZeroVector);

	// Sphere component to define post process volume boundaries
	PostProcessVolume = CreateDefaultSubobject<USphereComponent>(TEXT("PostProcessVolume"));
	PostProcessVolume->SetupAttachment(RootSceneComponent);
	PostProcessVolume->SetSphereRadius(500.0f); // Default radius, will be updated based on sphere
	PostProcessVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PostProcessVolume->SetHiddenInGame(false); // Keep visible for debugging, set to true in production

	// Post process component for exposure control
	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcessComponent->SetupAttachment(PostProcessVolume);
	PostProcessComponent->bUnbound = false;
	PostProcessComponent->BlendRadius = 100.0f;
	PostProcessComponent->BlendWeight = 1.0f;
	PostProcessComponent->Priority = 1;
}

void AACFCharacterPreviewStage::PositioningCharactersOnPreviewSphere(TArray<ACharacter*>& Characters,const float Radius,const float PreviewSpacingMultiplier)
{
	GeneratePoints(Characters.Num(), Radius,PreviewSpacingMultiplier);

	// Clear previous spot lights
	for (USpotLightComponent* Light : CharacterSpotLights)
	{
		if (IsValid(Light))
		{
			Light->DestroyComponent();
		}
	}
	CharacterSpotLights.Empty();

	// Configure post process volume size based on sphere radius
	if (PostProcessVolume && PostProcessComponent)
	{

		// Apply post process only if enabled
		if (bEnablePostProcess)
		{
			PostProcessComponent->Settings.bOverride_AutoExposureMethod = true;
			PostProcessComponent->Settings.AutoExposureMethod = ExposureMethod;
			PostProcessComponent->Settings.bOverride_AutoExposureBias = true;
			PostProcessComponent->Settings.AutoExposureBias = ExposureCompensation;
			PostProcessComponent->Settings.bOverride_AutoExposureMinBrightness = true;
			PostProcessComponent->Settings.AutoExposureMinBrightness = MinBrightness;
			PostProcessComponent->Settings.bOverride_AutoExposureMaxBrightness = true;
			PostProcessComponent->Settings.AutoExposureMaxBrightness = MaxBrightness;
			PostProcessComponent->SetActive(true);
		}
		else
		{
			PostProcessComponent->SetActive(false);
		}
	}

	for (int32 i = 0; i < Characters.Num(); i++)
	{
		PreviewSpawnPoints[i]->AttachToComponent(PreviewSphereMesh, FAttachmentTransformRules::KeepRelativeTransform);

		ACharacter* Character = Characters[i];
		if (!IsValid(Character))
		{
			continue;
		}

		const FTransform SpawnTransform = PreviewSpawnPoints[i]->GetComponentTransform();
		Character->SetActorLocation(SpawnTransform.GetLocation());
		Character->SetActorRotation(SpawnTransform.GetRotation().Rotator());
		Character->GetCharacterMovement()->GravityScale = 0.f;
		CharacterPreviewTransforms.Add(Character,SpawnTransform);

		// Create and attach spot light to spawn point (NOT to character, they exist also in combat)
		if (bEnableCharacterLights)
		{
			USpotLightComponent* CharacterLight = NewObject<USpotLightComponent>(this, USpotLightComponent::StaticClass());
			if (CharacterLight)
			{
				CharacterLight->RegisterComponent();
				CharacterLight->AttachToComponent(PreviewSpawnPoints[i], FAttachmentTransformRules::KeepRelativeTransform);
				CharacterLight->SetRelativeLocation(CharacterLightOffset);
				CharacterLight->SetRelativeRotation(CharacterLightRotation);
				CharacterLight->SetIntensity(CharacterLightIntensity);
				CharacterLight->SetSourceRadius(CharacterLightSourceRadius);
				CharacterLight->SetCastShadows(false);

				CharacterSpotLights.Add(CharacterLight);
			}
		}
	}
}

void AACFCharacterPreviewStage::MoveCharactersOnPreviewSphere(TArray<ACharacter*>& Characters)
{
	// Update post process settings if needed
	UpdatePostProcessSettings();

	for (ACharacter* Character : Characters)
	{
		if (CharacterPreviewTransforms.Contains(Character))
		{
			const FTransform SpawnTransform = CharacterPreviewTransforms[Character];
			Character->SetActorLocation(SpawnTransform.GetLocation());
			Character->SetActorRotation(SpawnTransform.GetRotation().Rotator());
			Character->GetCharacterMovement()->GravityScale = 0.f;
		}
	}
}


void AACFCharacterPreviewStage::GeneratePoints(const int32 NumPoints,const float Radius,const float PreviewSpacingMultiplier)
{
	PreviewSpawnPoints.Empty();

	if (NumPoints <= 0)
	{
		return;
	}
	const float HalfWidth = Radius * PreviewSpacingMultiplier;

	const float StepY = (NumPoints > 1)
		? (2.f * HalfWidth) / static_cast<float>(NumPoints - 1)
		: 0.f;

	for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
	{
		FVector Position;

		Position.X = Radius;

		Position.Y = -HalfWidth + StepY * static_cast<float>(PointIndex);
		Position.Z = 0.f;

		USceneComponent* SpawnPoint = NewObject<USceneComponent>(this);
		SpawnPoint->RegisterComponent();
		SpawnPoint->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		SpawnPoint->SetRelativeLocation(Position);

		PreviewSpawnPoints.Add(SpawnPoint);
	}
}

void AACFCharacterPreviewStage::UpdatePostProcessSettings()
{
	if (PostProcessComponent)
	{
		if (bEnablePostProcess)
		{
			PostProcessComponent->Settings.bOverride_AutoExposureMethod = true;
			PostProcessComponent->Settings.AutoExposureMethod = ExposureMethod;
			PostProcessComponent->Settings.bOverride_AutoExposureBias = true;
			PostProcessComponent->Settings.AutoExposureBias = ExposureCompensation;
			PostProcessComponent->Settings.bOverride_AutoExposureMinBrightness = true;
			PostProcessComponent->Settings.AutoExposureMinBrightness = MinBrightness;
			PostProcessComponent->Settings.bOverride_AutoExposureMaxBrightness = true;
			PostProcessComponent->Settings.AutoExposureMaxBrightness = MaxBrightness;
			PostProcessComponent->SetActive(true);
		}
		else
		{
			PostProcessComponent->SetActive(false);
		}
		PostProcessComponent->MarkRenderStateDirty();
	}
}

void AACFCharacterPreviewStage::UpdateCharacterLightsSettings()
{
	for (USpotLightComponent* Light : CharacterSpotLights)
	{
		if (IsValid(Light))
		{
			Light->SetIntensity(CharacterLightIntensity);
			Light->SetSourceRadius(CharacterLightSourceRadius);
			Light->SetRelativeLocation(CharacterLightOffset);
			Light->SetRelativeRotation(CharacterLightRotation);
			Light->MarkRenderStateDirty();
		}
	}
}

void AACFCharacterPreviewStage::SetCharacterLightsEnabled(bool bEnabled)
{
	bEnableCharacterLights = bEnabled;

	for (USpotLightComponent* Light : CharacterSpotLights)
	{
		if (IsValid(Light))
		{
			Light->SetVisibility(bEnabled);
		}
	}
}

