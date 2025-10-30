// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Actions/ACFAdvancedHitAction.h"
#include "ACFActionsFunctionLibrary.h"
#include "Actions/ACFActionAbility.h"
#include "Actors/ACFCharacter.h"
#include "Animation/ACFAnimInstance.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"

UACFAdvancedHitAction::UACFAdvancedHitAction()
{
    HitDirectionToMontageSectionMap.Add(EHitDirection::EFront, FName("Front"));
    HitDirectionToMontageSectionMap.Add(EHitDirection::EBack, FName("Back"));
    HitDirectionToMontageSectionMap.Add(EHitDirection::ERight, FName("Right"));
    HitDirectionToMontageSectionMap.Add(EHitDirection::ELeft, FName("Left"));
    ActionConfig.bStopBehavioralThree = true;
    ActionConfig.MontageReproductionType = EMontageReproductionType::EMotionWarped;
}

FTransform UACFAdvancedHitAction::GetWarpTransform_Implementation()
{
    AACFCharacter* acfCharacter = Cast<AACFCharacter>(GetCharacterOwner());
    FVector location = GetCharacterOwner()->GetActorLocation();
    FRotator rot = GetCharacterOwner()->GetActorForwardVector().Rotation();

    if (acfCharacter && animMontage) {

        const FACFDamageEvent damageReceived = acfCharacter->GetLastDamageInfo();
        const FVector damageMomentum = UACFFunctionLibrary::GetActorsRelativeDirectionVector(damageReceived);
        
        if (!damageMomentum.IsNearlyZero()) {
            const FVector offset = damageMomentum * hitWarpDistance;
            location += offset;
        }

        if (damageReceived.DamageDealer) {
            rot = UKismetMathLibrary::FindLookAtRotation(location, damageReceived.DamageDealer->GetActorLocation());
        }

        rot.Pitch = 0.f;
        rot.Roll = 0.f;
    }

    return FTransform(rot, location, FVector(1.f));
}

void UACFAdvancedHitAction::OnActionStarted_Implementation()
{
    Super::OnActionStarted_Implementation();

    AACFCharacter* acfCharacter = Cast<AACFCharacter>(GetCharacterOwner());

    if (acfCharacter) {
        UACFAbilitySystemComponent* actionsMan = acfCharacter->GetActionsComponent();
        if (actionsMan) {
            // Cleans the input buffer when staggered
            actionsMan->StoreAbilityInBuffer(FGameplayTag());
        }
    }
}

void UACFAdvancedHitAction::OnActionEnded_Implementation()
{
    Super::OnActionEnded_Implementation();
}

FName UACFAdvancedHitAction::GetMontageSectionName_Implementation()
{
    const AACFCharacter* acfCharacter = Cast<AACFCharacter>(GetCharacterOwner());

    if (!acfCharacter) {
        return Super::GetMontageSectionName_Implementation();
    }

    const FACFDamageEvent damageReceived = acfCharacter->GetLastDamageInfo();

    const EACFDirection direction = damageReceived.DamageDirection;

    switch (direction) {
    case EACFDirection::Front:
        return GetMontageSectionFromFront(damageReceived);
    case EACFDirection::Back:
        return GetMontageSectionFromHitDirectionSafe(EHitDirection::EBack);
    case EACFDirection::Left:
        return GetMontageSectionFromHitDirectionSafe(EHitDirection::ELeft);
    case EACFDirection::Right:
        return GetMontageSectionFromHitDirectionSafe(EHitDirection::ERight);
    }

    return GetMontageSectionFromHitDirectionSafe(EHitDirection::EFront);
}

FName UACFAdvancedHitAction::GetMontageSectionFromHitDirectionSafe(const EHitDirection hitDir) const
{
    const FName* section = HitDirectionToMontageSectionMap.Find(hitDir);

    if (section) {
        return *section;
    }

    return NAME_None;
}

FName UACFAdvancedHitAction::GetMontageSectionFromFront(const FACFDamageEvent& damageReceived) const
{
    const FName boneName = damageReceived.hitResult.BoneName;

    for (const auto& section : FrontDetailsSectionByBoneNames) {
        if (section.BoneNames.Contains(boneName)) {
            return section.SectionName;
        }
    }

    return GetMontageSectionFromHitDirectionSafe(EHitDirection::EFront);
}
