// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <GameFramework/PlayerController.h>
#include "AGSAction.generated.h"

class UAGSGraphNode;

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, /*abstract, */EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class AGSGRAPHRUNTIME_API UAGSAction : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = AGS)
	void Execute(class APlayerController* playerController, UAGSGraphNode* nodeOwner);
	
protected:

	UFUNCTION(BlueprintNativeEvent, Category = AGS)
	void ExecuteAction( class APlayerController* playerController, UAGSGraphNode* nodeOwner);
	virtual void ExecuteAction_Implementation(class APlayerController* playerController, UAGSGraphNode* nodeOwner); 

	UPROPERTY()
	TObjectPtr<APlayerController> Controller;

	UWorld* GetWorld() const override ;


};

