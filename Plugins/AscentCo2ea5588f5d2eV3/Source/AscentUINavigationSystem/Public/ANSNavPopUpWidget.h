// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ANSNavPageWidget.h"
#include "CoreMinimal.h"

#include "ANSNavPopUpWidget.generated.h"


/**
 *
 */
UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSNavPopUpWidget : public UANSNavPageWidget {
    GENERATED_BODY()

protected:
    /*To Be used to deactivate focus on main page when this pop up is spawned*/
    UPROPERTY(BlueprintReadWrite, Category = ANS, Meta = (ExposeOnSpawn = true))
    UANSNavPageWidget* pageOwner;

    UFUNCTION(BlueprintCallable, Category = ANS)
    void ResetFocusOnOwner();

    virtual void NativeConstruct() override;
};
