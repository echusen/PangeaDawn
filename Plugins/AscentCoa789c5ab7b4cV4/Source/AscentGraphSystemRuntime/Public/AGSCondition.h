// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <GameFramework/PlayerController.h>

#include "AGSCondition.generated.h"

class UAGSGraphNode;

/**
 *
 */

UCLASS(Blueprintable, BlueprintType,/* abstract,*/ EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class AGSGRAPHRUNTIME_API UAGSCondition : public UObject {
    GENERATED_BODY()

public:
    bool Verify(class APlayerController* playerController);
    bool VerifyForNode(class APlayerController* playerController, UAGSGraphNode* inNodeOwner);

protected:
    UFUNCTION(BlueprintNativeEvent, Category = AGS)
    bool VerifyCondition(class APlayerController* playerController) const;
    virtual bool VerifyCondition_Implementation(class APlayerController* playerController) const { return true; }

    UPROPERTY()
    APlayerController* Controller;

    UPROPERTY()
    UAGSGraphNode* nodeOwner;

    UFUNCTION(BlueprintPure, Category = AGS)
    UAGSGraphNode* GetNodeOwner() const
    {
        return nodeOwner;
    }

    UWorld* GetWorld() const override { return Controller ? Controller->GetWorld() : nullptr; }
};

UCLASS(NotBlueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class AGSGRAPHRUNTIME_API UABGORCondition : public UAGSCondition {
    GENERATED_BODY()

protected:
    UABGORCondition() { }

    UPROPERTY(Instanced, EditDefaultsOnly, Category = "Conditions")
    TArray<UAGSCondition*> OrConditions;

    virtual bool VerifyCondition_Implementation(class APlayerController* controller) const override
    {
        for (auto& cond : OrConditions) {
            if (cond->Verify(controller))
                return true;
        }
        return false;
    }
};

UCLASS(NotBlueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class AGSGRAPHRUNTIME_API UABGANDCondition : public UAGSCondition {
    GENERATED_BODY()

public:
    UABGANDCondition() { }

    UPROPERTY(Instanced, EditDefaultsOnly, Category = "Conditions")
    TArray<UAGSCondition*> AndConditions;

    virtual bool VerifyCondition_Implementation(class APlayerController* controller) const override
    {
        for (auto& cond : AndConditions) {
            if (!cond->Verify(controller))
                return false;
        }
        return true;
    }
};
