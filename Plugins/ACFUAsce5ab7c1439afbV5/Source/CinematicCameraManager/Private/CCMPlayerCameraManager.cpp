// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "CCMPlayerCameraManager.h"
#include "CCMCameraFunctionLibrary.h"
#include "CCMCameraSplineComponent.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h" 
#include <Camera/CameraActor.h>
#include <Camera/CameraComponent.h>
#include <CineCameraComponent.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <GameFramework/Character.h>
#include <GameFramework/CharacterMovementComponent.h>
#include <GameFramework/SpringArmComponent.h>
#include <Kismet/GameplayStatics.h>
#include <Kismet/KismetMathLibrary.h>
#include <TimerManager.h>
#include <Net/UnrealNetwork.h>

#include "ACFCameraPointComponent.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "DefaultLevelSequenceInstanceData.h"

class UACFBattleManagerComponent;

ACCMPlayerCameraManager::ACCMPlayerCameraManager()
{
	PrimaryActorTick.bStartWithTickEnabled = true;
	CameraActorClass = ACameraActor::StaticClass();
}



void ACCMPlayerCameraManager::BeginPlay()
{
	Super::BeginPlay();
	if (GetLocalRole() == ROLE_Authority) {
		BindEvents();
	}

	if (!UKismetSystemLibrary::IsDedicatedServer(this)) {

		UpdateCameraReferences();
		FinalMov.FovInterpSpeed = 0.f;
		FinalMov.InterpSpeed = 0.f;
	}
}


void ACCMPlayerCameraManager::BlendToPoint(AActor* OwnerWithPoints, FGameplayTag CameraTag, float BlendTime,
	TEnumAsByte<EViewTargetBlendFunction> BlendFunc, float BlendExp, bool bLockOutgoing)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !IsValid(OwnerWithPoints))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CCM] Invalid PC or TargetActor"));
		ensureAlwaysMsgf(false, TEXT("[CCM] Invalid PC or TargetActor"));
		return;
	}

	AActor* ViewTargetActor = nullptr;
	UACFCameraPointComponent* CameraComp = ResolveCameraPoint(OwnerWithPoints, CameraTag, ViewTargetActor);

	if (!CameraComp || !ViewTargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CCM] Camera Component not found for tag: %s"), *CameraTag.ToString());
		return;
	}

	if (CameraComp == currentCameraComp.Get())
	{
		return;
	}

	if (currentCameraComp)
	{
		currentCameraComp->OnCameraDeactivated();
	}

	currentCameraComp = CameraComp;

	// Deactivate other cameras on the view target actor
	TArray<UCameraComponent*> AllCams;
	ViewTargetActor->GetComponents<UCameraComponent>(AllCams);
	for (UCameraComponent* Cam : AllCams)
	{
		if (Cam != CameraComp)
		{
			Cam->Deactivate();
		}
	}

	CameraComp->Activate(true);
	CameraComp->OnCameraActivated();

	PlayerController->SetViewTargetWithBlend(ViewTargetActor, BlendTime, BlendFunc, BlendExp, bLockOutgoing);
}


UACFCameraPointComponent* ACCMPlayerCameraManager::ResolveCameraPoint(AActor* OwnerWithPoints, const FGameplayTag& CameraTag, AActor*& OutViewTarget) const
{
	OutViewTarget = nullptr;

	// Check direct components on the owner
	TArray<UACFCameraPointComponent*> CameraPoints;
	OwnerWithPoints->GetComponents<UACFCameraPointComponent>(CameraPoints);

	for (UACFCameraPointComponent* CameraPoint : CameraPoints)
	{
		if (IsValid(CameraPoint) && CameraPoint->GetCameraTag() == CameraTag)
		{
			OutViewTarget = OwnerWithPoints;
			return CameraPoint;
		}
	}

	// Check child actors
	TArray<UChildActorComponent*> ChildActorComps;
	OwnerWithPoints->GetComponents<UChildActorComponent>(ChildActorComps);

	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		AActor* ChildActor = ChildComp->GetChildActor();
		if (!ChildActor)
		{
			continue;
		}

		UACFCameraPointComponent* CameraPoint = ChildActor->FindComponentByClass<UACFCameraPointComponent>();
		if (CameraPoint && CameraPoint->GetCameraTag() == CameraTag)
		{
			OutViewTarget = ChildActor;
			return CameraPoint;
		}
	}

	return nullptr;
}

void ACCMPlayerCameraManager::BindEvents()
{
	APlayerController* controller = GetOwningPlayerController();
	if (controller) {
		auto& eventNewPawn = controller->GetOnNewPawnNotifier();
		eventNewPawn.AddLambda([this](APawn* newPawn) {
			HandlePawnChanged(newPawn);
			});
	}
}

void ACCMPlayerCameraManager::UpdateCamera(float deltaTime)
{
	Super::UpdateCamera(deltaTime);
	if (!UKismetSystemLibrary::IsDedicatedServer(this)) {
		if (GetOwningPlayerController() && playerCharacter != GetOwningPlayerController()->GetPawn()) {
			UpdateCameraReferences();
		}
		if (playerCamera && cameraBoom) {
			MoveCamera(deltaTime);
			UpdateActorFraming(deltaTime);
			UpdateLockOnTarget(deltaTime);

			if (bIsPlayingSequence) {
				UpdateCameraSequence(deltaTime);
			}
		}

		FollowPlayer(deltaTime);
	}
}

