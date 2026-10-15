// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "AMSActorMarker.h"
#include "AMSMapMarkerComponent.h"
#include "AMSMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AAMSActorMarker::AAMSActorMarker()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	MarkerComp = CreateDefaultSubobject<UAMSMapMarkerComponent>(TEXT("Marker Comp"));
}

// Called when the game starts or when spawned
void AAMSActorMarker::BeginPlay()
{
	Super::BeginPlay();
	if (MarkerComp) {
		MarkerComp->AddMarker();
		MarkerComp->SetTracked(true);
	}
}

void AAMSActorMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this)) {
		if (UAMSMapSubsystem* MapSubsystem = GameInstance->GetSubsystem<UAMSMapSubsystem>()) {
			MapSubsystem->UnregisterMarkerActor(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

