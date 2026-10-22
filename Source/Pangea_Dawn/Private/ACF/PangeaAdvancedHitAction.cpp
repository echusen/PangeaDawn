#include "ACF/PangeaAdvancedHitAction.h"
#include "ACF/PangeaACFIntegrationLibrary.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "GameFramework/Character.h"

UPangeaAdvancedHitAction::UPangeaAdvancedHitAction()
{
    HitDirectionToMontageSectionMap.Add(EACFDirection::Front, FName(TEXT("Front")));
    HitDirectionToMontageSectionMap.Add(EACFDirection::Back, FName(TEXT("Back")));
    HitDirectionToMontageSectionMap.Add(EACFDirection::Right, FName(TEXT("Right")));
    HitDirectionToMontageSectionMap.Add(EACFDirection::Left, FName(TEXT("Left")));
    ActionConfig.bStopBehavioralThree = true;
    ActionConfig.MontageReproductionType = EMontageReproductionType::EMotionWarped;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

FTransform UPangeaAdvancedHitAction::GetWarpTransform_Implementation()
{
    ACharacter* Character = GetCharacterOwner();
    if (!IsValid(Character)) return FTransform::Identity;
    FVector Location = Character->GetActorLocation();
    FRotator Rotation = Character->GetActorForwardVector().Rotation();
    if (animMontage)
    {
        const AActor* Target = StoredPayload.TargetActor;
        Location += UPangeaACFIntegrationLibrary::GetSafeRelativeDirection(Character, Target) * HitWarpDistance;
        if (IsValid(Target) && Target != Character)
        {
            const FVector Facing = Target->GetActorLocation() - Location;
            if (!FVector2D(Facing.X, Facing.Y).IsNearlyZero()) Rotation = Facing.Rotation();
        }
        Rotation.Pitch = 0.f;
        Rotation.Roll = 0.f;
    }
    return FTransform(Rotation, Location, FVector::OneVector);
}

FName UPangeaAdvancedHitAction::GetMontageSectionName_Implementation()
{
    if (!HasValidPayload()) return Super::GetMontageSectionName_Implementation();
    const FACFAbilityPayload Payload = GetTriggerPayload();
    const EACFDirection Direction = IsValid(Payload.TargetActor) && IsValid(Payload.HitResult.GetActor())
        ? UACFFunctionLibrary::GetHitDirectionByHitResult(Payload.TargetActor, Payload.HitResult)
        : EACFDirection::Front;
    if (Direction == EACFDirection::Front)
        for (const FBoneSections& Section : FrontDetailsSectionByBoneNames)
            if (Section.BoneNames.Contains(Payload.HitResult.BoneName)) return Section.SectionName;
    const FName* Section = HitDirectionToMontageSectionMap.Find(Direction);
    if (!Section) Section = HitDirectionToMontageSectionMap.Find(EACFDirection::Front);
    return Section ? *Section : NAME_None;
}

void UPangeaAdvancedHitAction::OnActionStarted_Implementation()
{
    Super::OnActionStarted_Implementation();
    if (ACharacter* Character = GetCharacterOwner())
        if (UACFAbilitySystemComponent* Actions = Character->FindComponentByClass<UACFAbilitySystemComponent>())
            Actions->StoreAbilityInBuffer(FGameplayTag());
}
