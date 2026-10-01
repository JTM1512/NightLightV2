#include "NightlightWaveMath.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Enemies/NightlightEnemy.h"
#include "../Enemies/NightlightEnemyBrute.h"
#include "../Enemies/NightlightEnemyShade.h"
#include "Misc/AutomationTest.h"

namespace
{
	// Fresh settings with the planning document defaults. The C++ enemy classes stand in for the
	// Blueprint children, because a row without a class is never planned.
	UNightlightWaveSettings* CreateTestSettings()
	{
		UNightlightWaveSettings* const Settings = NewObject<UNightlightWaveSettings>();
		for (FNightlightWaveEnemyEntry& Entry : Settings->Enemies)
		{
			switch (Entry.Role)
			{
			case ENightlightWaveEnemyRole::Shade:
				Entry.EnemyClass = ANightlightEnemyShade::StaticClass();
				break;
			case ENightlightWaveEnemyRole::Brute:
				Entry.EnemyClass = ANightlightEnemyBrute::StaticClass();
				break;
			default:
				Entry.EnemyClass = ANightlightEnemy::StaticClass();
				break;
			}
		}
		return Settings;
	}

	int32 SumThreatCost(const UNightlightWaveSettings* const Settings, const TArray<int32>& PlannedEntries)
	{
		int32 Total = 0;
		for (const int32 EntryIndex : PlannedEntries)
		{
			Total += Settings->Enemies[EntryIndex].ThreatCost;
		}
		return Total;
	}

	bool ContainsRole(
		const UNightlightWaveSettings* const Settings,
		const TArray<int32>& PlannedEntries,
		const ENightlightWaveEnemyRole Role)
	{
		return PlannedEntries.ContainsByPredicate([Settings, Role](const int32 EntryIndex)
		{
			return Settings->Enemies[EntryIndex].Role == Role;
		});
	}
}

