#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/MiningPresentationAgentComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningStationAnimationDataTest, "Pangea.ACF.Mining.StationAnimationCopies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningStationAnimationDataTest::RunTest(const FString& Parameters)
{
	USkeleton* Target = LoadObject<USkeleton>(nullptr, TEXT("/AscentCombatFramework/Animation/SkeletalMeshes/ACF_UE5Manny.ACF_UE5Manny"));
	if (!TestNotNull(TEXT("Worker skeleton"), Target)) return false;
	for (const TCHAR* Name : {TEXT("AS_Pangea_MiningStrike"), TEXT("AS_Pangea_MinerRetrieve"), TEXT("AS_Pangea_MinerDeposit")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Yarin/%s.%s"), Name, Name);
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Path);
		if (!TestNotNull(Name, Sequence)) continue;
		TestTrue(TEXT("Copy is compatible with worker skeleton"), Target->IsCompatibleForEditor(Sequence->GetSkeleton()));
		TestTrue(TEXT("Copied clip has animation data"), Sequence->GetPlayLength() > .1f);
		TestFalse(TEXT("Station clip cannot move the character root"), Sequence->bEnableRootMotion);
		TestTrue(TEXT("Station clip locks the root"), Sequence->bForceRootLock);
		TestTrue(TEXT("No combat/collision/reward notifies are inherited"), Sequence->Notifies.IsEmpty());
		AddInfo(FString::Printf(TEXT("%s length=%.3fs rate=%.3f skeleton=%s notifies=%d"), Name,
			Sequence->GetPlayLength(), Sequence->RateScale, *GetNameSafe(Sequence->GetSkeleton()), Sequence->Notifies.Num()));
	}
	UMiningPresentationAgentComponent* Agent = NewObject<UMiningPresentationAgentComponent>();
	TestTrue(TEXT("Duty waits for the full mining strike"), FMath::IsNearlyEqual(Agent->GetMiningPresentationDuration_Implementation(EMiningPresentationState::Working), 3.125f, .01f));
	TestTrue(TEXT("Retrieval has its own positive duration"), Agent->GetMiningPresentationDuration_Implementation(EMiningPresentationState::Retrieving) > 1.8f);
	TestTrue(TEXT("Deposit has its own positive duration"), Agent->GetMiningPresentationDuration_Implementation(EMiningPresentationState::Depositing) > .7f);
	return true;
}
#endif
