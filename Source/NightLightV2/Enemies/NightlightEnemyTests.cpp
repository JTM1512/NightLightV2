#include "NightlightEnemy.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NightlightEnemyBrute.h"
#include "NightlightEnemyShade.h"
#include "../Core/NightlightDreamCore.h"
#include "../Defenders/NightlightDefender.h"
#include "../Systems/NightlightActorRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
	// Each test gets its own game world, like the actor registry test. Play never begins, so no actor
	// registers itself and no timer runs unless a test asks for it.
	UWorld* CreateTestWorld()
	{
		UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);
		return World;
	}

	void DestroyTestWorld(UWorld* const World)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	// The tuning values are protected so only Blueprints and subclasses change them. The tests set them
	// by name through reflection instead of opening them up to every class (Epic Games, Inc., 2026a).
	bool SetFloatProperty(UObject* const Object, const FName PropertyName, const float Value)
	{
		FFloatProperty* const Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
		if (!Property)
		{
			return false;
		}

		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
}

// Simple automation tests run once in the editor and report each check separately
// (Epic Games, Inc., 2026c).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightShadeCoreRangeTest,
	"Nightlight.Enemies.ShadeCoreRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightShadeCoreRangeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* const World = CreateTestWorld();
	ANightlightEnemyShade* const Shade = World->SpawnActor<ANightlightEnemyShade>(FVector::ZeroVector, FRotator::ZeroRotator);
	ANightlightDreamCore* const Core = World->SpawnActor<ANightlightDreamCore>(FVector(2000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Shade spawns"), Shade) || !TestNotNull(TEXT("Dream Core spawns"), Core))
	{
		DestroyTestWorld(World);
		return false;
	}

	// Defaults: CoreAttackRange 700, safety margin 100, Core AttackRange 1000.
	TestEqual(TEXT("Without a Core the Shade keeps its own range"), Shade->GetEffectiveCoreAttackRange(), 700.0f);

	Shade->AssignDreamCore(Core);
	TestEqual(TEXT("A range already inside the Core's range is kept"), Shade->GetEffectiveCoreAttackRange(), 700.0f);

	TestTrue(TEXT("CoreAttackRange can be set"), SetFloatProperty(Shade, TEXT("CoreAttackRange"), 1200.0f));
	TestEqual(TEXT("A range outside the Core's range is pulled inside by the margin"), Shade->GetEffectiveCoreAttackRange(), 900.0f);

	TestTrue(TEXT("The Core's AttackRange can be set"), SetFloatProperty(Core, TEXT("AttackRange"), 150.0f));
	TestEqual(TEXT("A Core range smaller than the margin falls back to half of it"), Shade->GetEffectiveCoreAttackRange(), 75.0f);

	SetFloatProperty(Core, TEXT("AttackRange"), 0.0f);
	TestEqual(TEXT("A Core with no range never gives a negative range"), Shade->GetEffectiveCoreAttackRange(), 0.0f);

	DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightBruteSlamTest,
	"Nightlight.Enemies.BruteSlam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightBruteSlamTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* const World = CreateTestWorld();
	UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>();

	// Defaults: DefenderAttackRange 300, SlamRadius 450, DefenderAttackDamage 20. The first defender
	// starts the slam, the second is only inside the slam radius and the third is outside it
	// (Epic Games, Inc., 2026b).
	ANightlightEnemyBrute* const Brute = World->SpawnActor<ANightlightEnemyBrute>(FVector::ZeroVector, FRotator::ZeroRotator);
	ANightlightDefender* const TargetDefender = World->SpawnActor<ANightlightDefender>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ANightlightDefender* const NearbyDefender = World->SpawnActor<ANightlightDefender>(FVector(0.0f, 400.0f, 0.0f), FRotator::ZeroRotator);
	ANightlightDefender* const DistantDefender = World->SpawnActor<ANightlightDefender>(FVector(600.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("The test world creates the registry"), Registry)
		|| !TestNotNull(TEXT("Brute spawns"), Brute)
		|| !TestNotNull(TEXT("Target defender spawns"), TargetDefender)
		|| !TestNotNull(TEXT("Nearby defender spawns"), NearbyDefender)
		|| !TestNotNull(TEXT("Distant defender spawns"), DistantDefender))
	{
		DestroyTestWorld(World);
		return false;
	}

	Registry->RegisterDefender(TargetDefender);
	Registry->RegisterDefender(NearbyDefender);
	Registry->RegisterDefender(DistantDefender);

	// Timers do not run in this world, so the wind-up is removed and the slam lands on the first attack.
	TestTrue(TEXT("SlamWindUpTime can be set"), SetFloatProperty(Brute, TEXT("SlamWindUpTime"), 0.0f));

	// One Tick finds the target through the registry and attacks straight away.
	Brute->Tick(0.1f);

	const float SlamDamage = 20.0f;
	TestEqual(TEXT("The defender the Brute targeted takes slam damage"),
		TargetDefender->GetMaxHealth() - TargetDefender->GetCurrentHealth(), SlamDamage);
	TestEqual(TEXT("A defender inside the slam radius but outside attack range also takes damage"),
		NearbyDefender->GetMaxHealth() - NearbyDefender->GetCurrentHealth(), SlamDamage);
	TestEqual(TEXT("A defender outside the slam radius is untouched"),
		DistantDefender->GetCurrentHealth(), DistantDefender->GetMaxHealth());
	TestFalse(TEXT("The Brute is not left winding up after the slam"), Brute->IsWindingUpSlam());

	DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightEnemyDeathTest,
	"Nightlight.Enemies.Death",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightEnemyDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* const World = CreateTestWorld();
	UNightlightActorRegistrySubsystem* const Registry = World->GetSubsystem<UNightlightActorRegistrySubsystem>();
	ANightlightEnemy* const Enemy = World->SpawnActor<ANightlightEnemy>(FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("The test world creates the registry"), Registry) || !TestNotNull(TEXT("Enemy spawns"), Enemy))
	{
		DestroyTestWorld(World);
		return false;
	}

	Registry->RegisterEnemy(Enemy);
	Enemy->ApplyDamage(Enemy->GetMaxHealth() * 0.5f);
	TestFalse(TEXT("A partial hit does not kill the enemy"), Enemy->IsDead());
	TestEqual(TEXT("A hurt enemy is still counted"), Registry->GetLivingEnemyCount(), 1);

	Enemy->ApplyDamage(Enemy->GetMaxHealth());
	TestTrue(TEXT("A lethal hit kills the enemy"), Enemy->IsDead());
	TestEqual(TEXT("Health never drops below zero"), Enemy->GetCurrentHealth(), 0.0f);

	// The enemy waits out DeathRemovalDelay for its death effect, but nothing may target it meanwhile.
	TestTrue(TEXT("A dying enemy stays in the level for its death effect"), IsValid(Enemy));
	TestFalse(TEXT("A dying enemy has no collision"), Enemy->GetActorEnableCollision());
	TestTrue(TEXT("A dying enemy is set to be removed after the delay"), Enemy->GetLifeSpan() > 0.0f);
	TestEqual(TEXT("A dying enemy leaves the registry at once"), Registry->GetLivingEnemyCount(), 0);
	TestNull(TEXT("A dying enemy is never found as a target"), Registry->FindClosestEnemy(FVector::ZeroVector, 500.0f));

	Enemy->ApplyDamage(10.0f);
	TestEqual(TEXT("Hits on a dying enemy are ignored"), Enemy->GetCurrentHealth(), 0.0f);

	DestroyTestWorld(World);
	return true;
}

#endif

/*
References

Epic Games, Inc., 2026a. Reflection System in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/reflection-system-in-unreal-engine>
[Accessed 30 September 2026].

Epic Games, Inc., 2026b. UWorld::SpawnActor. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UWorld/SpawnActor>
[Accessed 30 September 2026].

Epic Games, Inc., 2026c. Write C++ Tests in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine>
[Accessed 30 September 2026].
*/
