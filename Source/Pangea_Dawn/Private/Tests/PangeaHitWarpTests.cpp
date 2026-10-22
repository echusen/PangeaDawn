#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ACF/PangeaAdvancedHitAction.h"
#include "ACF/PangeaACFIntegrationLibrary.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaSelfHitWarpTest, "Pangea.ACF.Player.SelfHitWarpFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaSelfHitWarpTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated geometry world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    ACharacter* Character = World->SpawnActor<ACharacter>();
    AActor* Attacker = World->SpawnActor<AActor>();
    if (!Character || !Attacker) return false;
    // AActor needs a root for SetActorLocation to affect geometry.
    USceneComponent* Root = NewObject<USceneComponent>(Attacker);
    Attacker->SetRootComponent(Root);
    Root->RegisterComponent();
    Character->SetActorLocationAndRotation(FVector(100, 200, 300), FRotator(0, 37, 0));
    const FVector SelfDirection = UPangeaACFIntegrationLibrary::GetSafeRelativeDirection(Character, Character);
    if (!TestFalse(TEXT("Self-caused damage does not normalize a zero vector into NaN"), SelfDirection.ContainsNaN())) return false;
    TestTrue(TEXT("Self-caused damage has no displacement direction"), SelfDirection.IsNearlyZero());
    Attacker->SetActorLocation(Character->GetActorLocation());
    TestTrue(TEXT("Overlapping actors have no displacement direction"), UPangeaACFIntegrationLibrary::GetSafeRelativeDirection(Character, Attacker).IsNearlyZero());
    TestTrue(TEXT("A missing attacker has no displacement direction"), UPangeaACFIntegrationLibrary::GetSafeRelativeDirection(Character, nullptr).IsNearlyZero());

    UPangeaAdvancedHitAction* Action = NewObject<UPangeaAdvancedHitAction>();
    Action->SetAnimMontage(NewObject<UAnimMontage>());
    const FObjectPropertyBase* Owner = FindFProperty<FObjectPropertyBase>(Action->GetClass(), TEXT("CharacterOwner"));
    const FStructProperty* Payload = FindFProperty<FStructProperty>(Action->GetClass(), TEXT("StoredPayload"));
    if (!Owner || !Payload) { AddError(TEXT("Native hit action fixture properties missing")); return false; }
    Owner->SetObjectPropertyValue_InContainer(Action, Character);
    FACFAbilityPayload* Hit = Payload->ContainerPtrToValuePtr<FACFAbilityPayload>(Action);
    UFunction* WarpFunction = Action->FindFunction(TEXT("GetWarpTransform"));
    if (!TestNotNull(TEXT("Hit warp event"), WarpFunction)) return false;
    const auto GetWarp = [Action, WarpFunction]()
    {
        struct { FTransform ReturnValue; } Params;
        Action->ProcessEvent(WarpFunction, &Params);
        return Params.ReturnValue;
    };
    for (AActor* Target : { static_cast<AActor*>(Character), Attacker, static_cast<AActor*>(nullptr) })
    {
        Hit->TargetActor = Target;
        const FTransform Warp = GetWarp();
        TestFalse(TEXT("Degenerate hit warp stays finite"), Warp.ContainsNaN());
        TestTrue(TEXT("Degenerate hit preserves location"), Warp.GetLocation().Equals(Character->GetActorLocation()));
        TestTrue(TEXT("Degenerate hit preserves facing rather than snapping to world zero"), Warp.GetRotation().Equals(Character->GetActorQuat(), 0.0001));
    }
    Attacker->SetActorLocation(Character->GetActorLocation() + FVector(500, 0, 0));
    Hit->TargetActor = Attacker;
    const FTransform Warp = GetWarp();
    TestTrue(TEXT("Ordinary hit retains configured 200cm displacement away from attacker"), Warp.GetLocation().Equals(Character->GetActorLocation() - FVector(200, 0, 0)));
    TestTrue(TEXT("Ordinary hit faces attacker"), Warp.GetRotation().Equals(FQuat::Identity, 0.0001));
    return !HasAnyErrors();
}
#endif