// A simple automation test runs once in the editor and reports each check separately
// (Epic Games, Inc., 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWaveBudgetTest,
	"Nightlight.Waves.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWaveBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();

	// Budgets from (6 + 3n) * A, with waves 5 and 10 being Brute waves at 25% more.
	TestEqual(TEXT("Wave 1 budget at A = 1"), UNightlightWaveMath::CalculateWaveBudget(Settings, 1, 1.0f), 9);
	TestEqual(TEXT("Wave 2 budget at A = 1"), UNightlightWaveMath::CalculateWaveBudget(Settings, 2, 1.0f), 12);
	TestEqual(TEXT("Wave 5 budget at A = 1 includes the Brute wave bonus"), UNightlightWaveMath::CalculateWaveBudget(Settings, 5, 1.0f), 26);
	TestEqual(TEXT("Wave 10 budget at A = 1 includes the Brute wave bonus"), UNightlightWaveMath::CalculateWaveBudget(Settings, 10, 1.0f), 45);
	TestEqual(TEXT("Wave 2 budget at the lowest A"), UNightlightWaveMath::CalculateWaveBudget(Settings, 2, 0.75f), 9);
	TestEqual(TEXT("Wave 2 budget at the highest A"), UNightlightWaveMath::CalculateWaveBudget(Settings, 2, 1.35f), 16);
	TestEqual(TEXT("A null settings asset uses the same defaults"), UNightlightWaveMath::CalculateWaveBudget(nullptr, 2, 1.0f), 12);

	TestTrue(TEXT("Wave 5 is a Brute wave"), UNightlightWaveMath::IsBruteWave(Settings, 5));
	TestFalse(TEXT("Wave 4 is not a Brute wave"), UNightlightWaveMath::IsBruteWave(Settings, 4));

	TestEqual(TEXT("Spawn gap at wave 1"), UNightlightWaveMath::CalculateSpawnGap(Settings, 1), 2.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Spawn gap at wave 10"), UNightlightWaveMath::CalculateSpawnGap(Settings, 10), 0.8f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Spawn gap stays flat after wave 10"), UNightlightWaveMath::CalculateSpawnGap(Settings, 15), 0.8f, KINDA_SMALL_NUMBER);

	const FNightlightEnemyMixWeights NeutralWeights;

	// Walkers cost 1 and unlock first, so every wave can spend its budget down to exactly zero.
	for (int32 WaveNumber = 1; WaveNumber <= 10; ++WaveNumber)
	{
		FRandomStream Stream(100 + WaveNumber);
		const int32 Budget = UNightlightWaveMath::CalculateWaveBudget(Settings, WaveNumber, 1.0f);
		const TArray<int32> Plan = UNightlightWaveMath::PlanWaveEnemies(Settings, WaveNumber, Budget, NeutralWeights, Stream);
		TestEqual(FString::Printf(TEXT("Wave %d spends exactly its budget"), WaveNumber), SumThreatCost(Settings, Plan), Budget);
	}

	// Waves 1 and 2 only have Walkers unlocked.
	for (int32 WaveNumber = 1; WaveNumber <= 2; ++WaveNumber)
	{
		FRandomStream Stream(7);
		const TArray<int32> Plan = UNightlightWaveMath::PlanWaveEnemies(
			Settings, WaveNumber, UNightlightWaveMath::CalculateWaveBudget(Settings, WaveNumber, 1.0f), NeutralWeights, Stream);
		TestFalse(FString::Printf(TEXT("Wave %d has no Shades"), WaveNumber), ContainsRole(Settings, Plan, ENightlightWaveEnemyRole::Shade));
		TestFalse(FString::Printf(TEXT("Wave %d has no Brutes"), WaveNumber), ContainsRole(Settings, Plan, ENightlightWaveEnemyRole::Brute));
	}

	// Wave 3 unlocks the Shade but not the Brute. A few seeds are tried so a Shade turns up at least once.
	bool bAnyWaveThreeShade = false;
	bool bAnyWaveThreeBrute = false;
	for (int32 Seed = 1; Seed <= 10; ++Seed)
	{
		FRandomStream Stream(Seed);
		const TArray<int32> Plan = UNightlightWaveMath::PlanWaveEnemies(
			Settings, 3, UNightlightWaveMath::CalculateWaveBudget(Settings, 3, 1.0f), NeutralWeights, Stream);
		bAnyWaveThreeShade |= ContainsRole(Settings, Plan, ENightlightWaveEnemyRole::Shade);
		bAnyWaveThreeBrute |= ContainsRole(Settings, Plan, ENightlightWaveEnemyRole::Brute);
	}
	TestTrue(TEXT("Wave 3 can contain Shades"), bAnyWaveThreeShade);
	TestFalse(TEXT("Wave 3 never contains Brutes"), bAnyWaveThreeBrute);

	// The same seed must give the same wave, so a wave can be replayed while balancing.
	FRandomStream FirstStream(1234);
	FRandomStream SecondStream(1234);
	const int32 WaveTenBudget = UNightlightWaveMath::CalculateWaveBudget(Settings, 10, 1.0f);
	const TArray<int32> FirstPlan = UNightlightWaveMath::PlanWaveEnemies(Settings, 10, WaveTenBudget, NeutralWeights, FirstStream);
	const TArray<int32> SecondPlan = UNightlightWaveMath::PlanWaveEnemies(Settings, 10, WaveTenBudget, NeutralWeights, SecondStream);
	TestTrue(TEXT("The same seed gives the same list twice"), FirstPlan == SecondPlan);

	// Rows without a class are never planned, so an unset asset spawns nothing instead of crashing.
	FRandomStream EmptyStream(1);
	TestEqual(
		TEXT("Rows without an enemy class are skipped"),
		UNightlightWaveMath::PlanWaveEnemies(nullptr, 1, 9, NeutralWeights, EmptyStream).Num(),
		0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWavePhasesTest,
	"Nightlight.Waves.Phases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWavePhasesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();
	const int32 WalkerIndex = Settings->Enemies.IndexOfByPredicate([](const FNightlightWaveEnemyEntry& Entry) { return Entry.Role == ENightlightWaveEnemyRole::Walker; });
	const int32 ShadeIndex = Settings->Enemies.IndexOfByPredicate([](const FNightlightWaveEnemyEntry& Entry) { return Entry.Role == ENightlightWaveEnemyRole::Shade; });
	const int32 BruteIndex = Settings->Enemies.IndexOfByPredicate([](const FNightlightWaveEnemyEntry& Entry) { return Entry.Role == ENightlightWaveEnemyRole::Brute; });

	// A 20-enemy plan: 12 Walkers, 6 Shades and 2 Brutes, mixed together like a real plan.
	TArray<int32> Plan;
	for (int32 Slot = 0; Slot < 20; ++Slot)
	{
		Plan.Add(Slot % 10 == 9 ? BruteIndex : (Slot % 3 == 1 ? ShadeIndex : WalkerIndex));
	}
	TestEqual(TEXT("The test plan has 2 Brutes"), Plan.FilterByPredicate([BruteIndex](const int32 Entry) { return Entry == BruteIndex; }).Num(), 2);
	TestEqual(TEXT("The test plan has 6 Shades"), Plan.FilterByPredicate([ShadeIndex](const int32 Entry) { return Entry == ShadeIndex; }).Num(), 6);

	FRandomStream Stream(42);
	const FNightlightWavePhaseLists Lists = UNightlightWaveMath::SplitIntoPhases(Settings, Plan, Stream);

	// 40% build-up, 45% peak and the rest relief.
	TestEqual(TEXT("Build-up holds 8 of 20"), Lists.BuildUp.Num(), 8);
	TestEqual(TEXT("Peak holds 9 of 20"), Lists.Peak.Num(), 9);
	TestEqual(TEXT("Relief holds 3 of 20"), Lists.Relief.Num(), 3);

	// Sorting both lists shows that every planned enemy appears exactly once.
	TArray<int32> Combined = Lists.BuildUp;
	Combined.Append(Lists.Peak);
	Combined.Append(Lists.Relief);
	Combined.Sort();
	TArray<int32> SortedPlan = Plan;
	SortedPlan.Sort();
	TestTrue(TEXT("Nothing is lost or duplicated"), Combined == SortedPlan);

	// The peak takes the most expensive enemies: its cheapest is never cheaper than anything outside it.
	int32 CheapestPeakCost = MAX_int32;
	for (const int32 EntryIndex : Lists.Peak)
	{
		CheapestPeakCost = FMath::Min(CheapestPeakCost, Settings->Enemies[EntryIndex].ThreatCost);
	}
	int32 DearestOffPeakCost = 0;
	for (const TArray<int32>* const OffPeak : { &Lists.BuildUp, &Lists.Relief })
	{
		for (const int32 EntryIndex : *OffPeak)
		{
			DearestOffPeakCost = FMath::Max(DearestOffPeakCost, Settings->Enemies[EntryIndex].ThreatCost);
		}
	}
	TestTrue(TEXT("The peak holds the most expensive enemies"), CheapestPeakCost >= DearestOffPeakCost);
	TestTrue(TEXT("Both Brutes are in the peak"), ContainsRole(Settings, Lists.Peak, ENightlightWaveEnemyRole::Brute));
	TestFalse(TEXT("Build-up has no Brute when there are Walkers to spare"), ContainsRole(Settings, Lists.BuildUp, ENightlightWaveEnemyRole::Brute));

	// The same seed gives the same split, so a wave can be replayed while balancing.
	FRandomStream RepeatStream(42);
	const FNightlightWavePhaseLists RepeatLists = UNightlightWaveMath::SplitIntoPhases(Settings, Plan, RepeatStream);
	TestTrue(TEXT("The same seed gives the same split"),
		RepeatLists.BuildUp == Lists.BuildUp && RepeatLists.Peak == Lists.Peak && RepeatLists.Relief == Lists.Relief);

	FRandomStream EmptyStream(1);
	const FNightlightWavePhaseLists EmptyLists = UNightlightWaveMath::SplitIntoPhases(Settings, TArray<int32>(), EmptyStream);
	TestTrue(TEXT("An empty plan gives three empty lists"),
		EmptyLists.BuildUp.IsEmpty() && EmptyLists.Peak.IsEmpty() && EmptyLists.Relief.IsEmpty());

	// Build-up and relief spawn 1.75 times slower than the peak.
	for (const int32 WaveNumber : { 1, 10 })
	{
		const float PeakGap = UNightlightWaveMath::GetPhaseSpawnGap(Settings, WaveNumber, ENightlightWavePhase::Peak);
		TestEqual(FString::Printf(TEXT("Wave %d peak gap is the base spawn gap"), WaveNumber), PeakGap, UNightlightWaveMath::CalculateSpawnGap(Settings, WaveNumber), KINDA_SMALL_NUMBER);
		TestEqual(FString::Printf(TEXT("Wave %d build-up gap is 1.75 times the peak gap"), WaveNumber), UNightlightWaveMath::GetPhaseSpawnGap(Settings, WaveNumber, ENightlightWavePhase::BuildUp), PeakGap * 1.75f, KINDA_SMALL_NUMBER);
		TestEqual(FString::Printf(TEXT("Wave %d relief gap is 1.75 times the peak gap"), WaveNumber), UNightlightWaveMath::GetPhaseSpawnGap(Settings, WaveNumber, ENightlightWavePhase::Relief), PeakGap * 1.75f, KINDA_SMALL_NUMBER);
	}

	return true;
}

#endif

/*
References

Epic Games, Inc., 2026. Write C++ Tests in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine>
[Accessed 1 October 2026].
*/
