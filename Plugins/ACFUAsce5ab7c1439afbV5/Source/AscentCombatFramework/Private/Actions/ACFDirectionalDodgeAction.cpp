// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFDirectionalDodgeAction.h"
#include "ATSTargetingFunctionLibrary.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFDestructableComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include <GameFramework/CharacterMovementComponent.h>

UACFDirectionalDodgeAction::UACFDirectionalDodgeAction()
{
	DodgeDirectionToMontageSectionMap.Add(EACFDirection::Front, FName("Front"));
	DodgeDirectionToMontageSectionMap.Add(EACFDirection::Back, FName("Back"));
	DodgeDirectionToMontageSectionMap.Add(EACFDirection::Right, FName("Right"));
	DodgeDirectionToMontageSectionMap.Add(EACFDirection::Left, FName("Left"));
	ActionConfig.bStopBehavioralThree = true;
	ActionConfig.MontageReproductionType = EMontageReproductionType::EMotionWarped;
}

TObjectPtr<AActor> UACFDirectionalDodgeAction::TryGetCurrentTarget()
{
	return UATSTargetingFunctionLibrary::GetTargetedActor(GetCharacterOwner());
}

void UACFDirectionalDodgeAction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{

	UCharacterMovementComponent* charMov = GetCharacterOwner()->GetCharacterMovement();
	if (charMov) {
		if (GetTriggerPayload().bIsValid) {
			dodgeDirection = GetTriggerPayload().VectorPayload;
		}
		else {
			dodgeDirection = charMov->GetCurrentAcceleration();
		}
	}

	if (FMath::IsNearlyZero(dodgeDirection.Size())) {
		finalDirection = defaultDodgeDirection;
	}
	else {
		finalDirection = UACFFunctionLibrary::GetDirectionFromInput(GetCharacterOwner(), dodgeDirection);
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UACFDirectionalDodgeAction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

}

FTransform UACFDirectionalDodgeAction::GetWarpTransform_Implementation()
{
	ACharacter* CharOwner = GetCharacterOwner();
	if (!CharOwner) {
		return FTransform::Identity;
	}

	FRotator finalRot = CharOwner->GetActorRotation();
	FVector finalPos = CharOwner->GetActorLocation();

	FRotator rotateDirection = FRotator::ZeroRotator;
	if (FMath::IsNearlyZero(dodgeDirection.Size())) {

		switch (defaultDodgeDirection) {
		case EACFDirection::Back:
			rotateDirection = FRotator(0.f, 180.f, 0.f);
			break;
		case EACFDirection::Front:
			rotateDirection = FRotator(0.f, 0.f, 0.f);
			break;
		case EACFDirection::Right:
			rotateDirection = FRotator(0.f, 90.f, 0.f);
			break;
		case EACFDirection::Left:
			rotateDirection = FRotator(0.f, 270.f, 0.f);
			break;
		}
		dodgeDirection = rotateDirection.RotateVector(CharOwner->GetActorForwardVector());
	}

	finalPos = UACFFunctionLibrary::GetPointAtDirectionAndDistanceFromActor(CharOwner, dodgeDirection, DodgeLength, ActionConfig.WarpInfo.bShowWarpDebug);
	if (!IsValidDodgeWarpLocation(finalPos)) {
		finalPos = GetSafeDodgeWarpLocation();
	}

	finalRot = UKismetMathLibrary::FindLookAtRotation(CharOwner->GetActorLocation(), finalPos);
	finalRot.Pitch = 0.f;
	finalRot.Roll = 0.f;

	return FTransform(finalRot, finalPos);
}

void UACFDirectionalDodgeAction::GetWarpInfo_Implementation(FACFWarpReproductionInfo& outWarpInfo)
{
	Super::GetWarpInfo_Implementation(outWarpInfo);

	const ACharacter* charOwner = GetCharacterOwner();
	if (!charOwner || outWarpInfo.WarpConfig.TargetType != EWarpTargetType::ETargetTransform) {
		return;
	}

	if (IsValidDodgeWarpLocation(outWarpInfo.WarpLocation)) {
		return;
	}

	outWarpInfo.WarpLocation = GetSafeDodgeWarpLocation();
	const FVector ownerLoc = charOwner->GetActorLocation();
	outWarpInfo.WarpRotation = (outWarpInfo.WarpLocation - ownerLoc).GetSafeNormal2D().Rotation();
	if (outWarpInfo.WarpRotation.IsNearlyZero()) {
		outWarpInfo.WarpRotation = charOwner->GetActorRotation();
	}

	UE_LOG(LogTemp, Warning, TEXT("Invalid dodge warp point, falling back to %s - UACFDirectionalDodgeAction::GetWarpInfo_Implementation"),
		*outWarpInfo.WarpLocation.ToString());
}

bool UACFDirectionalDodgeAction::IsValidDodgeWarpLocation(const FVector& WarpLocation) const
{
	const ACharacter* CharOwner = GetCharacterOwner();
	if (!CharOwner || WarpLocation.ContainsNaN() || WarpLocation.IsNearlyZero(1.f)) {
		return false;
	}

	return FVector::Dist2D(WarpLocation, CharOwner->GetActorLocation()) <= DodgeLength + 1.f;
}

FVector UACFDirectionalDodgeAction::GetSafeDodgeWarpLocation() const
{
	const ACharacter* CharOwner = GetCharacterOwner();
	if (!CharOwner) {
		return FVector::ZeroVector;
	}

	FVector FallbackDir = dodgeDirection.GetSafeNormal2D();
	if (FallbackDir.IsNearlyZero()) {
		FallbackDir = CharOwner->GetActorForwardVector().GetSafeNormal2D();
	}
	if (FallbackDir.IsNearlyZero()) {
		FallbackDir = FVector::ForwardVector;
	}

	FVector WarpLocation = CharOwner->GetActorLocation() + (FallbackDir * DodgeLength);
	if (WarpLocation.IsNearlyZero(1.f)) {
		WarpLocation = CharOwner->GetActorLocation();
	}
	return WarpLocation;
}

void UACFDirectionalDodgeAction::OnActionStarted_Implementation()
{
	bool bIsValid = true;
	if (!bIsValid) {
		UE_LOG(LogTemp, Warning, TEXT("INVALID Dodge Direaction - UACFDirectionalDodgeAction::OnActionStarted_Implementation "));
	}

	if (bDestroyDestructablesOnContact && GetCharacterOwner()) {
		GetCharacterOwner()->OnActorHit.AddUniqueDynamic(this, &UACFDirectionalDodgeAction::HandleOwnerHit);
		GetCharacterOwner()->OnActorBeginOverlap.AddUniqueDynamic(this, &UACFDirectionalDodgeAction::HandleOwnerBeginOverlap);
	}
	Super::OnActionStarted_Implementation();
}

void UACFDirectionalDodgeAction::OnActionEnded_Implementation()
{
	Super::OnActionEnded_Implementation();

	if (GetCharacterOwner()) {
		GetCharacterOwner()->OnActorHit.RemoveDynamic(this, &UACFDirectionalDodgeAction::HandleOwnerHit);
		GetCharacterOwner()->OnActorBeginOverlap.RemoveDynamic(this, &UACFDirectionalDodgeAction::HandleOwnerBeginOverlap);
	}

	if (UACFDamageHandlerComponent* damageComp = GetCharacterOwner()->FindComponentByClass<UACFDamageHandlerComponent>()) {
		damageComp->SetIsImmortal(false);
	}
}

void UACFDirectionalDodgeAction::HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	TryDestroyDestructable(OtherActor, Hit);
}

