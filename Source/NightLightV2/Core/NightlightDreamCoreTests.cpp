#include "NightlightDreamCore.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Defenders/NightlightDefender.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

// Checks the two default Core tiers: health, defender slots, cost and the stop after the last tier
// (Epic Games, Inc., 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightDreamCoreUpgradeTest,
	"Nightlight.Core.Upgrades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightDreamCoreUpgradeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Play never begins in this world, so the Core keeps its class defaults and defenders are
	// registered by hand.
	UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>();
	ANightlightDreamCore* const Core = World->SpawnActor<ANightlightDreamCore>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("The test world creates the registry"), Registry)
		|| !TestNotNull(TEXT("Core spawns"), Core))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("The Core has two upgrade tiers"), Core->GetMaxUpgradeLevel(), 2);
	TestEqual(TEXT("The Core starts at level 0"), Core->GetUpgradeLevel(), 0);
	TestEqual(TEXT("Six defenders are allowed before any upgrade"), Core->GetMaxDefenders(), 6);
	TestEqual(TEXT("The first tier costs 150"), Core->GetNextUpgradeCost(), 150);

	// Fill every starting slot, so placing another defender must wait for an upgrade.
	for (int32 DefenderIndex = 0; DefenderIndex < 6; ++DefenderIndex)
	{
		ANightlightDefender* const Defender = World->SpawnActor<ANightlightDefender>(
			FVector(DefenderIndex * 200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		Registry->RegisterDefender(Defender);
	}
	TestEqual(TEXT("All six defenders are counted"), Core->GetDefenderCount(), 6);
	TestFalse(TEXT("A seventh defender is blocked at level 0"), Core->CanPlaceDefender());

	Core->ApplyCoreDamage(20.0f);
	TestEqual(TEXT("Health percent follows the damage"), Core->GetHealthPercent(), 0.8f);
	TestTrue(TEXT("The first upgrade is applied"), Core->UpgradeCore());
	TestEqual(TEXT("Level 1 after the first upgrade"), Core->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Max health rises by 50"), Core->GetMaxHealth(), 150.0f);
	TestEqual(TEXT("The upgrade repairs the Core to the new max"), Core->GetCurrentHealth(), 150.0f);
	TestEqual(TEXT("Three more defenders are allowed"), Core->GetMaxDefenders(), 9);
	TestTrue(TEXT("A seventh defender can now be placed"), Core->CanPlaceDefender());
	TestEqual(TEXT("The second tier costs 300"), Core->GetNextUpgradeCost(), 300);

	TestTrue(TEXT("The second upgrade is applied"), Core->UpgradeCore());
	TestEqual(TEXT("Max health rises by 75"), Core->GetMaxHealth(), 225.0f);
	TestEqual(TEXT("The second upgrade also repairs to full"), Core->GetCurrentHealth(), 225.0f);
	TestEqual(TEXT("Twelve defenders are allowed at the last tier"), Core->GetMaxDefenders(), 12);

	FNightlightUpgradeTier NextTier;
	TestFalse(TEXT("A fully upgraded Core has no next tier"), Core->GetNextUpgradeTier(NextTier));
	TestEqual(TEXT("A fully upgraded Core has no cost"), Core->GetNextUpgradeCost(), -1);
	TestFalse(TEXT("A third upgrade is refused"), Core->UpgradeCore());
	TestEqual(TEXT("The refused upgrade changes nothing"), Core->GetMaxHealth(), 225.0f);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif

/*
References

Epic Games, Inc., 2026. Automation Test Framework in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine>
[Accessed 2 November 2026].
*/