void ACCMPlayerCameraManager::UpdateLockOnTarget(float deltaTime)
{
	if (targetLockType != ETargetLockType::ENone && playerCharacter && playerCamera) {
		FVector localPos = FVector();
		if (targetLockType == ETargetLockType::EActor && localtarget) {
			localPos = localtarget->GetActorLocation();
		}
		else if (localtargetComponent) {
			localPos = localtargetComponent->GetComponentLocation();
		}

		LockCameraOnPosition(localPos, deltaTime);
	}
}

void ACCMPlayerCameraManager::StartFramingActors(AActor* ActorToFrame, const FCCMActorFramingSettings& Settings, float FramingStrength)
{
	if (!ActorToFrame || !cameraBoom) {
		return;
	}

	// Only take a new base length when no framing session is running, otherwise reframing
	// while widened would make the current (widened) length the new resting length.
	if (!bIsFramingActors && !bIsRestoringFraming) {
		framingBaseArmLength = cameraBoom->TargetArmLength;
	}

	framedActor = ActorToFrame;
	framingSettings = Settings;
	_lockStrength = FramingStrength;
	bIsFramingActors = true;
	bIsRestoringFraming = false;
	// The two behaviours would be writing the same control rotation, so the framing takes the camera
	// over instead of sharing it.
	targetLockType = ETargetLockType::ENone;
}

void ACCMPlayerCameraManager::StopFramingActors()
{
	if (!bIsFramingActors) {
		return;
	}

	framedActor = nullptr;
	bIsFramingActors = false;
	bIsRestoringFraming = true;
}

void ACCMPlayerCameraManager::UpdateActorFraming(float deltaTime)
{
	if (!cameraBoom) {
		return;
	}

	if (bIsFramingActors) {
		if (framedActor && playerCharacter && playerCamera) {
			FrameActorPair(deltaTime);
			ApplyFramingSocketOffset();
			return;
		}

		// The framed actor was destroyed without anyone stopping the mode, so give the arm back
		// instead of leaving the camera stuck wide.
		framedActor = nullptr;
		bIsFramingActors = false;
		bIsRestoringFraming = true;
	}

	if (bIsRestoringFraming) {
		cameraBoom->TargetArmLength = FMath::FInterpTo(cameraBoom->TargetArmLength, framingBaseArmLength, deltaTime, framingSettings.ArmInterpSpeed);
		framingSocketOffset = FMath::VInterpTo(framingSocketOffset, FVector::ZeroVector, deltaTime, framingSettings.ArmInterpSpeed);
		if (FMath::IsNearlyEqual(cameraBoom->TargetArmLength, framingBaseArmLength, 0.5f) && framingSocketOffset.IsNearlyZero(0.5f)) {
			cameraBoom->TargetArmLength = framingBaseArmLength;
			framingSocketOffset = FVector::ZeroVector;
			bIsRestoringFraming = false;
		}
		ApplyFramingSocketOffset();
	}
}

void ACCMPlayerCameraManager::ApplyFramingSocketOffset()
{
	if (cameraBoom) {
		// The arm's offset is the two shares added up, never one of them alone: the camera events own
		// eventSocketOffset and the framing owns framingSocketOffset.
		cameraBoom->SocketOffset = eventSocketOffset + framingSocketOffset;
	}
}

