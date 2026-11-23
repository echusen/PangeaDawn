// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ACFFirstPersonSettings.generated.h"

class UCurveVector;

USTRUCT(BlueprintType)
struct FACFProceduralCurveMontageData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	float Delay = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	TObjectPtr<UCurveVector> CurveMontageRot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	TObjectPtr<UCurveVector> CurveMontageLoc;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector CurveMontageRotScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	FVector CurveMontageLocScale = FVector::OneVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Defaults)
	bool bIsCancellable = true;
};

UCLASS(Config = Plugins, DefaultConfig, meta = (DisplayName = "Ascent First Person Settings"))
class ASCENTFIRSTPERSON_API UACFFirstPersonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UACFFirstPersonSettings(const FObjectInitializer& ObjectInitializer);

	//~ Begin UDeveloperSettings interface
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End of UDeveloperSettings interface

	static UACFFirstPersonSettings* Get() { return CastChecked<UACFFirstPersonSettings>(StaticClass()->GetDefaultObject()); }

public:
	UPROPERTY(Config, EditAnywhere, Category="Additive")
	TMap<FGameplayTag, FACFProceduralCurveMontageData> CurveMontageData;

};
