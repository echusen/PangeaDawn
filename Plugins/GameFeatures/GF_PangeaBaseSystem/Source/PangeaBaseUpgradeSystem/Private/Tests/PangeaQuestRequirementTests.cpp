#include "Objects/Requirements/Req_QuestCompleted.h"
#include "AQSQuestManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaQuestRequirementTest, "Pangea.ACF.Base.QuestsUseOwningController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPangeaQuestRequirementTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Temporary test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    APawn* Pawn = World->SpawnActor<APawn>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Test pawn"), Pawn) || !TestNotNull(TEXT("Test controller"), Controller)) return false;
    UReq_QuestCompleted* Requirement = NewObject<UReq_QuestCompleted>();
    TestFalse(TEXT("A missing quest manager fails safely"), Requirement->IsRequirementMet(Pawn));
    UAQSQuestManagerComponent* Manager = NewObject<UAQSQuestManagerComponent>(Controller);
    Controller->AddInstanceComponent(Manager);
    Controller->Possess(Pawn);
    // An empty requirement succeeds only once the correct manager is resolved.
    TestTrue(TEXT("Quest manager is found on the interacting pawn's controller"), Requirement->IsRequirementMet(Pawn));
    return true;
}
#endif
