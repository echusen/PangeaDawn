#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MiningPresentationAgentInterface.generated.h"

/** Job a cosmetic mining site NPC performs. */
UENUM(BlueprintType)
enum class EMiningPresentationRole : uint8
{
	/** Mines at the site. */
	Worker,
	/** Guards the site. */
	Guard,
	/** Carries shipments away from the site. */
	Courier,
};

/** Cosmetic activity a mining site NPC is currently showing. */
UENUM(BlueprintType)
enum class EMiningPresentationState : uint8
{
	/** Not performing any duty. */
	Idle,
	/** Moving to a destination. */
	Traveling,
	/** Worker occupied at its primary station. */
	Working,
	/** Worker occupied at its secondary station. */
	Depositing,
	/** Standing guard. */
	Guarding,
	/** Courier occupied at its primary (pick-up) stop. */
	Loading,
	/** Courier occupied at its secondary (drop-off) stop. */
	Unloading,
	/** Default active state of the courier role. */
	Hauling,
	/** Worker retrieval phase of an ACF routine job. Append only: preserve serialized values of existing states. */
	Retrieving,
};

/** UObject side of IMiningPresentationAgentInterface, required by the reflection system. */
UINTERFACE(BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningPresentationAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by an NPC actor, or one of its components, that shows mining presentation state.
 * Lets the mining runtime drive role, state, focus and speed without depending on the presentation module.
 */
class GF_PANGEAMININGSYSTEMRUNTIME_API IMiningPresentationAgentInterface
{
	GENERATED_BODY()

public:
	/** Sets the travel speed the agent uses for the given role. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Presentation")
	void SetMiningTravelSpeed(EMiningPresentationRole Role, float Speed);

	/** Duration of one cosmetic clip cycle; zero means use the duty's configured fallback. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Presentation")
	float GetMiningPresentationDuration(EMiningPresentationState State);

	/** Tells the agent which role it is presenting. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Presentation")
	void SetMiningPresentationRole(EMiningPresentationRole Role);

	/** Tells the agent which activity to present. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Presentation")
	void SetMiningPresentationState(EMiningPresentationState State);

	/** Gives the agent the world location its current activity is directed at. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Presentation")
	void SetMiningPresentationFocus(const FVector& WorldLocation);
};
