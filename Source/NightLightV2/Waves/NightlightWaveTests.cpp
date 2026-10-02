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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWaveAdaptationTest,
	"Nightlight.Waves.Adaptation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWaveAdaptationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();
	const auto MakeMeasures = [](const float CoreHealth, const float Stopped, const float ClearTime, const float Tokens)
	{
		FNightlightWaveMeasures Measures;
		Measures.CoreHealthKept = CoreHealth;
		Measures.EnemiesStopped = Stopped;
		Measures.ClearTime = ClearTime;
		Measures.UnspentTokens = Tokens;
		return Measures;
	};

	// Above 0.7 raises A by 0.1, below 0.4 lowers it by 0.1 and anything between leaves it.
	const float PerfectScore = UNightlightWaveMath::CalculatePlayerScore(Settings, MakeMeasures(1.0f, 1.0f, 1.0f, 1.0f));
	const float PoorScore = UNightlightWaveMath::CalculatePlayerScore(Settings, MakeMeasures(0.0f, 0.2f, 0.0f, 0.1f));
	const float MiddleScore = UNightlightWaveMath::CalculatePlayerScore(Settings, MakeMeasures(0.5f, 0.5f, 0.5f, 0.5f));
	TestEqual(TEXT("Perfect measures score 1"), PerfectScore, 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Perfect measures raise A by 0.1"), UNightlightWaveMath::UpdateAdaptiveFactor(Settings, 1.0f, PerfectScore), 1.1f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Poor measures lower A by 0.1"), UNightlightWaveMath::UpdateAdaptiveFactor(Settings, 1.0f, PoorScore), 0.9f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("A middle score leaves A alone"), UNightlightWaveMath::UpdateAdaptiveFactor(Settings, 1.0f, MiddleScore), 1.0f, KINDA_SMALL_NUMBER);

	// Many good or bad waves in a row never push A past its limits.
	float HighA = 1.0f;
	float LowA = 1.0f;
	for (int32 Wave = 0; Wave < 30; ++Wave)
	{
		HighA = UNightlightWaveMath::UpdateAdaptiveFactor(Settings, HighA, PerfectScore);
		LowA = UNightlightWaveMath::UpdateAdaptiveFactor(Settings, LowA, PoorScore);
	}
	TestEqual(TEXT("A stops at 1.35 after many good waves"), HighA, 1.35f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("A stops at 0.75 after many bad waves"), LowA, 0.75f, KINDA_SMALL_NUMBER);

	// An unavailable measure is left out and the other weights share its place.
	TestEqual(TEXT("Unknown tokens are left out"), UNightlightWaveMath::CalculatePlayerScore(Settings, MakeMeasures(1.0f, 1.0f, 1.0f, -1.0f)), 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Two unknown measures leave an average of the other two"), UNightlightWaveMath::CalculatePlayerScore(Settings, MakeMeasures(1.0f, 0.0f, -1.0f, -1.0f)), 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Unknown tokens score -1"), UNightlightWaveMath::CalculateUnspentTokensScore(Settings, -1), -1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("50 of 100 tokens scores 0.5"), UNightlightWaveMath::CalculateUnspentTokensScore(Settings, 50), 0.5f, KINDA_SMALL_NUMBER);

	// 5 seconds of spawning plus 10 seconds of grace gives a 15 second target.
	TestEqual(TEXT("Clearing inside the grace scores 1"), UNightlightWaveMath::CalculateClearTimeScore(Settings, 12.0f, 5.0f), 1.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Clearing halfway to twice the target scores 0.5"), UNightlightWaveMath::CalculateClearTimeScore(Settings, 22.5f, 5.0f), 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Clearing at twice the target scores 0"), UNightlightWaveMath::CalculateClearTimeScore(Settings, 30.0f, 5.0f), 0.0f, KINDA_SMALL_NUMBER);

	// Wave 1 at 2 seconds a peak gap: 4 build-up enemies at 3.5 s, then 4 peak enemies at 2 s, less the last gap.
	FNightlightWavePhaseLists PhaseLists;
	PhaseLists.BuildUp = { 0, 0, 0, 0 };
	PhaseLists.Peak = { 0, 0, 0, 0 };
	TestEqual(TEXT("Planned spawning time follows the phase gaps"), UNightlightWaveMath::CalculatePlannedSpawnSeconds(Settings, 1, PhaseLists), 20.0f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWaveChallengeTest,
	"Nightlight.Waves.Challenge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWaveChallengeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();

	// Full pressure is a fifth of the Core lost, 2 defenders lost or 4 enemies near the Core.
	TestEqual(TEXT("Nothing happening gives 0"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.0f, 0, 0), 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("All three inputs at full gives 100"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.2f, 2, 4), 100.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Inputs above full stay at 100"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.9f, 7, 20), 100.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Half the Core damage alone gives about 17"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.1f, 0, 0), 100.0f / 6.0f, 0.01f);
	TestEqual(TEXT("Half the defenders lost alone gives about 17"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.0f, 1, 0), 100.0f / 6.0f, 0.01f);
	TestEqual(TEXT("Half the enemies near the Core alone gives about 17"), UNightlightWaveMath::CalculateChallengeLevel(Settings, 0.0f, 0, 2), 100.0f / 6.0f, 0.01f);
	TestEqual(TEXT("A null settings asset uses the same defaults"), UNightlightWaveMath::CalculateChallengeLevel(nullptr, 0.2f, 2, 4), 100.0f, KINDA_SMALL_NUMBER);

	// Moving the rest of build-up onto the front of the peak keeps both orders and every enemy exactly once.
	TArray<int32> BuildUp = { 0, 0, 1 };
	TArray<int32> Peak = { 2, 1 };
	TArray<int32> Expected = BuildUp;
	Expected.Append(Peak);
	UNightlightWaveMath::MovePhaseEnemiesToFront(BuildUp, Peak);
	TestTrue(TEXT("The moved enemies spawn first, in their old order"), Peak == Expected);
	TestTrue(TEXT("The phase they left is empty"), BuildUp.IsEmpty());

	TArray<int32> EmptyPhase;
	TArray<int32> Relief = { 1 };
	UNightlightWaveMath::MovePhaseEnemiesToFront(EmptyPhase, Relief);
	TestTrue(TEXT("Moving an empty phase changes nothing"), Relief == TArray<int32>({ 1 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWaveRouteWeightsTest,
	"Nightlight.Waves.RouteWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWaveRouteWeightsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();

	// Two parallel routes 2000 units apart, with route B twice as long. One defender (600 range, 20 damage per
	// second) stands 300 units beside route A.
	const TArray<FVector> RouteA = { FVector(0.0f, 0.0f, 0.0f), FVector(1000.0f, 0.0f, 0.0f), FVector(2000.0f, 0.0f, 0.0f) };
	const TArray<FVector> RouteB = { FVector(0.0f, 2000.0f, 0.0f), FVector(4000.0f, 2000.0f, 0.0f) };
	const TArray<FVector> Defenders = { FVector(500.0f, 300.0f, 0.0f) };
	const float DefenceA = UNightlightWaveMath::CalculateRouteDefence(RouteA, Defenders, { 600.0f }, { 20.0f });
	const float DefenceB = UNightlightWaveMath::CalculateRouteDefence(RouteB, Defenders, { 600.0f }, { 20.0f });
	TestEqual(TEXT("The defender beside route A counts once"), DefenceA, 20.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Route B is out of reach"), DefenceB, 0.0f, KINDA_SMALL_NUMBER);

	// With no randomness, 20 damage per second halves route A and the bare route B weighs exactly 1.
	Settings->RouteRandomFactor = 0.0f;
	FRandomStream Stream(3);
	TestEqual(TEXT("The defended route weighs 0.5"), UNightlightWaveMath::CalculateRouteWeight(Settings, DefenceA, Stream), 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("The undefended route weighs 1"), UNightlightWaveMath::CalculateRouteWeight(Settings, DefenceB, Stream), 1.0f, KINDA_SMALL_NUMBER);

	// Equal, undefended routes stay inside the plus or minus 20% band.
	Settings->RouteRandomFactor = 0.2f;
	bool bAllInBand = true;
	for (int32 Roll = 0; Roll < 200; ++Roll)
	{
		const float Weight = UNightlightWaveMath::CalculateRouteWeight(Settings, 0.0f, Stream);
		bAllInBand &= Weight >= 0.8f - KINDA_SMALL_NUMBER && Weight <= 1.2f + KINDA_SMALL_NUMBER;
	}
	TestTrue(TEXT("Equal routes stay within plus or minus 20%"), bAllInBand);

	// From equal weights, Brutes boost the shortest route, Shades the longest and Walkers neither.
	const TArray<float> Equal = { 1.0f, 1.0f };
	const TArray<float> Lengths = { UNightlightWaveMath::CalculateRouteLength(RouteA), UNightlightWaveMath::CalculateRouteLength(RouteB) };
	TestEqual(TEXT("Route lengths sum their segments"), Lengths[1], 4000.0f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Brutes favour the shortest route"), UNightlightWaveMath::ApplyRoleRoutePreference(Settings, ENightlightWaveEnemyRole::Brute, Equal, Lengths) == TArray<float>({ 1.5f, 1.0f }));
	TestTrue(TEXT("Shades favour the longest route"), UNightlightWaveMath::ApplyRoleRoutePreference(Settings, ENightlightWaveEnemyRole::Shade, Equal, Lengths) == TArray<float>({ 1.0f, 1.5f }));
	TestTrue(TEXT("Walkers leave the weights alone"), UNightlightWaveMath::ApplyRoleRoutePreference(Settings, ENightlightWaveEnemyRole::Walker, Equal, Lengths) == Equal);
	TestTrue(TEXT("One route changes nothing"), UNightlightWaveMath::ApplyRoleRoutePreference(Settings, ENightlightWaveEnemyRole::Brute, { 0.7f }, { 1000.0f }) == TArray<float>({ 0.7f }));

	// Zero weights are never picked, and with nothing to pick the result is -1.
	bool bPickedZero = false;
	for (int32 Pick = 0; Pick < 1000; ++Pick)
	{
		const int32 Picked = UNightlightWaveMath::PickWeightedIndex({ 0.0f, 1.0f, 0.0f, 2.0f, 0.0f }, Stream);
		bPickedZero |= Picked != 1 && Picked != 3;
	}
	TestFalse(TEXT("PickWeightedIndex never returns a zero-weight index"), bPickedZero);
	TestEqual(TEXT("All-zero weights return -1"), UNightlightWaveMath::PickWeightedIndex({ 0.0f, 0.0f }, Stream), static_cast<int32>(INDEX_NONE));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightWaveTemplatesTest,
	"Nightlight.Waves.Templates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightWaveTemplatesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateTestSettings();
	using EStyle = ENightlightDefenderStyle;
	const TArray<FVector> Close = { FVector::ZeroVector, FVector(200.0f, 0.0f, 0.0f), FVector(400.0f, 0.0f, 0.0f) };
	const TArray<FVector> Apart = { FVector::ZeroVector, FVector(2000.0f, 0.0f, 0.0f), FVector(4000.0f, 0.0f, 0.0f) };

	// How often each template is picked for wave 6 over 200 rolls, after a Siege wave 5.
	const auto CountPicks = [Settings](const FNightlightPlayStyle& PlayStyle, const FName Name)
	{
		FRandomStream Stream(11);
		int32 Picks = 0;
		for (int32 Roll = 0; Roll < 200; ++Roll)
		{
			Picks += UNightlightWaveMath::ChooseTemplate(Settings, 6, PlayStyle, TEXT("Siege"), Stream).Name == Name ? 1 : 0;
		}
		return Picks;
	};

	const FNightlightPlayStyle ShortRange = UNightlightWaveMath::AnalysePlayStyle(Settings, Apart, { EStyle::ShortRange, EStyle::ShortRange, EStyle::ShortRange });
	const FNightlightPlayStyle LongRange = UNightlightWaveMath::AnalysePlayStyle(Settings, Apart, { EStyle::LongRange, EStyle::LongRange, EStyle::LongRange });
	const FNightlightPlayStyle Packed = UNightlightWaveMath::AnalysePlayStyle(Settings, Close, { EStyle::Standard, EStyle::Standard, EStyle::Standard });
	const FNightlightPlayStyle Spread = UNightlightWaveMath::AnalysePlayStyle(Settings, Apart, { EStyle::Standard, EStyle::Standard, EStyle::Standard });
	TestTrue(TEXT("Three Pulse-style defenders are mostly short range"), ShortRange.bMostlyShortRange && !ShortRange.bMostlyLongRange);
	TestTrue(TEXT("Three Shooter-style defenders are mostly long range"), LongRange.bMostlyLongRange && !LongRange.bMostlyShortRange);
	TestTrue(TEXT("Three defenders 200 apart are packed"), Packed.bPackedTogether);
	TestEqual(TEXT("Their nearest neighbours average 200 apart"), Packed.AverageNearestDefenderDistance, 200.0f, KINDA_SMALL_NUMBER);
	TestFalse(TEXT("The same three 2000 apart are not packed"), Spread.bPackedTogether);
	TestTrue(TEXT("Short range favours Skirmish"), CountPicks(ShortRange, TEXT("Skirmish")) > CountPicks(ShortRange, TEXT("Swarm")));
	TestTrue(TEXT("Long range favours Swarm"), CountPicks(LongRange, TEXT("Swarm")) > CountPicks(LongRange, TEXT("Skirmish")));

	// Packed defenders favour Siege on a wave where it is allowed, here after a Swarm wave 7.
	FRandomStream Stream(5);
	int32 SiegePicks = 0;
	for (int32 Roll = 0; Roll < 200; ++Roll)
	{
		SiegePicks += UNightlightWaveMath::ChooseTemplate(Settings, 8, Packed, TEXT("Swarm"), Stream).Name == TEXT("Siege") ? 1 : 0;
	}
	TestTrue(TEXT("Packed defenders favour Siege"), SiegePicks > 100);

	// Brute waves are always Siege, and waves 1 and 2 only have Swarm unlocked.
	for (int32 Roll = 0; Roll < 20; ++Roll)
	{
		TestEqual(TEXT("Wave 5 is Siege"), UNightlightWaveMath::ChooseTemplate(Settings, 5, LongRange, TEXT("Siege"), Stream).Name, FName(TEXT("Siege")));
		TestEqual(TEXT("Wave 10 is Siege"), UNightlightWaveMath::ChooseTemplate(Settings, 10, ShortRange, TEXT("Swarm"), Stream).Name, FName(TEXT("Siege")));
		TestEqual(TEXT("Wave 1 is Swarm"), UNightlightWaveMath::ChooseTemplate(Settings, 1, Packed, NAME_None, Stream).Name, FName(TEXT("Swarm")));
		TestEqual(TEXT("Wave 2 is Swarm"), UNightlightWaveMath::ChooseTemplate(Settings, 2, Packed, TEXT("Swarm"), Stream).Name, FName(TEXT("Swarm")));
	}

	// From wave 6 on, no non-Brute wave repeats the template before it, even with packed defenders pulling to Siege.
	FName Previous = TEXT("Siege");
	bool bRepeated = false;
	for (int32 WaveNumber = 6, Checked = 0; Checked < 50; ++WaveNumber)
	{
		const FName Name = UNightlightWaveMath::ChooseTemplate(Settings, WaveNumber, Packed, Previous, Stream).Name;
		if (!UNightlightWaveMath::IsBruteWave(Settings, WaveNumber))
		{
			bRepeated |= Name == Previous;
			++Checked;
		}
		Previous = Name;
	}
	TestFalse(TEXT("No template runs twice in a row"), bRepeated);

	// The wave 6 budget of 24 planned with each template's weights over 50 seeds, counting the Shades.
	const auto CountShades = [Settings](const FName Name)
	{
		const FNightlightWaveTemplate* const Template = Settings->Templates.FindByPredicate(
			[Name](const FNightlightWaveTemplate& Entry) { return Entry.Name == Name; });
		int32 Shades = 0;
		for (int32 Seed = 1; Seed <= 50 && Template; ++Seed)
		{
			FRandomStream PlanStream(Seed);
			for (const int32 EntryIndex : UNightlightWaveMath::PlanWaveEnemies(Settings, 6, 24, Template->Weights, PlanStream))
			{
				Shades += Settings->Enemies[EntryIndex].Role == ENightlightWaveEnemyRole::Shade ? 1 : 0;
			}
		}
		return Shades;
	};
	TestTrue(TEXT("A Skirmish wave 6 has more Shades than a Swarm wave 6"), CountShades(TEXT("Skirmish")) > CountShades(TEXT("Swarm")));

	return true;
}

#endif

/*
References

Epic Games, Inc., 2026. Write C++ Tests in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine>
[Accessed 1 October 2026].
*/
