#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MiningPresentationAgentInterface.generated.h"

UENUM(BlueprintType)
enum class EMiningPresentationRole : uint8
{
	Worker,
	Guard,
	Courier,
};

UENUM(BlueprintType)
enum class EMiningPresentationState : uint8
{
	Idle,
	Traveling,
	Working,
	Depositing,
	Guarding,
	Loading,
	Unloading,
	Hauling,
	Retrieving, // Append only: preserve serialized values of existing states.
};

UINTERFACE(BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningPresentationAgentInterface : public UInterface
{
	GENERATED_BODY()
};

class GF_PANGEAMININGSYSTEMRUNTIME_API IMiningPresentationAgentInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Presentation")
	void SetMiningTravelSpeed(EMiningPresentationRole Role, float Speed);

	/** Duration of one cosmetic clip cycle; zero means use the duty's configured fallback. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Presentation")
	float GetMiningPresentationDuration(EMiningPresentationState State);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Presentation")
	void SetMiningPresentationRole(EMiningPresentationRole Role);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Presentation")
	void SetMiningPresentationState(EMiningPresentationState State);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Presentation")
	void SetMiningPresentationFocus(const FVector& WorldLocation);
};
