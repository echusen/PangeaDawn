#include "ACF/PangeaCharacterSetupComponent.h"
#include "ACF/PangeaACFIntegrationLibrary.h"
#include "Actors/ACFCharacter.h"
#include "ARSLevelingComponent.h"

UPangeaCharacterSetupComponent::UPangeaCharacterSetupComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bWantsInitializeComponent = true;
}

void UPangeaCharacterSetupComponent::InitializeComponent()
{
    Super::InitializeComponent();
    if (AACFCharacter* Character = Cast<AACFCharacter>(GetOwner()))
        Character->OnCharacterFullyInitialized.AddUniqueDynamic(this, &ThisClass::HandleCharacterInitialized);
    ApplyPangeaLevelingPolicy();
}

void UPangeaCharacterSetupComponent::BeginPlay()
{
    Super::BeginPlay();
    ApplyPangeaLevelingPolicy();
}

void UPangeaCharacterSetupComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AACFCharacter* Character = Cast<AACFCharacter>(GetOwner()))
        Character->OnCharacterFullyInitialized.RemoveDynamic(this, &ThisClass::HandleCharacterInitialized);
    Super::EndPlay(EndPlayReason);
}

bool UPangeaCharacterSetupComponent::ApplyPangeaLevelingPolicy()
{
    return UPangeaACFIntegrationLibrary::EnsureFixedDeathRewardCurve(
        GetOwner() ? GetOwner()->FindComponentByClass<UARSLevelingComponent>() : nullptr);
}

void UPangeaCharacterSetupComponent::HandleCharacterInitialized()
{
    ApplyPangeaLevelingPolicy();
}
