// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "CoreMinimal.h"

#include "ACFGASTypes.generated.h"

/**
 *
 */
UENUM(BlueprintType)
enum class ELevelingType : uint8 {
    ECantLevelUp = 0 UMETA(DisplayName = "Do not use Leveling System"),
    EGenerateNewStatsFromCurves UMETA(DisplayName = "Generate Stats From Curves"),
    EAssignPerksManually UMETA(DisplayName = "Assign Perks Manually"),
};

USTRUCT(BlueprintType)
struct FAttributeSerializeKeys : public FTableRowBase {

    GENERATED_BODY()

public:
    FAttributeSerializeKeys() { };

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FGameplayAttribute Attribute;
};

USTRUCT(BlueprintType)
struct FAttributeSerializeNames {

    GENERATED_BODY()

public:
    FAttributeSerializeNames() { Value = 0.f; };

    FAttributeSerializeNames(const FName& inName, const float inValue)
    {

        AttributeName = inName;
        Value = inValue;
    };

    UPROPERTY(EditAnywhere, Savegame, BlueprintReadWrite, Category = "ACF")
    FName AttributeName;

    UPROPERTY(EditAnywhere, Savegame, BlueprintReadWrite, Category = "ACF")
    float Value;

    FORCEINLINE bool operator!=(const FName& Other) const
    {
        return this->AttributeName != Other;
    }

    FORCEINLINE bool operator==(const FName& Other) const
    {
        return this->AttributeName == Other;
    }
};

USTRUCT(BlueprintType)
struct FAttributeClamps {

    GENERATED_BODY()

public:
    FAttributeClamps() { };

    FAttributeClamps(const FGameplayAttribute& inToClamp,
        const FGameplayAttribute& inMaxValue, bool inClampZero)
    {
        AttributeToClamp = inToClamp;
        MaxValueAttribute = inMaxValue;
        bClampToZero = inClampZero;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FGameplayAttribute AttributeToClamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FGameplayAttribute MaxValueAttribute;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    bool bClampToZero = true;

    FORCEINLINE bool operator!=(const FGameplayAttribute& Other) const
    {
        return AttributeToClamp != Other;
    }

    FORCEINLINE bool operator==(const FGameplayAttribute& Other) const
    {
        return AttributeToClamp == Other;
    }
};

USTRUCT(BlueprintType)
struct FAttributeInit : public FTableRowBase {

    GENERATED_BODY()

public:
    FAttributeInit() { InitValue = 1.f; };

    FAttributeInit(const FGameplayAttribute& inAtt, const float inValue)
    {
        Attribute = inAtt;
        InitValue = inValue;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FGameplayAttribute Attribute;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    float InitValue;
};

USTRUCT(BlueprintType)
struct FACFAttributeInits : public FTableRowBase {

    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAttributeInit> PawnAttributesInit;
};

UCLASS()
class ASCENTGASRUNTIME_API UACFGASTypes : public UObject {
public:
    GENERATED_BODY()
    UACFGASTypes();
};
