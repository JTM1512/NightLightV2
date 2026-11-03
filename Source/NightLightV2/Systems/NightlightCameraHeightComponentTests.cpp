#include "NightlightCameraHeightComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

// A simple automation test runs once in the editor and reports each check separately
// (Epic Games, Inc., 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightCameraStartViewTest,
	"Nightlight.Systems.CameraStartView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightCameraStartViewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// A camera at the grid's corner looks diagonally across it to the Core in the middle.
	const FRotator FromCorner = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(0.0f, 0.0f, 1200.0f), FVector(3000.0f, 3000.0f, 300.0f), -45.0f);
	TestEqual(TEXT("A corner camera turns towards the middle of the map"), FromCorner.Yaw, 45.0, 0.01);
	TestEqual(TEXT("The set pitch is kept"), FromCorner.Pitch, -45.0, 0.01);
	TestEqual(TEXT("Roll is always zero"), FromCorner.Roll, 0.0, 0.01);

	// A camera past the far edge has to turn round to face back over the map.
	const FRotator FromBehind = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(-1000.0f, 3000.0f, 1200.0f), FVector(3000.0f, 3000.0f, 300.0f), -30.0f);
	TestEqual(TEXT("A camera on the -X side faces +X"), FromBehind.Yaw, 0.0, 0.01);

	const FRotator FromSide = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(3000.0f, 7000.0f, 1200.0f), FVector(3000.0f, 3000.0f, 300.0f), -30.0f);
	TestEqual(TEXT("A camera on the +Y side faces -Y"), FromSide.Yaw, -90.0, 0.01);

	// Height alone never changes the tilt, and the tilt never goes past straight down or above level.
	const FRotator TooSteep = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(0.0f, 0.0f, 5000.0f), FVector(3000.0f, 0.0f, 0.0f), -120.0f);
	TestEqual(TEXT("A pitch past straight down is clamped"), TooSteep.Pitch, -89.0, 0.01);

	const FRotator TooHigh = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(0.0f, 0.0f, 5000.0f), FVector(3000.0f, 0.0f, 0.0f), 20.0f);
	TestEqual(TEXT("A pitch above level is clamped"), TooHigh.Pitch, 0.0, 0.01);

	// Directly above the target there is no direction to face, so the yaw falls back to zero.
	const FRotator Above = UNightlightCameraHeightComponent::ComputeStartViewRotation(
		FVector(3000.0f, 3000.0f, 1200.0f), FVector(3000.0f, 3000.0f, 300.0f), -45.0f);
	TestEqual(TEXT("A camera straight above the target keeps a zero yaw"), Above.Yaw, 0.0, 0.01);

	return true;
}

#endif

/*
References

Epic Games, Inc., 2026. Automation Test Framework in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine>
[Accessed 3 November 2026].
*/