bool ACCMPlayerCameraManager::GetFramingComposition(FRotator& OutRotation, float& OutArmLength, FVector& OutLateralOffset) const
{
	const AController* controller = playerCharacter ? playerCharacter->GetController() : nullptr;
	OutLateralOffset = FVector::ZeroVector;
	if (!controller || !playerCamera || !cameraBoom || !framedActor) {
		OutRotation = FRotator::ZeroRotator;
		OutArmLength = cameraBoom ? cameraBoom->TargetArmLength : 0.f;
		return false;
	}

	OutRotation = controller->GetControlRotation();
	OutArmLength = cameraBoom->TargetArmLength;

	const FVector playerLocation = playerCharacter->GetActorLocation();
	const FVector otherLocation = framedActor->GetActorLocation();
	const FVector midPoint = (playerLocation + otherLocation) * 0.5f;
	const FVector cameraLocation = playerCamera->GetComponentLocation();

	// Half of the screen the pair is allowed to use, as an angle. Vertically it is the same fraction
	// of the vertical field of view, which is the narrower of the two.
	const float screenFraction = FMath::Clamp(framingSettings.ScreenFraction, 0.1f, 1.f);
	const float halfFovRad = FMath::DegreesToRadians(playerCamera->FieldOfView * 0.5f);
	const float yawBound = FMath::RadiansToDegrees(halfFovRad) * screenFraction;
	const float aspectRatio = playerCamera->AspectRatio > 0.f ? playerCamera->AspectRatio : 1.777f;
	const float pitchBound = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(halfFovRad) / aspectRatio)) * screenFraction;

	const FRotator viewRotation = playerCamera->GetComponentRotation();
	const FVector boomDirection = viewRotation.Vector();
	const FVector pairSpan = otherLocation - playerLocation;
	const float otherAlongBoom = FVector::DotProduct(pairSpan, boomDirection);
	const float otherCamDepth = FVector::DotProduct(otherLocation - cameraLocation, boomDirection);

	// A target on the boom (behind the player, toward or past the lens) has almost no lateral span,
	// so the fit-in-FOV arm below would never grow. Dolly back until the camera sits behind both
	// with the same rest length it normally keeps behind the player. Do this before asking for yaw:
	// FindLookAtRotation on a point behind the camera is ~180 degrees and would spin the view.
	const float widestArm = FMath::Max(framingSettings.MaxArmLength, framingBaseArmLength);
	float desiredArm = framingBaseArmLength;
	if (otherAlongBoom < 0.f) {
		desiredArm = framingBaseArmLength - otherAlongBoom;
	}

	// Angle correction is only valid while the other actor is in front of the lens. Skip it while
	// they are at or behind the camera: the dolly above is what brings them into frame.
	const bool bOtherInFrontOfCamera = otherCamDepth > MinLockTargetDistance;
	FVector2D otherAngles = FVector2D::ZeroVector;
	if (bOtherInFrontOfCamera) {
		// Measured against where the camera is looking right now, so the correction below is exactly
		// what is missing to bring whoever is drifting out back inside that fraction of the screen.
		const FVector2D playerAngles = UCCMCameraFunctionLibrary::GetViewRelativeAngles(cameraLocation, viewRotation, playerLocation);
		otherAngles = UCCMCameraFunctionLibrary::GetViewRelativeAngles(cameraLocation, viewRotation, otherLocation);

		OutRotation.Yaw += UCCMCameraFunctionLibrary::GetFramingCorrection(playerAngles.X, otherAngles.X, yawBound);
		if (framingSettings.bFramePitch) {
			const float pitchCorrection = UCCMCameraFunctionLibrary::GetFramingCorrection(playerAngles.Y, otherAngles.Y, pitchBound);
			OutRotation.Pitch = FMath::Clamp(OutRotation.Pitch + pitchCorrection, -CameraLockPitchLowerLimit, CameraLockPitchUpperLimit);
		}
	}

	// How wide the pair reads on screen is its span across the view, not the raw distance between
	// them: an enemy straight ahead needs no room at all, one off to the side needs all of it.
	const FVector viewDirection = OutRotation.Vector();
	const float spanAcrossView = (pairSpan - FVector::DotProduct(pairSpan, viewDirection) * viewDirection).Size();

	// Distance the camera needs from the middle of the pair for that span to fit inside the same
	// fraction of the screen the rotation works with.
	const float neededDistance = (spanAcrossView * 0.5f) / FMath::Max(FMath::Tan(FMath::DegreesToRadians(yawBound)), UE_KINDA_SMALL_NUMBER);

	// The arm is measured from the player, while the distance above is from the middle of the pair,
	// which sits some way ahead along the view direction.
	const float midPointDepth = FVector::DotProduct(midPoint - playerLocation, viewDirection);
	// Never narrower than the camera's own arm length: this mode widens, it does not reframe the
	// whole game camera.
	desiredArm = FMath::Max(desiredArm, neededDistance - midPointDepth);
	OutArmLength = FMath::Clamp(desiredArm, framingBaseArmLength, widestArm);

	// Turning alone can never move the player off the center of the screen: the arm points at the
	// player, so the camera just orbits and only the other actor travels across the frame. Sliding
	// the arm sideways is what gives them a side each. Scaled by how far off center the other actor
	// is, so it fades out to nothing when it is straight ahead instead of snapping over as it
	// crosses in front. Held while the target is behind the lens: those angles are ~180 degrees.
	if (bOtherInFrontOfCamera && framingSettings.MaxLateralOffset > 0.f) {
		const float sideAlpha = FMath::Clamp(FMath::Abs(otherAngles.X) / yawBound, 0.f, 1.f);
		// Half the allowed angle leaves the player around the near third of the screen and the other
		// actor the far side of it.
		const float playerShift = OutArmLength * FMath::Tan(FMath::DegreesToRadians(yawBound * 0.5f)) * sideAlpha;
		// Sliding toward the other actor's side is what pushes the player off center on the opposite
		// one.
		OutLateralOffset.Y = FMath::Clamp(FMath::Sign(otherAngles.X) * playerShift, -framingSettings.MaxLateralOffset, framingSettings.MaxLateralOffset);
	}

	return true;
}

void ACCMPlayerCameraManager::FrameActorPair_Implementation(float deltaTime)
{
	FRotator desiredRotation;
	float desiredArmLength;
	FVector desiredLateralOffset;
	if (!GetFramingComposition(desiredRotation, desiredArmLength, desiredLateralOffset)) {
		return;
	}

	// Same magnetism as the lock, so the camera works just as hard at keeping them on screen; the
	// difference is that it is chasing the edge of the frame instead of the target itself, and once
	// they are inside there is nothing left to chase.
	AController* controller = playerCharacter->GetController();
	if (controller && _lockStrength > 0.f) {
		const FRotator oldRotation = controller->GetControlRotation();
		FRotator newRotation = FMath::RInterpTo(oldRotation, desiredRotation, deltaTime, _lockStrength);

		if (MaxLockRotationRate > 0.f) {
			const float maxStep = MaxLockRotationRate * deltaTime;
			const FRotator delta = (newRotation - oldRotation).GetNormalized();
			newRotation = oldRotation + FRotator(
				FMath::Clamp(delta.Pitch, -maxStep, maxStep),
				FMath::Clamp(delta.Yaw, -maxStep, maxStep),
				0.f);
		}

		controller->SetControlRotation(newRotation);
	}

	cameraBoom->TargetArmLength = FMath::FInterpTo(cameraBoom->TargetArmLength, desiredArmLength, deltaTime, framingSettings.ArmInterpSpeed);
	// Only the framing's own share of the socket offset: MoveCamera owns the camera events' share and
	// the two are added together, so neither can overwrite the other.
	framingSocketOffset = FMath::VInterpTo(framingSocketOffset, desiredLateralOffset, deltaTime, framingSettings.ArmInterpSpeed);
}

