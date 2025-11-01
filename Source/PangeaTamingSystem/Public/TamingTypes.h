#pragma once

#include "GameplayTagContainer.h"
#include "TamingTypes.generated.h"


UENUM(BlueprintType)
enum class ETameState : uint8
{
	Wild UMETA(DisplayName="Wild"),
	Hostile UMETA(DisplayName="Hostile"),
	Tamed UMETA(DisplayName="Tamed")
};


UENUM(BlueprintType)
enum class ETamedRole : uint8
{
	None UMETA(DisplayName="None"),
	Mount UMETA(DisplayName="Mount"),
	Companion UMETA(DisplayName="Companion")
};


USTRUCT(BlueprintType)
struct FTameOutcome
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSuccess = false;
};