// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Actions/ACFActionAbility.h"
#include "CoreMinimal.h"

#include "ACFBoatBroadsideFireAbility.generated.h"

class UACFShootingComponent;

/**
 * Broadside cannon volley for water vehicles, usable by both AI boats and player-conquered ones.
 * On activation it collects the ACFShootingComponents on the avatar pawn and fires them as a
 * staggered volley: at the AI target when the pawn is driven by an AACFBoatAIController with a
 * valid target (only the cannons facing it), otherwise straight out along each cannon socket.
 */
UCLASS()
class VEHICLESYSTEM_API UACFBoatBroadsideFireAbility : public UACFActionAbility {
    GENERATED_BODY()

public:
    UACFBoatBroadsideFireAbility();

protected:
    virtual void OnActionStarted_Implementation() override;
    virtual void OnActionEnded_Implementation() override;

    /*Seconds between two shots of the same volley*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF, meta = (ClampMin = "0.05"))
    float VolleyInterval = 0.25f;

    /*World-units random offset added to the predicted target position per shot*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF, meta = (ClampMin = "0"))
    float RandomDeviation = 150.f;

    /*Loudness of the cannon-fire AI noise event (spreads the battle to ships in hearing range)*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF, meta = (ClampMin = "0"))
    float CannonNoiseLoudness = 2.f;

private:
    void FireNextShot();

    TArray<TWeakObjectPtr<UACFShootingComponent>> PendingComps;
    TWeakObjectPtr<AActor> VolleyTarget;
    int32 NextCompIndex = 0;
    FTimerHandle VolleyTimerHandle;
};
