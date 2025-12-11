#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "ACFAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ACFBaseAttributeSet.h"
#include "PangeaAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class ATTRIBUTESYSTEM_API UPangeaAttributeSet : public UACFBaseAttributeSet
{

	GENERATED_BODY()
	
public:
	UPangeaAttributeSet();

	/* SECONDARY ATT */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Footholding;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Footholding);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData BlockDefense;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, BlockDefense);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Bravery;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Bravery);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Breeding;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Breeding);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Charm;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Charm);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Crafting;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Crafting);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData CritChance;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, CritChance);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Defense);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Dodge;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Dodge);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Intimidation;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Intimidation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData KnockOutChance;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, KnockOutChance);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, MovementSpeed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData ParryChance;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, ParryChance);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Precision;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Precision);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData StaggerChance;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, StaggerChance);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Stealth;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Stealth);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Taming;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Taming);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Taunting;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Taunting);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Vision;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Vision);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Secondary Attributes")
	FGameplayAttributeData Weight;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Weight);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Primary Attributes")
	FGameplayAttributeData Agility;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Agility);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Primary Attributes")
	FGameplayAttributeData Instinct;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Instinct);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Primary Attributes")
	FGameplayAttributeData Persuasion;
	ATTRIBUTE_ACCESSORS(UPangeaAttributeSet, Persuasion);

	

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
