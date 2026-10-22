#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ARSLevelingComponent.h"
#include "ACF/PangeaACFIntegrationLibrary.h"
#include "Curves/CurveFloat.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPangeaFixedDeathRewardTest, "Pangea.ACF.Player.OptionalDeathRewardCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPangeaFixedDeathRewardTest::RunTest(const FString& Parameters)
{
	UARSLevelingComponent* Leveling = NewObject<UARSLevelingComponent>();
	Leveling->SetExpToGiveOnDeath(30.f);
	Leveling->SetLevelingType(ELevelingType::EAssignPerksManually);
	TestTrue(TEXT("Pangea supplies a fixed reward curve without changing ACF"), UPangeaACFIntegrationLibrary::EnsureFixedDeathRewardCurve(Leveling));
	TestEqual(TEXT("Leveling remains enabled with a constant reward curve"), Leveling->GetExpOnDeath(), 30);
	TestEqual(TEXT("Fixed reward value remains unchanged"), Leveling->GetExpToGiveOnDeath(), 30.f);
	if (!TestNotNull(TEXT("Pangea runtime curve exists"), Leveling->GetExpToGiveOnDeathByCurrentLevel())) return false;
	TestEqual(TEXT("Fixed curve stays constant at high levels"), Leveling->GetExpToGiveOnDeathByCurrentLevel()->GetFloatValue(100.f), 30.f);
	TestTrue(TEXT("Fixed reward does not disable gaining levels"), Leveling->CanLevelUp());
	UCurveFloat* Curve = NewObject<UCurveFloat>();
	Curve->FloatCurve.AddKey(1.f, 75.f);
	Leveling->SetExpToGiveOnDeathByCurrentLevel(Curve);
	UPangeaACFIntegrationLibrary::EnsureFixedDeathRewardCurve(Leveling);
	TestTrue(TEXT("Pangea does not replace an authored override"), Leveling->GetExpToGiveOnDeathByCurrentLevel() == Curve);
	TestEqual(TEXT("An authored override still wins for levelable characters"), Leveling->GetExpOnDeath(), 75);
	Leveling->SetLevelingType(ELevelingType::ECantLevelUp);
	TestEqual(TEXT("Non-leveling actors retain their fixed reward"), Leveling->GetExpOnDeath(), 30);
	return !HasAnyErrors();
}
#endif
