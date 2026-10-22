#include "Components/PangeaGASStatisticsComponent.h"

#include "Actors/ACFCharacter.h"
#include "Components/PangeaCharacterInitializerComponent.h"
#include "Data/ACFCharacterInitializerComponent.h"

void UPangeaGASStatisticsComponent::BeginPlay()
{
	if (const AACFCharacter* Character = Cast<AACFCharacter>(GetOwner()))
	{
		const UACFCharacterInitializerComponent* Initializer = Character->GetInitializerComponent();
		const UPangeaCharacterInitializerComponent* PangeaInitializer = Cast<UPangeaCharacterInitializerComponent>(Initializer);
		if (Initializer && (Initializer->GetAutoInitDataAsset() ||
			(PangeaInitializer && PangeaInitializer->GetEffectiveCharacterProfile())))
		{
			// ACF's initializer performs this pass itself, regardless of component BeginPlay order.
			// Keep Super::BeginPlay for its difficulty hooks, ASC cache and health delegate.
			bAutoInitialize = false;
		}
	}
	Super::BeginPlay();
}