void ACCMPlayerCameraManager::UpdateCameraSequence(float deltaTime)
{
	if (currentSequence.cameraSequenceComp && currentSequence.currentTime < currentSequence.cameraSequenceComp->GetSplineLength() && currentSequenceSettings.lookAtActor) {

		UCameraComponent* cameraComp = sequenceCameraActor->GetCameraComponent();
		if (!cameraComp) {
			return;
		}
		const float interpFov = FMath::FInterpTo(cameraComp->FieldOfView, currentSequenceSettings.CameraFov, deltaTime, currentSequenceSettings.FovInterpSpeed);
		cameraComp->SetFieldOfView(interpFov);
		float currentSpeed = currentSequenceSettings.CameraSpeed;
		float const SplineLen = currentSequence.cameraSequenceComp->GetSplineLength();
		currentSequence.currentTime += currentSpeed * deltaTime * SplineLen;
		FVector const MountPos = currentSequence.cameraSequenceComp->GetLocationAtDistanceAlongSpline(currentSequence.currentTime, ESplineCoordinateSpace::World);
		sequenceCameraActor->SetActorLocation(MountPos);
		FRotator lookAtRot = UKismetMathLibrary::FindLookAtRotation(MountPos, currentSequenceSettings.lookAtActor->GetActorLocation());
		FRotator smoothedLookAt = FMath::RInterpTo(sequenceCameraActor->GetActorRotation(), lookAtRot, deltaTime, currentSequenceSettings.CameraRotationsSpeed);
		sequenceCameraActor->SetActorRotation(smoothedLookAt);
	}
	else {
		StopCurrentCameraSequence();
	}
}

void ACCMPlayerCameraManager::TriggerCameraSequence(AActor* cameraOwner, FName CameraEventName)
{
	if (cameraOwner && !bIsPlayingSequence) {

		TArray<UCCMCameraSplineComponent*> CameraSequences;
		cameraOwner->GetComponents<UCCMCameraSplineComponent>(CameraSequences, true);

		for (UCCMCameraSplineComponent* cameraSeq : CameraSequences) {
			if (cameraSeq->GetCameraSequenceName() == CameraEventName) {
				// StopLookingActor();
				currentSequence.Reset(cameraSeq);
				bIsPlayingSequence = true;
				break;
			}
		}

		if (currentSequence.cameraSequenceComp) {
			SetCameraSequenceData();

			if (UKismetSystemLibrary::IsServer(this)) {
				UGameplayStatics::SetGlobalTimeDilation(this, currentSequence.cameraSequenceComp->GetCameraSequenceSettings().TimeDilatation);
			}

			APlayerController* controller = UGameplayStatics::GetPlayerController(this, 0);
			if (controller) {
				controller->SetViewTargetWithBlend(sequenceCameraActor, currentSequence.cameraSequenceComp->GetCameraSequenceSettings().BlendSettingsTime);
			}
		}
	}
}

ALevelSequenceActor* ACCMPlayerCameraManager::PlayGameplaySequenceWithBinding(
	ULevelSequence* Sequence,
	const FTransform& TransformOrigin,
	const TArray<FCCMSequenceBinding>& Bindings)
{
	// Only one sequence at a time: stop any current one
	StopSequence();

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;

	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer* SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this,
		Sequence,
		PlaybackSettings,
		SequenceActor);

	if (!SequenceActor || !SequencePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayGameplaySequenceWithBinding: Failed to create LevelSequencePlayer for Sequence: %s"),
			*GetNameSafe(Sequence));
		return nullptr;
	}

	SequenceActor->bOverrideInstanceData = true;

	if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
	{
		InstanceData->TransformOrigin = TransformOrigin;
	}

	for (const FCCMSequenceBinding& Binding : Bindings)
	{
		if (Binding.TagName.IsNone() || !Binding.BoundActor || !IsValid(Binding.BoundActor))
		{
			continue;
		}

		TArray<AActor*> ActorsToBind;
		ActorsToBind.Add(Binding.BoundActor);
		SequenceActor->SetBindingByTag(Binding.TagName, ActorsToBind, false);
	}

	// Store reference so it can be stopped via StopSequence(); clear when finished
	CurrentLevelSequenceActor = SequenceActor;
	SequencePlayer->OnFinished.AddDynamic(this, &ACCMPlayerCameraManager::OnLevelSequenceFinished);

	SequencePlayer->Play();

	return SequenceActor;
}

void ACCMPlayerCameraManager::StopSequence()
{
	if (!CurrentLevelSequenceActor)
	{
		return;
	}

	if (ULevelSequencePlayer* Player = CurrentLevelSequenceActor->GetSequencePlayer())
	{
		Player->OnFinished.RemoveDynamic(this, &ACCMPlayerCameraManager::OnLevelSequenceFinished);
		Player->Stop();
	}

	CurrentLevelSequenceActor = nullptr;
}

