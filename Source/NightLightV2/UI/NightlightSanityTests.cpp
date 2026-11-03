#include "NightlightSanityLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

// Checks the 60% and 30% mood thresholds the crayon HUD uses (Epic Games, Inc., 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightSanityStateTest,
	"Nightlight.UI.SanityState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightSanityStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("Full sanity is Brave"), UNightlightSanityLibrary::GetSanityState(1.0f), ENightlightSanityState::Brave);
	TestEqual(TEXT("Exactly 60% is still Brave"), UNightlightSanityLibrary::GetSanityState(0.6f), ENightlightSanityState::Brave);
	TestEqual(TEXT("Just below 60% is Scared"), UNightlightSanityLibrary::GetSanityState(0.59f), ENightlightSanityState::Scared);
	TestEqual(TEXT("Exactly 30% is still Scared"), UNightlightSanityLibrary::GetSanityState(0.3f), ENightlightSanityState::Scared);
	TestEqual(TEXT("Just below 30% is Losing It"), UNightlightSanityLibrary::GetSanityState(0.29f), ENightlightSanityState::LosingIt);
	TestEqual(TEXT("Zero sanity is Losing It"), UNightlightSanityLibrary::GetSanityState(0.0f), ENightlightSanityState::LosingIt);
	TestEqual(
		TEXT("Custom thresholds are used"),
		UNightlightSanityLibrary::GetSanityState(0.7f, 0.8f, 0.5f),
		ENightlightSanityState::Scared);

	TestEqual(TEXT("Brave ink stage"), UNightlightSanityLibrary::GetInkStage(0.9f), 0);
	TestEqual(TEXT("Scared ink stage"), UNightlightSanityLibrary::GetInkStage(0.5f), 1);
	TestEqual(TEXT("Losing ink stage"), UNightlightSanityLibrary::GetInkStage(0.2f), 2);
	TestEqual(TEXT("Overrun ink stage"), UNightlightSanityLibrary::GetInkStage(0.1f), 3);

	TestEqual(
		TEXT("Corrupted labels swap letters for numbers"),
		UNightlightSanityLibrary::CorruptText(FText::FromString(TEXT("Shooter"))).ToString(),
		FString(TEXT("Sh00t3r")));
	return true;
}

#endif

/*
References

Epic Games, Inc., 2026. Automation Test Framework in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine>
[Accessed 3 November 2026].
*/
