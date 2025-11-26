// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.


#include "ANSUIConfigComponent.h"
#include <Engine/DataTable.h>

// Sets default values for this component's properties
UANSUIConfigComponent::UANSUIConfigComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UANSUIConfigComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...

}
UDataTable* UANSUIConfigComponent::GetIconsByTag() const
{
	return IconsByTag;
}

UDataTable* UANSUIConfigComponent::GetKeysConfigForPlatform(const FString& Platform) const
{

	if (!KeysConfigByPlatform.Contains(Platform)) {
		UE_LOG(LogTemp, Error, TEXT("UANSUIConfigComponent::GetKeysConfigForPlatform - Add keys icons data table in UIConfig Component in Player Controller!"));
		return nullptr;
	}

	// Check if platform key exists in the map
	if (const TSoftObjectPtr<UDataTable>* Found = KeysConfigByPlatform.Find(Platform))
	{
		// Attempt to load the data table synchronously
		UDataTable* LoadedDataTable = Found->LoadSynchronous();

		// Verify that the data table was successfully loaded
		if (!LoadedDataTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("UANSUIConfigComponent::GetKeysConfigForPlatform - Failed to load DataTable for platform '%s'. The asset path may be invalid or the asset may not exist."), *Platform);
			return nullptr;
		}

		return LoadedDataTable;
	}

	// Platform not found in the configuration map
	UE_LOG(LogTemp, Warning, TEXT("UANSUIConfigComponent::GetKeysConfigForPlatform - Platform '%s' not found in KeysConfigByPlatform map."), *Platform);
	return nullptr;
}



