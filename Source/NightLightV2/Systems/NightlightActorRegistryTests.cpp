#include "NightlightActorRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Defenders/NightlightDefender.h"
#include "../Enemies/NightlightEnemy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

// A simple automation test runs once in the editor and reports each check separately
// (Epic Games, Inc., 2026b).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightActorRegistryTest,
	"Nightlight.Systems.ActorRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightActorRegistryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// A separate game world gets its own registry. Play never begins in it, so the actors do not
	// register themselves and the test controls every entry.
	UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>();
	if (!TestNotNull(TEXT("The test world creates the registry"), Registry))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	// Actors are placed along one axis so the distances are easy to read (Epic Games, Inc., 2026a).
	ANightlightEnemy* const NearEnemy = World->SpawnActor<ANightlightEnemy>(FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ANightlightEnemy* const FarEnemy = World->SpawnActor<ANightlightEnemy>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ANightlightDefender* const Defender = World->SpawnActor<ANightlightDefender>(FVector(0.0f, 200.0f, 0.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Near enemy spawns"), NearEnemy)
		|| !TestNotNull(TEXT("Far enemy spawns"), FarEnemy)
		|| !TestNotNull(TEXT("Defender spawns"), Defender))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	Registry->RegisterEnemy(NearEnemy);
	Registry->RegisterEnemy(FarEnemy);
	Registry->RegisterEnemy(NearEnemy);
	Registry->RegisterDefender(Defender);

	TestEqual(TEXT("A second registration of the same enemy is ignored"), Registry->GetLivingEnemyCount(), 2);
	TestEqual(TEXT("GetEnemies returns both enemies"), Registry->GetEnemies().Num(), 2);
	TestEqual(TEXT("GetDefenders returns the defender"), Registry->GetDefenders().Num(), 1);

	TestTrue(
		TEXT("The closest enemy in range is found"),
		Registry->FindClosestEnemy(FVector::ZeroVector, 500.0f) == NearEnemy);
	TestTrue(
		TEXT("An enemy exactly at the range edge counts as in range"),
		Registry->FindClosestEnemy(FVector::ZeroVector, 100.0f) == NearEnemy);
	TestNull(TEXT("No enemy is found when all are out of range"), Registry->FindClosestEnemy(FVector::ZeroVector, 50.0f));
	TestNull(TEXT("A negative range finds nothing"), Registry->FindClosestEnemy(FVector::ZeroVector, -10.0f));

	TestTrue(
		TEXT("The defender in range is found"),
		Registry->FindClosestDefender(FVector::ZeroVector, 250.0f) == Defender);
	TestNull(TEXT("No defender is found out of range"), Registry->FindClosestDefender(FVector::ZeroVector, 150.0f));

	// A lethal hit kills the near enemy, which removes itself from the registry at once.
	NearEnemy->ApplyDamage(NearEnemy->GetMaxHealth() + 1.0f);
	TestEqual(TEXT("A dead enemy is not counted as living"), Registry->GetLivingEnemyCount(), 1);
	TestEqual(TEXT("GetEnemies skips a dead enemy"), Registry->GetEnemies().Num(), 1);
	TestTrue(
		TEXT("The closest search skips a dead enemy"),
		Registry->FindClosestEnemy(FVector::ZeroVector, 500.0f) == FarEnemy);

	// A dead defender stays in the list because EndPlay never runs in this world, so the lookups
	// must skip it themselves.
	Defender->ApplyDamage(Defender->GetMaxHealth() + 1.0f);
	TestNull(TEXT("The closest search skips a dead defender"), Registry->FindClosestDefender(FVector::ZeroVector, 250.0f));
	TestEqual(TEXT("GetDefenders skips a dead defender"), Registry->GetDefenders().Num(), 0);

	Registry->UnregisterEnemy(FarEnemy);
	TestEqual(TEXT("An unregistered enemy is no longer counted"), Registry->GetLivingEnemyCount(), 0);
	TestNull(TEXT("An unregistered enemy is no longer found"), Registry->FindClosestEnemy(FVector::ZeroVector, 500.0f));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif

/*
References

Epic Games, Inc., 2026a. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 29 September 2026].

Epic Games, Inc., 2026b. Write C++ Tests in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine>
[Accessed 29 September 2026].
*/
