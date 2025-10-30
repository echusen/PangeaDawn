// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

#pragma once


#include "CoreMinimal.h"
#include <GameplayTagContainer.h>
#include "Animation/AnimSequence.h"
#include "ACFCoreTypes.h"
#include "ACFCCTypes.h"
#include "ACFAnimTypes.generated.h"

/**
 * 
 */
 USTRUCT(BlueprintType)
struct FBaseLayer {

    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,  meta = (Categories = "Moveset"), Category = ACF)
    FGameplayTag TagName;

    FORCEINLINE bool operator!=(const FGameplayTag& Other) const
    {
        return TagName != Other;
    }

    FORCEINLINE bool operator==(const FGameplayTag& Other) const
    {
        return TagName == Other;
    }

    FORCEINLINE bool operator!=(const FACFStruct& Other) const
    {
        return TagName != Other.TagName;
    }

    FORCEINLINE bool operator==(const FACFStruct& Other) const
    {
        return TagName == Other.TagName;
    }
};


USTRUCT(BlueprintType)
struct FMoveset : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFMovesetLayer> Moveset;
};


USTRUCT(BlueprintType)
struct FOverlayLayer : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFOverlayLayer> Overlay;
};


USTRUCT(BlueprintType)
struct FRiderLayer : public FBaseLayer {

	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	TSubclassOf<class UACFRiderLayer> RiderLayer;
};




