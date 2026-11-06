#include "NightlightEliteGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NightlightEnemy.h"
#include "NightlightEnemyBrute.h"
#include "NightlightEnemyShade.h"
#include "../Waves/NightlightWaveSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
	// Fresh settings with the planning document defaults and the C++ enemy classes standing in for the Blueprint
	// children, the same as the wave tests.
	UNightlightWaveSettings* CreateEliteTestSettings()
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

	int32 SumPlannedCost(const UNightlightWaveSettings* const Settings, const TArray<FNightlightPlannedEnemy>& Plan)
	{
		int32 Total = 0;
		for (const FNightlightPlannedEnemy& PlannedEnemy : Plan)
		{
			Total += UNightlightWaveMath::GetPlannedEnemyCost(Settings, PlannedEnemy);
		}
		return Total;
	}

	int32 CountElites(const TArray<FNightlightPlannedEnemy>& Plan)
	{
		return Plan.FilterByPredicate([](const FNightlightPlannedEnemy& PlannedEnemy) { return PlannedEnemy.bIsElite; }).Num();
	}

	// How many of Samples profiles, rolled at WaveNumber with this play style, have the mutation.
	int32 CountMutation(
		const UNightlightWaveSettings* const Settings,
		const int32 WaveNumber,
		const FNightlightPlayStyle& PlayStyle,
		const ENightlightEliteMutation Mutation,
		const int32 Samples)
	{
		int32 Count = 0;
		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const int32 Seed = UNightlightEliteGenerator::MakeEliteSeed(7, WaveNumber, Sample);
			Count += UNightlightEliteGenerator::GenerateEliteProfile(Settings, WaveNumber, Seed, PlayStyle, ENightlightWaveEnemyRole::Walker)
				.HasMutation(Mutation) ? 1 : 0;
		}
		return Count;
	}

	UWorld* CreateEliteTestWorld()
	{
		UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);
		return World;
	}

	void DestroyEliteTestWorld(UWorld* const World)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	// The tuning values are protected, so the test reads them by name through reflection (Epic Games, Inc., 2026a).
	float GetFloatProperty(const UObject* const Object, const FName PropertyName)
	{
		const FFloatProperty* const Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1.0f;
	}

	int32 GetIntProperty(const UObject* const Object, const FName PropertyName)
	{
		const FIntProperty* const Property = FindFProperty<FIntProperty>(Object->GetClass(), PropertyName);
		return Property ? Property->GetPropertyValue_InContainer(Object) : -1;
	}
}

