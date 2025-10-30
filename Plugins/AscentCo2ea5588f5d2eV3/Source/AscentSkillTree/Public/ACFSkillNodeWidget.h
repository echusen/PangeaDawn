// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "ANSNavWidget.h"
#include "CoreMinimal.h"


#include "ACFSkillNodeWidget.generated.h"


class UACFBaseSkillNode;

/**
 *
 */
UCLASS()
class ASCENTSKILLTREE_API UACFSkillNodeWidget : public UANSNavWidget {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, Category = ACF)
    void SetupWithNode(UACFBaseSkillNode* skillNode);
};
