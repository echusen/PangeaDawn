#include "Components/PangeaCharacterInitializerComponent.h"

#include "Data/ACFCharacterDataAsset.h"
#include "Definitions/PangeaACFProfileFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PDDefinitionProviderInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaCharacterProfile, Log, All);

UACFCharacterDataAsset* UPangeaCharacterInitializerComponent::FindDefinitionDefault() const
{
	const AActor* Owner = GetOwner();
	const IPDDefinitionProviderInterface* Provider = Cast<IPDDefinitionProviderInterface>(Owner);
	if (!Provider) return nullptr;
	// Actor::ProcessEvent suppresses interface Execute_ dispatch while a loaded world
	// is still initializing. This early configuration must come from the native
	// provider (APDDinosaurBase's serialized CreatureDefinition), not a Blueprint graph.
	const UPangeaCreatureDefinition* Definition = Provider->GetCreatureDefinition_Implementation();
	if (!Definition) return nullptr;

	const UPangeaACFProfileFragment* Selected = nullptr;
	for (const UPangeaDefinitionFragment* Fragment : Definition->Fragments)
	{
		if (const UPangeaACFProfileFragment* ProfileFragment = Cast<UPangeaACFProfileFragment>(Fragment))
		{
			// Do not choose an arbitrary first profile when a definition is malformed.
			if (Selected) return nullptr;
			Selected = ProfileFragment;
		}
	}
	return Selected ? Selected->CharacterProfile.Get() : nullptr;
}

UACFCharacterDataAsset* UPangeaCharacterInitializerComponent::GetEffectiveCharacterProfile() const
{
	if (CharacterDataAsset) return CharacterDataAsset;
	if (CharacterInitDataAsset) return CharacterInitDataAsset;
	return FindDefinitionDefault();
}

EPangeaCharacterProfileSource UPangeaCharacterInitializerComponent::GetCharacterProfileSource() const
{
	const UACFCharacterDataAsset* Selected = GetEffectiveCharacterProfile();
	if (!Selected) return EPangeaCharacterProfileSource::Unconfigured;
	if (CharacterDataAsset || CharacterInitDataAsset)
		return DefinitionDefaultUsed.Get() == Selected
			? EPangeaCharacterProfileSource::DefinitionDefault : EPangeaCharacterProfileSource::ExplicitOverride;
	return EPangeaCharacterProfileSource::DefinitionDefault;
}

void UPangeaCharacterInitializerComponent::InitializeComponent()
{
	// This only selects input to ACF's normal lifecycle. Never call InitFromDataAsset here:
	// it clears abilities/reinitializes stats and would add a second initialization pass.
	if (!IsTemplate())
	{
		OnInitializationCompleted.AddUniqueDynamic(this, &ThisClass::HandleProfileInitializationCompleted);
		if (CharacterDataAsset)
		{
			// A deferred spawner may have supplied a profile before component initialization.
			CharacterInitDataAsset = CharacterDataAsset;
		}
		else if (!CharacterInitDataAsset)
		{
			CharacterInitDataAsset = FindDefinitionDefault();
			DefinitionDefaultUsed = CharacterInitDataAsset.Get();
		}

		UE_LOG(LogPangeaCharacterProfile, Verbose, TEXT("%s selected profile %s (source %d)"),
			*GetNameSafe(GetOwner()), *GetNameSafe(GetEffectiveCharacterProfile()), static_cast<int32>(GetCharacterProfileSource()));
	}
	// ACF applies appearance in InitializeComponent, then gameplay data in BeginPlay.
	Super::InitializeComponent();
}

void UPangeaCharacterInitializerComponent::HandleProfileInitializationCompleted(UACFCharacterDataAsset* Profile)
{
	bProfileInitializationCompleted = IsValid(Profile);
}

void UPangeaCharacterInitializerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnInitializationCompleted.RemoveDynamic(this, &ThisClass::HandleProfileInitializationCompleted);
	bProfileInitializationCompleted = false;
	Super::EndPlay(EndPlayReason);
}
