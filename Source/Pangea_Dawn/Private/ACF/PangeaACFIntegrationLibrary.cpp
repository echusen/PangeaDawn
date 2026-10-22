#include "ACF/PangeaACFIntegrationLibrary.h"
#include "ARSLevelingComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"

FVector UPangeaACFIntegrationLibrary::GetSafeRelativeDirection(const AActor* Source, const AActor* Target)
{
    return IsValid(Source) && IsValid(Target)
        ? (Source->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal()
        : FVector::ZeroVector;
}

bool UPangeaACFIntegrationLibrary::EnsureFixedDeathRewardCurve(UARSLevelingComponent* Leveling)
{
    if (!IsValid(Leveling)) return false;
    if (IsValid(Leveling->GetExpToGiveOnDeathByCurrentLevel())) return true;
    const float Reward = Leveling->GetExpToGiveOnDeath();
    if (!FMath::IsFinite(Reward)) return false;

    // ACF requires a curve for levelable actors. Honor that contract rather than
    // changing vendor GetExpOnDeath(). One key evaluates constantly at every level.
    UCurveFloat* Curve = NewObject<UCurveFloat>(Leveling, NAME_None, RF_Transient);
    Curve->FloatCurve.AddKey(0.f, Reward);
    Leveling->SetExpToGiveOnDeathByCurrentLevel(Curve);
    return true;
}