void ACCMPlayerCameraManager::OnLevelSequenceFinished()
{
	if (CurrentLevelSequenceActor)
	{
		if (ULevelSequencePlayer* Player = CurrentLevelSequenceActor->GetSequencePlayer())
		{
			Player->OnFinished.RemoveDynamic(this, &ACCMPlayerCameraManager::OnLevelSequenceFinished);
		}
		CurrentLevelSequenceActor = nullptr;
	}
}

void ACCMPlayerCameraManager::DeactivateCurrentCameraPoint()
{
	if (currentCameraComp)
	{
		currentCameraComp->StopMovement();
		currentCameraComp->OnCameraDeactivated();
		currentCameraComp = nullptr;
	}
}

void ACCMPlayerCameraManager::StopCurrentCameraSequence()
{
	bIsPlayingSequence = false;
	if (UKismetSystemLibrary::IsServer(this)) {
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}

	APlayerController* controller = UGameplayStatics::GetPlayerController(this, 0);
	if (controller && sequenceCameraActor) {
		controller->SetViewTargetWithBlend(playerCharacter, currentSequence.cameraSequenceComp->GetCameraSequenceSettings().OutBlendSettings);
	}
}

void ACCMPlayerCameraManager::AddSequenceEventModifier(const FCCMSequenceEvent& modifier)
{
	if (!bIsPlayingSequence) {
		return;
	}
	if (modifier.bChangeTimeDilation && GetOwningPlayerController()->HasAuthority()) {
		UGameplayStatics::SetGlobalTimeDilation(this, modifier.TimeDilatation);
	}
	if (modifier.bEditCameraSpeed) {
		currentSequenceSettings.CameraSpeed = modifier.CameraSpeed;
	}
	if (modifier.bEditFov) {
		currentSequenceSettings.CameraFov = currentSequenceSettings.CameraFov += modifier.CameraFovOffset;
		currentSequenceSettings.FovInterpSpeed = modifier.FovOffsetInterpSpeed;
	}
	if (modifier.bSwitchLookAt) {
		currentSequenceSettings.lookAtActor = currentSequence.cameraSequenceComp->GetLookAtPointLocationByName(modifier.LookAtPoint);
	}
}

void ACCMPlayerCameraManager::RemoveSequenceEventModifier(const FCCMSequenceEvent& modifier)
{
	if (!bIsPlayingSequence) {
		return;
	}
	if (!currentSequence.cameraSequenceComp) {
		return;
	}

	if (modifier.bChangeTimeDilation) {
		UGameplayStatics::SetGlobalTimeDilation(this, currentSequence.cameraSequenceComp->GetCameraSequenceSettings().TimeDilatation);
	}
	if (modifier.bEditCameraSpeed) {
		currentSequenceSettings.CameraSpeed = currentSequence.cameraSequenceComp->GetCameraSequenceSettings().CameraSpeed;
	}
	if (modifier.bEditFov) {
		currentSequenceSettings.CameraFov = currentSequence.cameraSequenceComp->GetCameraSequenceSettings().CameraFov;
	}
	if (modifier.bSwitchLookAt) {
		currentSequenceSettings.lookAtActor = currentSequence.cameraSequenceComp->GetLookAtPointLocationByName(currentSequence.cameraSequenceComp->GetCameraSequenceSettings().LookAtPoint);
	}
}

void ACCMPlayerCameraManager::OverrideCameraReferences(class UCameraComponent* inPlayerCamera, class USpringArmComponent* inCameraBoom)
{
	playerCamera = inPlayerCamera;
	cameraBoom = inCameraBoom;
	if (cameraBoom && bEnforceCameraProbeChannel) {
		cameraBoom->ProbeChannel = ECC_Camera;
	}
}

void ACCMPlayerCameraManager::SetCameraSequenceData()
{
	if (!sequenceCameraActor) {
		sequenceCameraActor = GetWorld()->SpawnActor<ACameraActor>(CameraActorClass);
	}

	currentSequenceSettings = currentSequence.cameraSequenceComp->GetCameraSequenceSettings();
	currentSequenceSettings.lookAtActor = currentSequence.cameraSequenceComp->GetLookAtPointLocationByName(currentSequenceSettings.LookAtPoint);
	sequenceCameraActor->GetCameraComponent()->SetFieldOfView(currentSequenceSettings.CameraFov);
	FVector startingLoc = currentSequence.cameraSequenceComp->GetLocationAtTime(0.f, ESplineCoordinateSpace::World);
	FRotator rot = UKismetMathLibrary::FindLookAtRotation(startingLoc, currentSequenceSettings.lookAtActor->GetActorLocation());
	sequenceCameraActor->SetActorLocationAndRotation(startingLoc, rot);
}