// Simple automation tests run once in the editor and report each check separately (Epic Games, Inc., 2026b).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightEliteChanceTest,
	"Nightlight.Enemies.Elite.Chance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightEliteChanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UNightlightWaveSettings* const Settings = CreateEliteTestSettings();

	// min(0.05 + 0.02 * (n - 4), 0.30) from wave 4, and nothing before it.
	for (const int32 WaveNumber : { 1, 2, 3 })
	{
		TestEqual(FString::Printf(TEXT("Wave %d has no elite chance"), WaveNumber), UNightlightEliteGenerator::CalculateEliteChance(Settings, WaveNumber), 0.0f);
	}
	TestEqual(TEXT("Wave 4 starts at 5%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 4), 0.05f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Wave 5 is 7%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 5), 0.07f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Wave 10 is 17%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 10), 0.17f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Wave 16 is 29%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 16), 0.29f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Wave 17 is capped at 30%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 17), 0.3f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Wave 50 stays capped at 30%"), UNightlightEliteGenerator::CalculateEliteChance(Settings, 50), 0.3f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("A null settings asset uses the same defaults"), UNightlightEliteGenerator::CalculateEliteChance(nullptr, 4), 0.05f, KINDA_SMALL_NUMBER);

	// One mutation up to wave 9, two from wave 10.
	for (int32 WaveNumber = 4; WaveNumber <= 12; ++WaveNumber)
	{
		TestEqual(FString::Printf(TEXT("Wave %d mutation count"), WaveNumber), UNightlightEliteGenerator::GetMutationCount(Settings, WaveNumber), WaveNumber >= 10 ? 2 : 1);
	}

	// Rewards: base x (1 + 0.5 x mutations).
	FNightlightEliteProfile Profile;
	Profile.bIsElite = true;
	Profile.TokenMultiplier = 1.5f;
	TestEqual(TEXT("A one-mutation elite is worth 1.5 times the tokens"), UNightlightEliteGenerator::CalculateEliteTokens(20, Profile), 30);
	Profile.TokenMultiplier = 2.0f;
	TestEqual(TEXT("A two-mutation elite is worth twice the tokens"), UNightlightEliteGenerator::CalculateEliteTokens(40, Profile), 80);
	TestEqual(TEXT("A normal enemy keeps its tokens"), UNightlightEliteGenerator::CalculateEliteTokens(10, FNightlightEliteProfile()), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightEliteSeedTest,
	"Nightlight.Enemies.Elite.Seed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightEliteSeedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UNightlightWaveSettings* const Settings = CreateEliteTestSettings();
	FNightlightPlayStyle PlayStyle;
	PlayStyle.bPackedTogether = true;

	TestEqual(TEXT("The same map, wave and plan slot give the same seed"),
		UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 3), UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 3));
	TestNotEqual(TEXT("Another plan slot gives another seed"),
		UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 3), UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 4));
	TestNotEqual(TEXT("Another wave gives another seed"),
		UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 3), UNightlightEliteGenerator::MakeEliteSeed(1234, 7, 3));
	TestNotEqual(TEXT("Another map gives another seed"),
		UNightlightEliteGenerator::MakeEliteSeed(1234, 6, 3), UNightlightEliteGenerator::MakeEliteSeed(4321, 6, 3));

	// Every rolled value is compared, so the same seed has to give exactly the same elite.
	int32 DifferentProfiles = 0;
	for (int32 PlanIndex = 0; PlanIndex < 50; ++PlanIndex)
	{
		const int32 Seed = UNightlightEliteGenerator::MakeEliteSeed(1234, 12, PlanIndex);
		const FNightlightEliteProfile First = UNightlightEliteGenerator::GenerateEliteProfile(Settings, 12, Seed, PlayStyle, ENightlightWaveEnemyRole::Shade);
		const FNightlightEliteProfile Second = UNightlightEliteGenerator::GenerateEliteProfile(Settings, 12, Seed, PlayStyle, ENightlightWaveEnemyRole::Shade);
		const bool bSame = First.Mutations == Second.Mutations
			&& First.MutationStrengths == Second.MutationStrengths
			&& First.HealthMultiplier == Second.HealthMultiplier
			&& First.SpeedMultiplier == Second.SpeedMultiplier
			&& First.HurtAttackIntervalMultiplier == Second.HurtAttackIntervalMultiplier
			&& First.HealFraction == Second.HealFraction
			&& First.MeshScale == Second.MeshScale
			&& First.Tint == Second.Tint
			&& First.Glow == Second.Glow
			&& First.DisplayName.EqualTo(Second.DisplayName);
		TestTrue(FString::Printf(TEXT("Plan slot %d rolls the same elite twice"), PlanIndex), bSame);

		const FNightlightEliteProfile Other = UNightlightEliteGenerator::GenerateEliteProfile(
			Settings, 12, UNightlightEliteGenerator::MakeEliteSeed(1234, 12, PlanIndex + 1), PlayStyle, ENightlightWaveEnemyRole::Shade);
		DifferentProfiles += Other.Mutations != First.Mutations || Other.MutationStrengths != First.MutationStrengths ? 1 : 0;
	}
	TestTrue(TEXT("Different seeds give different elites"), DifferentProfiles > 25);

	// The name lists the mutations in pick order, then the base type.
	const FText Name = UNightlightEliteGenerator::BuildEliteDisplayName(
		{ ENightlightEliteMutation::Swift, ENightlightEliteMutation::Siphon }, ENightlightWaveEnemyRole::Shade);
	TestEqual(TEXT("The display name reads like the plan's example"), Name.ToString(), FString(TEXT("Swift Siphon Shade")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightEliteRulesTest,
	"Nightlight.Enemies.Elite.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightEliteRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UNightlightWaveSettings* const Settings = CreateEliteTestSettings();
	const FNightlightPlayStyle PlayStyle;
	int32 ClampFailures = 0;
	int32 PairFailures = 0;
	int32 CountFailures = 0;
	int32 RepeatFailures = 0;
	int32 StrengthFailures = 0;
	int32 NameFailures = 0;
	TSet<ENightlightEliteMutation> SeenMutations;

	// 1000 fixed seeds across waves 4 to 15, so both the one- and two-mutation waves are covered. Failures are
	// counted, so a broken rule reports once instead of a thousand times.
	for (int32 Sample = 0; Sample < 1000; ++Sample)
	{
		const int32 WaveNumber = 4 + Sample % 12;
		const ENightlightWaveEnemyRole Role = static_cast<ENightlightWaveEnemyRole>(Sample % 3);
		const int32 Seed = UNightlightEliteGenerator::MakeEliteSeed(2026, WaveNumber, Sample);
		const FNightlightEliteProfile Profile = UNightlightEliteGenerator::GenerateEliteProfile(Settings, WaveNumber, Seed, PlayStyle, Role);

		ClampFailures += Profile.HealthMultiplier > 2.0f + KINDA_SMALL_NUMBER || Profile.SpeedMultiplier > 1.6f + KINDA_SMALL_NUMBER ? 1 : 0;
		PairFailures += Profile.HasMutation(ENightlightEliteMutation::Swift) && Profile.HasMutation(ENightlightEliteMutation::Armoured) ? 1 : 0;
		CountFailures += Profile.Mutations.Num() != (WaveNumber >= 10 ? 2 : 1) ? 1 : 0;
		RepeatFailures += TSet<ENightlightEliteMutation>(Profile.Mutations).Num() != Profile.Mutations.Num() ? 1 : 0;
		NameFailures += !Profile.DisplayName.ToString().EndsWith(UEnum::GetDisplayValueAsText(Role).ToString()) ? 1 : 0;
		for (const float Strength : Profile.MutationStrengths)
		{
			StrengthFailures += Strength < 0.0f || Strength > 1.0f ? 1 : 0;
		}
		SeenMutations.Append(Profile.Mutations);

		// Spot checks on the ranges: each mutation's own stat stays inside the planned range.
		if (Profile.Mutations.Num() == 1)
		{
			switch (Profile.Mutations[0])
			{
			case ENightlightEliteMutation::Swift:
				ClampFailures += Profile.SpeedMultiplier < 1.3f - KINDA_SMALL_NUMBER || !FMath::IsNearlyEqual(Profile.HealthMultiplier, 0.85f) ? 1 : 0;
				ClampFailures += Profile.MeshScale.Z <= Profile.MeshScale.X ? 1 : 0;
				break;
			case ENightlightEliteMutation::Armoured:
				ClampFailures += Profile.HealthMultiplier < 1.5f - KINDA_SMALL_NUMBER || !FMath::IsNearlyEqual(Profile.SpeedMultiplier, 0.8f) ? 1 : 0;
				ClampFailures += Profile.MeshScale.X > 1.35f + KINDA_SMALL_NUMBER ? 1 : 0;
				break;
			case ENightlightEliteMutation::Frenzied:
				ClampFailures += !FMath::IsNearlyEqual(Profile.HurtAttackIntervalMultiplier, 0.6f) ? 1 : 0;
				break;
			case ENightlightEliteMutation::Siphon:
				ClampFailures += Profile.HealFraction < 0.15f - KINDA_SMALL_NUMBER || Profile.HealFraction > 0.3f + KINDA_SMALL_NUMBER ? 1 : 0;
				break;
			}
		}
	}

	TestEqual(TEXT("Health stays at most x2.0 and speed at most x1.6, inside each mutation's range"), ClampFailures, 0);
	TestEqual(TEXT("Swift and Armoured never appear together"), PairFailures, 0);
	TestEqual(TEXT("One mutation on waves 4 to 9 and two from wave 10"), CountFailures, 0);
	TestEqual(TEXT("No mutation repeats"), RepeatFailures, 0);
	TestEqual(TEXT("Every strength is between 0 and 1"), StrengthFailures, 0);
	TestEqual(TEXT("Every name ends with the base type"), NameFailures, 0);
	TestEqual(TEXT("All four mutations appear across the seeds"), SeenMutations.Num(), 4);

	// More mutations glow brighter, and the elite rewards follow the mutation count.
	const FNightlightEliteProfile OneMutation = UNightlightEliteGenerator::GenerateEliteProfile(Settings, 6, 11, PlayStyle, ENightlightWaveEnemyRole::Walker);
	const FNightlightEliteProfile TwoMutations = UNightlightEliteGenerator::GenerateEliteProfile(Settings, 12, 11, PlayStyle, ENightlightWaveEnemyRole::Walker);
	TestTrue(TEXT("Two mutations glow brighter than one"), TwoMutations.Glow > OneMutation.Glow);
	TestEqual(TEXT("One mutation earns 1.5 times the tokens"), OneMutation.TokenMultiplier, 1.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Two mutations earn twice the tokens"), TwoMutations.TokenMultiplier, 2.0f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightElitePlanTest,
	"Nightlight.Enemies.Elite.Plan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightElitePlanTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UNightlightWaveSettings* const Settings = CreateEliteTestSettings();
	const FNightlightEnemyMixWeights NeutralWeights;

	// Waves 1 to 3 cannot have elites, so they plan exactly what the plain planner does for the same seed.
	for (int32 WaveNumber = 1; WaveNumber <= 3; ++WaveNumber)
	{
		const int32 Budget = UNightlightWaveMath::CalculateWaveBudget(Settings, WaveNumber, 1.0f);
		FRandomStream PlainStream(99);
		FRandomStream EliteStream(99);
		const TArray<int32> Plain = UNightlightWaveMath::PlanWaveEnemies(Settings, WaveNumber, Budget, NeutralWeights, PlainStream);
		const TArray<FNightlightPlannedEnemy> WithElites = UNightlightWaveMath::PlanWaveEnemiesWithElites(Settings, WaveNumber, Budget, NeutralWeights, EliteStream);
		TArray<int32> WithElitesEntries;
		for (const FNightlightPlannedEnemy& PlannedEnemy : WithElites)
		{
			WithElitesEntries.Add(PlannedEnemy.EntryIndex);
		}
		TestTrue(FString::Printf(TEXT("Wave %d plans the same enemies as before elites"), WaveNumber), Plain == WithElitesEntries);
		TestEqual(FString::Printf(TEXT("Wave %d has no elites"), WaveNumber), CountElites(WithElites), 0);
		TestEqual(FString::Printf(TEXT("Wave %d leaves the stream where the plain planner does"), WaveNumber), EliteStream.GetCurrentSeed(), PlainStream.GetCurrentSeed());
	}

	// Many seeds at waves 4 to 20: elites appear, every plan spends exactly its budget and never more, and an
	// elite counts double towards it.
	int32 TotalElites = 0;
	int32 OverspentPlans = 0;
	int32 UnderspentPlans = 0;
	for (int32 Seed = 0; Seed < 200; ++Seed)
	{
		const int32 WaveNumber = 4 + Seed % 17;
		const int32 Budget = UNightlightWaveMath::CalculateWaveBudget(Settings, WaveNumber, 1.35f, 1.5f);
		FRandomStream Stream(Seed);
		const TArray<FNightlightPlannedEnemy> Plan = UNightlightWaveMath::PlanWaveEnemiesWithElites(Settings, WaveNumber, Budget, NeutralWeights, Stream);
		const int32 Spent = SumPlannedCost(Settings, Plan);
		OverspentPlans += Spent > Budget ? 1 : 0;
		UnderspentPlans += Spent < Budget ? 1 : 0;
		TotalElites += CountElites(Plan);
	}
	TestEqual(TEXT("No plan with elites overspends its budget"), OverspentPlans, 0);
	TestEqual(TEXT("Every plan with elites still spends its whole budget"), UnderspentPlans, 0);
	TestTrue(TEXT("Elites appear from wave 4"), TotalElites > 0);

	// With every pick promoted and only Walkers, the cost doubling is easy to see: a budget of 10 buys five elite
	// Walkers, and a budget of 9 buys four elites and one normal Walker, because the last elite would not fit.
	UNightlightWaveSettings* const WalkerSettings = CreateEliteTestSettings();
	WalkerSettings->Enemies.SetNum(1);
	WalkerSettings->EliteBaseChance = 1.0f;
	WalkerSettings->EliteMaxChance = 1.0f;
	FRandomStream WalkerStream(5);
	const TArray<FNightlightPlannedEnemy> TenPlan = UNightlightWaveMath::PlanWaveEnemiesWithElites(WalkerSettings, 4, 10, NeutralWeights, WalkerStream);
	TestEqual(TEXT("A budget of 10 buys 5 elite Walkers"), TenPlan.Num(), 5);
	TestEqual(TEXT("All 5 are elites"), CountElites(TenPlan), 5);
	const TArray<FNightlightPlannedEnemy> NinePlan = UNightlightWaveMath::PlanWaveEnemiesWithElites(WalkerSettings, 4, 9, NeutralWeights, WalkerStream);
	TestEqual(TEXT("A budget of 9 buys 5 Walkers"), NinePlan.Num(), 5);
	TestEqual(TEXT("Only 4 of them can be elites"), CountElites(NinePlan), 4);
	TestEqual(TEXT("An elite Walker costs 2"), UNightlightWaveMath::GetPlannedEnemyCost(WalkerSettings, TenPlan[0]), 2);

	// Plan indices are the order of the plan, which the elite seeds rely on.
	bool bPlanIndicesInOrder = true;
	for (int32 Index = 0; Index < TenPlan.Num(); ++Index)
	{
		bPlanIndicesInOrder &= TenPlan[Index].PlanIndex == Index;
	}
	TestTrue(TEXT("Each planned enemy knows its place in the plan"), bPlanIndicesInOrder);

	// The split keeps every elite flag and sorts elites by their doubled cost: an elite Walker (2) outranks a normal
	// Walker (1) and ties a Shade, so with only a few peak slots the elites land there first.
	const int32 WalkerIndex = 0;
	TArray<FNightlightPlannedEnemy> Mixed;
	for (int32 Slot = 0; Slot < 10; ++Slot)
	{
		FNightlightPlannedEnemy& PlannedEnemy = Mixed.AddDefaulted_GetRef();
		PlannedEnemy.EntryIndex = WalkerIndex;
		PlannedEnemy.PlanIndex = Slot;
		PlannedEnemy.bIsElite = Slot == 3 || Slot == 7;
	}
	FRandomStream SplitStream(42);
	const FNightlightPlannedPhaseLists Lists = UNightlightWaveMath::SplitPlannedEnemiesIntoPhases(Settings, Mixed, SplitStream);
	TestEqual(TEXT("Both elites survive the split"), CountElites(Lists.BuildUp) + CountElites(Lists.Peak) + CountElites(Lists.Relief), 2);
	TestEqual(TEXT("Both elites are in the peak, because they cost double"), CountElites(Lists.Peak), 2);
	TestEqual(TEXT("Nothing is lost in the split"), Lists.BuildUp.Num() + Lists.Peak.Num() + Lists.Relief.Num(), 10);

	FRandomStream RepeatStream(42);
	const FNightlightPlannedPhaseLists RepeatLists = UNightlightWaveMath::SplitPlannedEnemiesIntoPhases(Settings, Mixed, RepeatStream);
	TestTrue(TEXT("The same seed gives the same split with elites"),
		RepeatLists.BuildUp == Lists.BuildUp && RepeatLists.Peak == Lists.Peak && RepeatLists.Relief == Lists.Relief);

	// Moving a phase forward when the challenge level reacts keeps the elite flags.
	TArray<FNightlightPlannedEnemy> From = Lists.Peak;
	TArray<FNightlightPlannedEnemy> To = Lists.Relief;
	UNightlightWaveMath::MovePlannedEnemiesToFront(From, To);
	TestEqual(TEXT("Moved elites keep their flag"), CountElites(To), 2);
	TestTrue(TEXT("The moved phase is emptied"), From.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightElitePlayStyleTest,
	"Nightlight.Enemies.Elite.PlayStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightElitePlayStyleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UNightlightWaveSettings* const Settings = CreateEliteTestSettings();
	constexpr int32 Samples = 2000;
	const FNightlightPlayStyle Neutral;

	FNightlightPlayStyle LongRange;
	LongRange.bMostlyLongRange = true;
	FNightlightPlayStyle ShortRange;
	ShortRange.bMostlyShortRange = true;
	FNightlightPlayStyle Packed;
	Packed.bPackedTogether = true;

	// Each counter doubles its mutation's weight, so over many seeds it turns up clearly more often.
	const int32 NeutralSwift = CountMutation(Settings, 6, Neutral, ENightlightEliteMutation::Swift, Samples);
	const int32 NeutralArmoured = CountMutation(Settings, 6, Neutral, ENightlightEliteMutation::Armoured, Samples);
	const int32 NeutralSiphon = CountMutation(Settings, 6, Neutral, ENightlightEliteMutation::Siphon, Samples);
	TestTrue(TEXT("A long-range build sees more Swift elites"), CountMutation(Settings, 6, LongRange, ENightlightEliteMutation::Swift, Samples) > NeutralSwift * 1.3f);
	TestTrue(TEXT("A short-range build sees more Armoured elites"), CountMutation(Settings, 6, ShortRange, ENightlightEliteMutation::Armoured, Samples) > NeutralArmoured * 1.3f);
	TestTrue(TEXT("A packed build sees more Siphon elites"), CountMutation(Settings, 6, Packed, ENightlightEliteMutation::Siphon, Samples) > NeutralSiphon * 1.3f);

	// With four equal weights each mutation is about a quarter of the one-mutation elites.
	TestTrue(TEXT("Without a counter Swift is about a quarter of elites"), NeutralSwift > Samples / 6 && NeutralSwift < Samples / 3);

	FNightlightEliteMutationRule SwiftRule;
	SwiftRule.Mutation = ENightlightEliteMutation::Swift;
	SwiftRule.Counters = ENightlightWaveTemplateCounter::LongRange;
	TestEqual(TEXT("The counter doubles the weight"), UNightlightEliteGenerator::GetMutationWeight(Settings, SwiftRule, LongRange), 2.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Another play style leaves the weight alone"), UNightlightEliteGenerator::GetMutationWeight(Settings, SwiftRule, ShortRange), 1.0f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightEliteEnemyTest,
	"Nightlight.Enemies.Elite.Enemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightEliteEnemyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* const World = CreateEliteTestWorld();
	ANightlightEnemy* const Enemy = World->SpawnActor<ANightlightEnemy>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Enemy spawns"), Enemy))
	{
		DestroyEliteTestWorld(World);
		return false;
	}

	const float BaseHealth = Enemy->GetMaxHealth();
	const float BaseSpeed = GetFloatProperty(Enemy, TEXT("MovementSpeed"));
	const float BaseInterval = GetFloatProperty(Enemy, TEXT("DefenderAttackInterval"));
	const int32 BaseTokens = GetIntProperty(Enemy, TEXT("TokensOnDeath"));
	TestFalse(TEXT("A new enemy is not elite"), Enemy->IsElite());

	// A Frenzied Siphon Walker with simple numbers.
	FNightlightEliteProfile Profile;
	Profile.bIsElite = true;
	Profile.Mutations = { ENightlightEliteMutation::Frenzied, ENightlightEliteMutation::Siphon };
	Profile.MutationStrengths = { 0.5f, 0.5f };
	Profile.HealthMultiplier = 1.5f;
	Profile.SpeedMultiplier = 1.2f;
	Profile.HurtAttackIntervalMultiplier = 0.6f;
	Profile.FrenzyHealthFraction = 0.5f;
	Profile.HealFraction = 0.25f;
	Profile.TokenMultiplier = 2.0f;
	Profile.Glow = 2.0f;
	Profile.DisplayName = UNightlightEliteGenerator::BuildEliteDisplayName(Profile.Mutations, ENightlightWaveEnemyRole::Walker);
	Enemy->ApplyEliteProfile(Profile);

	TestTrue(TEXT("The enemy is elite"), Enemy->IsElite());
	TestEqual(TEXT("The name is exposed"), Enemy->GetEliteDisplayName().ToString(), FString(TEXT("Frenzied Siphon Walker")));
	TestEqual(TEXT("Max health is scaled"), Enemy->GetMaxHealth(), BaseHealth * 1.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("The elite starts at full health"), Enemy->GetCurrentHealth(), Enemy->GetMaxHealth(), KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Speed is scaled"), GetFloatProperty(Enemy, TEXT("MovementSpeed")), BaseSpeed * 1.2f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("Tokens are scaled"), GetIntProperty(Enemy, TEXT("TokensOnDeath")), BaseTokens * 2);

	// A second profile is ignored, so an elite is never scaled twice.
	Enemy->ApplyEliteProfile(Profile);
	TestEqual(TEXT("A second profile changes nothing"), Enemy->GetMaxHealth(), BaseHealth * 1.5f, KINDA_SMALL_NUMBER);

	// Frenzied: the attack interval only drops once health is below half.
	const float MaxHealth = Enemy->GetMaxHealth();
	Enemy->ApplyDamage(MaxHealth * 0.4f);
	TestEqual(TEXT("Above half health the attack interval is unchanged"), GetFloatProperty(Enemy, TEXT("DefenderAttackInterval")), BaseInterval, KINDA_SMALL_NUMBER);
	Enemy->ApplyDamage(MaxHealth * 0.2f);
	TestEqual(TEXT("Below half health the attack interval is x0.6"), GetFloatProperty(Enemy, TEXT("DefenderAttackInterval")), BaseInterval * 0.6f, KINDA_SMALL_NUMBER);

	// Siphon: a quarter of the damage dealt is healed back.
	const float HurtHealth = Enemy->GetCurrentHealth();
	Enemy->NotifyDefenderDamageDealt(40.0f);
	TestEqual(TEXT("Siphon heals a share of the damage dealt"), Enemy->GetCurrentHealth(), HurtHealth + 10.0f, KINDA_SMALL_NUMBER);
	Enemy->NotifyDefenderDamageDealt(10000.0f);
	TestEqual(TEXT("Healing never goes above max health"), Enemy->GetCurrentHealth(), MaxHealth, KINDA_SMALL_NUMBER);

	// A normal enemy never heals from its hits.
	ANightlightEnemy* const Normal = World->SpawnActor<ANightlightEnemy>(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	if (TestNotNull(TEXT("Second enemy spawns"), Normal))
	{
		Normal->ApplyDamage(30.0f);
		const float NormalHealth = Normal->GetCurrentHealth();
		Normal->NotifyDefenderDamageDealt(40.0f);
		TestEqual(TEXT("A normal enemy does not siphon"), Normal->GetCurrentHealth(), NormalHealth, KINDA_SMALL_NUMBER);
	}

	DestroyEliteTestWorld(World);
	return true;
}

#endif

/*
References

Epic Games, Inc., 2026a. Reflection System in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/reflection-system-in-unreal-engine>
[Accessed 6 November 2026].

Epic Games, Inc., 2026b. Write C++ Tests in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/write-cplusplus-tests-in-unreal-engine>
[Accessed 6 November 2026].
*/
