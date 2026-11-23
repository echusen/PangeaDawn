// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFMusicFunctionLibrary.h"
#include "ACFMusicComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UACFMusicComponent* UACFMusicFunctionLibrary::GetMusicManager(const UObject* WorldContextObject)
{
	if (!WorldContextObject) {
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) {
		return nullptr;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) {
		return nullptr;
	}

	return PC->FindComponentByClass<UACFMusicComponent>();
}
