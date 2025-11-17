// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/FacilityManagerComponent.h"

#include "AMSMapMarkerComponent.h"
#include "GameplayTagAssetInterface.h"
#include "Actors/FacilityMarker.h"
#include "Kismet/GameplayStatics.h"

void UFacilityManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	DiscoverFacilityMarkersInWorld();
}

bool UFacilityManagerComponent::GetFacility(FGameplayTag FacilityTag, FFacilityEntry*& OutEntry)
{
	for (FFacilityEntry& Entry : Facilities)
	{
		if (Entry.FacilityTag == FacilityTag)
		{
			OutEntry = &Entry;
			return true;
		}
	}
	OutEntry = nullptr;
	return false;
}

void UFacilityManagerComponent::DiscoverFacilityMarkersInWorld()
{
	FacilityMarkers.Empty();
    FacilityNPCs.Empty();
    Facilities.Empty(); // Clear existing facilities

    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Starting facility discovery..."));

    // Discover markers
    TArray<AActor*> AllMarkers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFacilityMarker::StaticClass(), AllMarkers);

    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Found %d facility markers"), AllMarkers.Num());

    for (AActor* Marker : AllMarkers)
    {
        FGameplayTagContainer OwnedTags = GetActorGameplayTags(Marker);
        for (const FGameplayTag& Tag : OwnedTags)
        {
            FacilityMarkers.Add(Tag, Marker);

            // ADD THIS: Create facility entry
            FFacilityEntry NewEntry;
            NewEntry.FacilityTag = Tag;
            NewEntry.bUnlocked = false; // Start disabled
            NewEntry.FacilityActor = Marker;
            Facilities.Add(NewEntry);

            UE_LOG(LogTemp, Log, TEXT("  - Registered marker '%s' with tag '%s'"),
                *Marker->GetName(), *Tag.ToString());
        }
    }

    // Try a more robust approach - get all actors first
    FGameplayTag NPCMarkerTag = FGameplayTag::RequestGameplayTag(FName("Marker.NPC"));
    int32 NPCsFound = 0;
    int32 MapMarkersFound = 0;

    // Get ALL actors
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Checking %d total actors for map markers..."), AllActors.Num());

    for (AActor* Actor : AllActors)
    {
        UAMSMapMarkerComponent* MapMarker = Actor->FindComponentByClass<UAMSMapMarkerComponent>();

        if (MapMarker)
        {
            MapMarkersFound++;
            FGameplayTagContainer MarkerCategory = MapMarker->GetMarkerCategory();

            if (MarkerCategory.HasTag(NPCMarkerTag))
            {
                FGameplayTagContainer OwnedTags = GetActorGameplayTags(Actor);

                for (const FGameplayTag& Tag : OwnedTags)
                {
                    if (FacilityMarkers.Contains(Tag))
                    {
                        FacilityNPCs.FindOrAdd(Tag).Add(Actor);
                        NPCsFound++;
                        UE_LOG(LogTemp, Log, TEXT("  - Registered NPC '%s' with facility tag '%s'"),
                            *Actor->GetName(), *Tag.ToString());
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Found %d actors with AMSMapMarkerComponent"), MapMarkersFound);
    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Discovery complete. Found %d NPCs with facility tags"), NPCsFound);
    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Total facility categories: %d"), FacilityNPCs.Num());
    UE_LOG(LogTemp, Log, TEXT("FacilityManager: Total registered facilities: %d"), Facilities.Num());

    RefreshNPCStates();
}

AActor* UFacilityManagerComponent::GetFacilityMarker(FGameplayTag FacilityTag) const
{
	if (AActor* const* FoundMarker = FacilityMarkers.Find(FacilityTag))
	{
		return *FoundMarker;
	}
    
	return nullptr;
}

void UFacilityManagerComponent::RegisterFacilityMarker(AFacilityMarker* Marker)
{
	if (!Marker)
		return;

	FFacilityEntry NewEntry;
	NewEntry.FacilityTag = Marker->FacilityTag;
	NewEntry.bUnlocked = false;
	NewEntry.FacilityActor = Marker;

	Facilities.Add(NewEntry);

	UE_LOG(LogTemp, Log, TEXT("FacilityManager: Registered marker %s (%s)"),
		   *Marker->GetName(),
		   *Marker->FacilityTag.ToString());
}

bool UFacilityManagerComponent::IsFacilityEnabled(FGameplayTag FacilityTag) const
{
	for (const FFacilityEntry& Entry : Facilities)
	{
		if (Entry.FacilityTag == FacilityTag)
		{
			return Entry.bUnlocked;
		}
	}
	return false;
}

void UFacilityManagerComponent::RefreshNPCStates()
{
	UE_LOG(LogTemp, Log, TEXT("FacilityManager: Refreshing NPC states..."));

	for (const auto& Pair : FacilityNPCs)
	{
		const FGameplayTag& FacilityTag = Pair.Key;
		bool bFacilityEnabled = IsFacilityEnabled(FacilityTag);

		UE_LOG(LogTemp, Log, TEXT("  - Facility '%s' is %s"), 
			*FacilityTag.ToString(), 
			bFacilityEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));

		for (const TWeakObjectPtr<AActor>& NPCPtr : Pair.Value)
		{
			if (AActor* NPC = NPCPtr.Get())
			{
				NPC->SetActorHiddenInGame(!bFacilityEnabled);
				NPC->SetActorEnableCollision(bFacilityEnabled);
				NPC->SetActorTickEnabled(bFacilityEnabled);

				UE_LOG(LogTemp, Log, TEXT("    - NPC '%s': Hidden=%s, Collision=%s, Tick=%s"), 
					*NPC->GetName(),
					!bFacilityEnabled ? TEXT("YES") : TEXT("NO"),
					bFacilityEnabled ? TEXT("ON") : TEXT("OFF"),
					bFacilityEnabled ? TEXT("ON") : TEXT("OFF"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("    - NPC pointer is invalid (actor destroyed?)"));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("FacilityManager: Refresh complete."));
}

void UFacilityManagerComponent::EnableFacility(FGameplayTag FacilityTag)
{
	FFacilityEntry* Entry = nullptr;
	if (!GetFacility(FacilityTag, Entry))
	{
		UE_LOG(LogTemp, Warning, TEXT("FacilityManager: Facility %s not found"),
			*FacilityTag.ToString());
		return;
	}

	Entry->bUnlocked = true;

	if (Entry->FacilityActor.IsValid())
	{
		AActor* Actor = Entry->FacilityActor.Get();
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(true);
	}
	
	RefreshNPCStates(); // Update NPCs when facility state changes
}

void UFacilityManagerComponent::DisableFacility(FGameplayTag FacilityTag)
{
	FFacilityEntry* Entry = nullptr;
	if (!GetFacility(FacilityTag, Entry))
		return;

	Entry->bUnlocked = false;

	if (Entry->FacilityActor.IsValid())
	{
		AActor* Actor = Entry->FacilityActor.Get();
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
	}
}

FGameplayTagContainer UFacilityManagerComponent::GetActorGameplayTags(AActor* Actor)
{
	FGameplayTagContainer Tags;

	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FacilityManager: GetActorGameplayTags called with null actor"));
		return Tags;
	}

	// Check if implements IGameplayTagAssetInterface
	if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
	{
		TagInterface->GetOwnedGameplayTags(Tags);
		UE_LOG(LogTemp, Verbose, TEXT("  - Actor '%s' has interface tags: %s"), 
			*Actor->GetName(), *Tags.ToStringSimple());
		return Tags;
	}

	// Check for FGameplayTagContainer properties via reflection
	for (TFieldIterator<FProperty> PropIt(Actor->GetClass()); PropIt; ++PropIt)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
		{
			if (StructProp->Struct == FGameplayTagContainer::StaticStruct())
			{
				const FGameplayTagContainer* Container = StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(Actor);
				if (Container && Container->Num() > 0)
				{
					Tags.AppendTags(*Container);
					UE_LOG(LogTemp, Verbose, TEXT("  - Actor '%s' has property '%s' with tags: %s"), 
						*Actor->GetName(), *StructProp->GetName(), *Container->ToStringSimple());
				}
			}
		}
	}

	// Support built-in Actor Tags array (works for pure Blueprint actors)
	int32 ActorTagsFound = 0;
	for (const FName& Tag : Actor->Tags)
	{
		FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(Tag, false);
		if (GameplayTag.IsValid())
		{
			Tags.AddTag(GameplayTag);
			ActorTagsFound++;
		}
	}

	if (ActorTagsFound > 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("  - Actor '%s' has %d valid Actor Tags converted to GameplayTags"), 
			*Actor->GetName(), ActorTagsFound);
	}

	return Tags;
}

