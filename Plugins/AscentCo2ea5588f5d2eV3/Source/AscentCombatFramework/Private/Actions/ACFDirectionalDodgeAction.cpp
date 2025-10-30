// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Actions/ACFDirectionalDodgeAction.h"
#include "ATSTargetingFunctionLibrary.h"
#include "Actors/ACFCharacter.h"
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
    const ENetMode NetMode = GetCharacterOwner()->GetNetMode();

    if (NetMode == NM_Standalone) {
        // Single player: uses local input
        dodgeDirection = GetCharacterOwner()->GetCharacterMovement()->GetPendingInputVector();
    } else {
        // Multiplayer: uses replicated acceleration
        dodgeDirection = GetCharacterOwner()->GetCharacterMovement()->GetCurrentAcceleration();
    }
    if (FMath::IsNearlyZero(dodgeDirection.Size())) {
        finalDirection = defaultDodgeDirection;
    } else {
        finalDirection = UACFFunctionLibrary::GetDirectionFromInput(GetCharacterOwner(), dodgeDirection);
    }
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

FTransform UACFDirectionalDodgeAction::GetWarpTransform_Implementation()
{
    FRotator finalRot;
    FVector finalPos;

    if (GetCharacterOwner() && animMontage) {
        TObjectPtr<AActor> target = TryGetCurrentTarget();
        FRotator rotateDirection;
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
            dodgeDirection = rotateDirection.RotateVector(GetCharacterOwner()->GetActorForwardVector());
        }

        if (bConstraintRotationAroundTarged) {

            if (target) {
                switch (finalDirection) {

                case EACFDirection::Right:
                    rotateDirection = FRotator(0.f, 60.f, 0.f);
                    dodgeDirection = rotateDirection.RotateVector(GetCharacterOwner()->GetActorForwardVector());
                    break;
                case EACFDirection::Left:
                    rotateDirection = FRotator(0.f, 300.f, 0.f);
                    dodgeDirection = rotateDirection.RotateVector(GetCharacterOwner()->GetActorForwardVector());
                    break;
                default:
                    break;
                }
            }
        }

        finalPos = UACFFunctionLibrary::GetPointAtDirectionAndDistanceFromActor(GetCharacterOwner(), dodgeDirection, DodgeLength, ActionConfig.WarpInfo.bShowWarpDebug);

        if (bConstraintRotationAroundTarged && target) {
            finalRot = UKismetMathLibrary::FindLookAtRotation(finalPos, target->GetActorLocation());
        } else if (ActionConfig.WarpInfo.RotationType == EMotionWarpRotationType::Facing) {
            finalRot = UKismetMathLibrary::FindLookAtRotation(GetCharacterOwner()->GetActorLocation(), finalPos);
        } else {
            finalRot = GetCharacterOwner()->GetActorForwardVector().Rotation();
        }
        finalRot.Pitch = 0.f;
        finalRot.Roll = 0.f;
    }

    return FTransform(finalRot, finalPos);
}

void UACFDirectionalDodgeAction::OnActionStarted_Implementation()
{
    bool bIsValid = true;
    if (!bIsValid) {
        UE_LOG(LogTemp, Warning, TEXT("INVALID Dodge Direaction - UACFDirectionalDodgeAction::OnActionStarted_Implementation "));
    }
    Super::OnActionStarted_Implementation();
}

void UACFDirectionalDodgeAction::OnActionEnded_Implementation()
{
    Super::OnActionEnded_Implementation();
    AACFCharacter* acfCharacter = Cast<AACFCharacter>(GetCharacterOwner());

    if (acfCharacter) {
        acfCharacter->SetIsImmortal(false);
    }
}

FName UACFDirectionalDodgeAction::GetMontageSectionName_Implementation()
{
    const FName* section = DodgeDirectionToMontageSectionMap.Find(finalDirection);

    if (section) {
        return *section;
    }

    return Super::GetMontageSectionName_Implementation();
}