void ACCMPlayerCameraManager::LockCameraOnPosition_Implementation(FVector localPos, float deltaTime)
{
	if (!playerCharacter || !playerCamera) {
		return;
	}

	if (!playerCharacter->GetController()) {
		return;
	}

	// Hold rotation when the target sits on top of the camera — FindLookAtRotation degenerates
	// and yaw swings wildly (e.g. dodge-roll through the lock target).
	if (MinLockTargetDistance > 0.f &&
		FVector::DistSquared(playerCamera->GetComponentLocation(), localPos) < FMath::Square(MinLockTargetDistance)) {
		return;
	}

	const FRotator OldRotation = playerCharacter->GetController()->GetControlRotation();
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(playerCamera->GetComponentLocation(), localPos);
	FRotator NewRotation = OldRotation;
	TargetRotation.Roll = OldRotation.Roll;
	TargetRotation.Pitch += GlobalLockOffset.Y;
	TargetRotation.Yaw += GlobalLockOffset.X;
	TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch, -CameraLockPitchLowerLimit, CameraLockPitchUpperLimit);

	if (currentLockType == ELockType::EYawOnly) {
		TargetRotation.Pitch = OldRotation.Pitch;
	}
	NewRotation = FMath::RInterpTo(OldRotation, TargetRotation, deltaTime, _lockStrength);

	// Opt-in cap on the per-frame re-center speed, off by default so the lock keeps its
	// original feel unless a project asks for a calmer camera.
	if (MaxLockRotationRate > 0.f) {
		const float MaxStep = MaxLockRotationRate * deltaTime;
		const FRotator Delta = (NewRotation - OldRotation).GetNormalized();
		NewRotation = OldRotation + FRotator(
			FMath::Clamp(Delta.Pitch, -MaxStep, MaxStep),
			FMath::Clamp(Delta.Yaw, -MaxStep, MaxStep),
			0.f);
	}

	playerCharacter->GetController()->SetControlRotation(NewRotation);
}

void ACCMPlayerCameraManager::TriggerCameraEvent(FName CameraEventName)
{
	if (CameraEventName == NAME_None) {
		return;
	}

	if (UKismetSystemLibrary::IsDedicatedServer(this)) {
		return;
	}

	if (CameraMovements) {
		FCCMCameraMovementSettings* mov = CameraMovements->FindRow<FCCMCameraMovementSettings>(CameraEventName, "Searching for camera event");

		if (currentlyActiveCameraEvents.Find(CameraEventName)) {
			UE_LOG(LogTemp, Warning, TEXT("Camera Event Already Triggered - Cinematic Camera Manager"));
			return;
		}

		if (mov) {
			FinalMov += *(mov);
			currentlyActiveCameraEvents.Add(CameraEventName, *mov);
		}
	}
}

void ACCMPlayerCameraManager::StopCameraEvent(FName CameraEventName)
{
	if (CameraEventName == NAME_None) {
		return;
	}

	if (currentlyActiveCameraEvents.Contains(CameraEventName)) {
		FCCMCameraMovementSettings* mov = currentlyActiveCameraEvents.Find(CameraEventName);
		if (mov) {
			FinalMov -= *(mov);
			currentlyActiveCameraEvents.FindAndRemoveChecked(CameraEventName);
		}
	}
}

void ACCMPlayerCameraManager::TriggerTimedCameraEvent(FName CameraEventName, float duration)
{
	if (!UKismetSystemLibrary::IsDedicatedServer(this)) {
		if (currentlyActiveCameraEvents.Find(CameraEventName)) {
			UE_LOG(LogTemp, Warning, TEXT("Camera Event Already Triggered - Cinematic Camera Manager"));
			return;
		}

		UWorld* world = GetWorld();
		if (world) {
			FTimerDelegate TimerDel;
			FTimerHandle TimerHandle;
			TimerDel.BindUFunction(this, FName("StopCameraEvent"), CameraEventName);
			world->GetTimerManager().SetTimer(TimerHandle, TimerDel, duration, false);
			TriggerCameraEvent(CameraEventName);
		}
	}
}

void ACCMPlayerCameraManager::ResetCameraPosition(bool bInstantReset /*= false*/)
{
	// StopCurrentCameraRotationEvent();

	if (!UKismetSystemLibrary::IsDedicatedServer(this)) {
		TArray<FName> iterator;
		currentlyActiveCameraEvents.GetKeys(iterator);

		TArray<FName> iteratorCopy = iterator;
		for (FName cameraevent : iteratorCopy) {
			StopCameraEvent(cameraevent);
		}

		if (bInstantReset && playerCamera && cameraBoom) {
			playerCamera->FieldOfView = originalFov;
			eventSocketOffset = originalPos;
			ApplyFramingSocketOffset();
		}
	}
}

void ACCMPlayerCameraManager::ApplyCameraBasePreset(FVector NewBaseSocketOffset, float NewBaseFOV, float NewBaseTargetArmLength)
{
	if (!cameraBoom || !playerCamera) {
		UE_LOG(LogTemp, Warning, TEXT("Cannot apply camera base preset without valid camera references - Cinematic Camera Manager"));
		return;
	}

	// Preserve each transient layer's current contribution while moving the base underneath it.
	const FVector currentEventOffset = eventSocketOffset - originalPos;
	const float currentEventFov = playerCamera->FieldOfView - originalFov;
	const bool bHasFramingArmOffset = bIsFramingActors || bIsRestoringFraming;
	const float currentFramingArmOffset = bHasFramingArmOffset ? cameraBoom->TargetArmLength - framingBaseArmLength : 0.f;

	originalPos = NewBaseSocketOffset;
	originalFov = NewBaseFOV;
	framingBaseArmLength = NewBaseTargetArmLength;
	eventSocketOffset = originalPos + currentEventOffset;
	cameraBoom->TargetArmLength = framingBaseArmLength + currentFramingArmOffset;
	ApplyFramingSocketOffset();
	playerCamera->FieldOfView = originalFov + currentEventFov;
}

