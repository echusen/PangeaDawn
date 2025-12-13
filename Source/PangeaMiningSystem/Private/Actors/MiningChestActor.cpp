#include "Actors/MiningChestActor.h"

AMiningChestActor::AMiningChestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	StorageComponent = CreateDefaultSubobject<UACFStorageComponent>(TEXT("StorageComponent"));
}