void UACFDirectionalDodgeAction::HandleOwnerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	FHitResult SyntheticHit;
	if (OtherActor) {
		SyntheticHit.ImpactPoint = OtherActor->GetActorLocation();
		SyntheticHit.Location = SyntheticHit.ImpactPoint;
	}
	TryDestroyDestructable(OtherActor, SyntheticHit);
}

void UACFDirectionalDodgeAction::TryDestroyDestructable(AActor* OtherActor, const FHitResult& Hit)
{
	UACFDestructableComponent* Destructable = OtherActor ? OtherActor->FindComponentByClass<UACFDestructableComponent>() : nullptr;
	if (!Destructable) {
		return;
	}

	ACharacter* CharOwner = GetCharacterOwner();
	if (!CharOwner || !CharOwner->HasAuthority()) {
		return;
	}

	FACFDamageEvent DamageEvent;
	DamageEvent.DamageDealer = CharOwner;
	DamageEvent.DamageReceiver = OtherActor;
	DamageEvent.hitResult = Hit;

	// ACF convention: hitDirection points from the victim back toward the damager
	DamageEvent.hitDirection = (CharOwner->GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal2D();

	// Same validation path as weapon hits: team check + first-hit rule inside the component
	Destructable->HandleHitDestruction(DamageEvent);
}

FName UACFDirectionalDodgeAction::GetMontageSectionName_Implementation()
{
	const FName* section = DodgeDirectionToMontageSectionMap.Find(finalDirection);

	if (section) {
		return *section;
	}

	return Super::GetMontageSectionName_Implementation();
}