bool ACCMPlayerCameraManager::ApplyCameraPreset(FGameplayTag PresetTag)
{
	if (!PresetTag.IsValid()) {
		return false;
	}

	const FCCMCameraPreset* preset = CameraPresets.Find(PresetTag);
	if (!preset) {
		UE_LOG(LogTemp, Warning, TEXT("Camera preset '%s' was not found - Cinematic Camera Manager"), *PresetTag.ToString());
		return false;
	}

	ApplyCameraPresetSettings(*preset);
	if (!cameraBoom || !playerCamera) {
		return false;
	}

	ActiveCameraPreset = PresetTag;
	return true;
}

void ACCMPlayerCameraManager::ApplyCameraPresetSettings(const FCCMCameraPreset& Preset)
{
	ApplyCameraBasePreset(Preset.SocketOffset, Preset.FOV, Preset.TargetArmLength);
	if (!cameraBoom || !playerCamera) {
		return;
	}

	cameraBoom->TargetOffset = Preset.TargetOffset;
	cameraBoom->bEnableCameraLag = Preset.EnableCameraLag;
	cameraBoom->CameraLagSpeed = Preset.CameraLagSpeed;
	cameraBoom->bEnableCameraRotationLag = Preset.EnableCameraRotationLag;
	cameraBoom->CameraRotationLagSpeed = Preset.CameraRotationLagSpeed;
	ActiveCameraPreset = FGameplayTag();
}

void ACCMPlayerCameraManager::LockCameraOnActor(AActor* ActorLookAt, ELockType lockType, float lockStrength)
{
	if (ActorLookAt) {
		localtarget = ActorLookAt;
		currentLockType = lockType;
		targetLockType = ETargetLockType::EActor;
		_lockStrength = lockStrength;
		// The lock and the framing would be writing the same control rotation, so asking for one
		// gives up the other.
		StopFramingActors();
	}
}

void ACCMPlayerCameraManager::LockCameraOnComponent(class USceneComponent* ComponentToLookAt, ELockType lockType, float lockStrength /*= 5.f*/)
{
	if (ComponentToLookAt) {
		localtargetComponent = ComponentToLookAt;
		currentLockType = lockType;
		targetLockType = ETargetLockType::EComponent;
		_lockStrength = lockStrength;
		StopFramingActors();
	}
}

void ACCMPlayerCameraManager::StopLookingActor()
{
	targetLockType = ETargetLockType::ENone;
	StopFramingActors();
}

void ACCMPlayerCameraManager::PlayCameraBlend(AActor* TargetActor, const FGameplayTag& ComponentTag, float BlendTime, TEnumAsByte<EViewTargetBlendFunction> BlendFunc, float BlendExp, bool bLockOutgoing)
{
	BlendToPoint(TargetActor, ComponentTag, BlendTime, BlendFunc, BlendExp, bLockOutgoing);

}



void ACCMPlayerCameraManager::UpdateCameraReferences()
{
	if (PCOwner && CameraMovements) {
		ResetCameraPosition(true);
		// The framing session belongs to the old pawn's spring arm, so the stored base length
		// means nothing on the new one.
		framedActor = nullptr;
		bIsFramingActors = false;
		bIsRestoringFraming = false;
		framingSocketOffset = FVector::ZeroVector;
		playerCharacter = PCOwner->GetPawn();
		if (playerCharacter) {
			playerCamera = playerCharacter->FindComponentByClass<UCameraComponent>();
			cameraBoom = playerCharacter->FindComponentByClass<USpringArmComponent>();
			if (playerCamera && cameraBoom) {
				originalFov = playerCamera->FieldOfView;
				originalPos = cameraBoom->SocketOffset;
				eventSocketOffset = originalPos;
				// ACF characters clear their response to the Camera channel, so the arm has to probe
				// on that same channel for a character crossing in front of the camera to be
				// ignored instead of yanking the camera in.
				if (bEnforceCameraProbeChannel) {
					cameraBoom->ProbeChannel = ECC_Camera;
				}

				const FGameplayTag presetToApply = ActiveCameraPreset.IsValid() ? ActiveCameraPreset : DefaultCameraPreset;
				if (presetToApply.IsValid()) {
					ApplyCameraPreset(presetToApply);
				}

				return;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Invalid Camera References !! - Cinematic Camera Manager"));
}

void ACCMPlayerCameraManager::MoveCamera(float deltaTime)
{
	if (cameraBoom) {
		const FVector targetpos = FinalMov.CameraOffset + originalPos;
		// Interpolated on its own value rather than on the arm's, so that the framing's share of the
		// offset does not leak into what the camera events think their own offset is.
		eventSocketOffset = FMath::VInterpTo(eventSocketOffset, targetpos, deltaTime, FinalMov.InterpSpeed);
		cameraBoom->SocketOffset = eventSocketOffset + framingSocketOffset;
	}

	if (playerCamera) {
		const float finalfov = FinalMov.FOV + originalFov;
		playerCamera->FieldOfView = FMath::FInterpTo(playerCamera->FieldOfView, finalfov, deltaTime, FinalMov.FovInterpSpeed);
	}

	if (FinalMov.bShakeLooping && PCOwner) {
		PCOwner->ClientStartCameraShake(FinalMov.Shake, FinalMov.ShakeIntensity);
	}
}

void ACCMPlayerCameraManager::StoreData()
{
	if (playerCharacter) {
		_localStrafing = playerCharacter->bUseControllerRotationYaw;
		_localIsLockingAt = targetLockType;
		_localArmLength = cameraBoom->TargetArmLength;
		_localTimeDilatation = 1.f;
		UCharacterMovementComponent* _charmov = playerCharacter->FindComponentByClass<UCharacterMovementComponent>();
		if (_charmov) {
			_localOrientToMov = _charmov->bOrientRotationToMovement;
		}
	}
}

void ACCMPlayerCameraManager::RecoverStoredData()
{
	if (playerCharacter) {
		playerCharacter->bUseControllerRotationYaw = _localStrafing;
		cameraBoom->TargetArmLength = _localArmLength;
		UGameplayStatics::SetGlobalTimeDilation(this, _localTimeDilatation);

		UCharacterMovementComponent* _charmov = playerCharacter->FindComponentByClass<UCharacterMovementComponent>();
		if (_charmov) {
			_charmov->bOrientRotationToMovement = _localOrientToMov;
		}
	}
}

void ACCMPlayerCameraManager::HandlePawnChanged(APawn* newPawn)
{
	UpdateCameraReferences();
}

void ACCMPlayerCameraManager::SetFollowPlayer(bool bFollow, float inFollowSpeed /*= 1.f*/)
{
	bFollowPlayer = bFollow;
	FollowSpeed = inFollowSpeed;
}

void ACCMPlayerCameraManager::FollowPlayer(float deltaTime)
{
	if (bFollowPlayer && playerCharacter && playerCharacter->GetController() && targetLockType == ETargetLockType::ENone) {
		const FRotator OldRotation = playerCharacter->GetController()->GetControlRotation();
		const FRotator TargetRotation = playerCharacter->GetActorForwardVector().Rotation();
		const FRotator NewRotation = FMath::RInterpTo(OldRotation, TargetRotation, deltaTime, FollowSpeed);

		const float deltaAngle = NewRotation.Yaw - OldRotation.Yaw;
		if (bPrintDebugDeltaAngle) {
			GEngine->AddOnScreenDebugMessage(1, 0.2f, FColor::Yellow, FString::Printf(TEXT("Rot Dif: %f"), deltaAngle));
		}
		if (deltaAngle < MaxFollowDeltaAngle) {
			playerCharacter->GetController()->SetControlRotation(FRotator(OldRotation.Pitch, NewRotation.Yaw, OldRotation.Roll));
		}
	}
}

/*
void ACCMPlayerCameraManager::PlayCinematicCameraAnim(UCameraAnimationSequence* CameraAnimAsset, USceneComponent* ReferencePoint)
{
	if (!CameraAnimAsset || !ReferencePoint) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid camera animation or reference point"));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) {
		return;
	}

	// 1. Spawn or reuse a temporary fixed camera actor
	static ACameraActor* CachedFixedCamera = nullptr;
	if (!CachedFixedCamera || !CachedFixedCamera->IsValidLowLevel()) {
		UWorld* World = ReferencePoint->GetWorld();
		CachedFixedCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
		CachedFixedCamera->SetActorTickEnabled(false);
	}

	// 2. Align the fixed camera with the reference point
	CachedFixedCamera->SetActorLocation(ReferencePoint->GetComponentLocation());
	CachedFixedCamera->SetActorRotation(ReferencePoint->GetComponentRotation());

	// 3. Blend to the fixed camera
	PC->SetViewTargetWithBlend(CachedFixedCamera, 0.5f);

	// 4. Retrieve or add the camera modifier used for camera animations
	UCameraAnimationCameraModifier* CamMod = Cast<UCameraAnimationCameraModifier>(
		PC->PlayerCameraManager->FindCameraModifierByClass(UCameraAnimationCameraModifier::StaticClass()));

	if (!CamMod) {
		CamMod = Cast<UCameraAnimationCameraModifier>(
			PC->PlayerCameraManager->AddNewCameraModifier(UCameraAnimationCameraModifier::StaticClass()));
	}

	// 5. Set up camera animation parameters
	FCameraAnimationParams AnimParams;
	AnimParams.PlaySpace = ECameraAnimationPlaySpace::CameraLocal;
	AnimParams.bLoop = false;
	AnimParams.bRandomStartTime = false;
	AnimParams.Scale = 1.0f;
	AnimParams.DurationOverride = -1.f; // use the asset's duration

	// 6. Play the camera animation
	if (CamMod)
	{
		CamMod->PlayCameraAnimation(CameraAnimAsset, AnimParams);
	}

	// 7. (Optional) Return to player view after the animation ends
	const float DelayBeforeReturn = CameraAnimAsset->GetPlayLength();
	FTimerHandle ReturnHandle;
	FTimerDelegate ReturnDelegate = FTimerDelegate::CreateLambda([PC]() {
		PC->SetViewTargetWithBlend(PC->GetPawn(), 0.5f);
	});

	PC->GetWorld()->GetTimerManager().SetTimer(ReturnHandle, ReturnDelegate, DelayBeforeReturn, false);
}
*/