#include "NightlightWorldGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightRouteGenerationTest,
	"Nightlight.ProceduralGeneration.RouteData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightRouteGenerationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// The test uses the class default object because generation only changes
	// logical data. Restore its original state before leaving the test.
	ANightlightWorldGenerator* Generator = GetMutableDefault<ANightlightWorldGenerator>();
	const FNightlightGenerationSettings OriginalSettings = Generator->GenerationSettings;
	const TArray<FNightlightCellData> OriginalCells = Generator->Cells;
	const TArray<FNightlightRouteData> OriginalRoutes = Generator->Routes;
	const int32 OriginalActiveSeed = Generator->ActiveSeed;

	// Start with the documented Part 1 baseline and a controlled seed.
	Generator->GenerationSettings.GridWidth = 31;
	Generator->GenerationSettings.GridDepth = 31;
	Generator->GenerationSettings.RouteCount = 3;
	Generator->GenerationSettings.bUseRandomSeed = false;
	Generator->GenerationSettings.Seed = 1337;
	Generator->GenerateLogicalGrid();

	const FIntPoint CoreCoordinate(15, 15);
	TestEqual(TEXT("Fixed seed is used"), Generator->GetActiveSeed(), 1337);
	TestEqual(TEXT("Three routes are generated"), Generator->GetRoutes().Num(), 3);
	TestTrue(
		TEXT("Routes pass connectivity, role and overlap validation"),
		Generator->ValidateGeneratedRoutes(CoreCoordinate, 31, 31));

	// Keep the ordered routes, generate them again and compare the public contract.
	const TArray<FNightlightRouteData> FirstRoutes = Generator->GetRoutes();
	Generator->GenerateLogicalGrid();

	bool bFixedSeedMatches = FirstRoutes.Num() == Generator->GetRoutes().Num();
	for (int32 Index = 0; bFixedSeedMatches && Index < FirstRoutes.Num(); ++Index)
	{
		bFixedSeedMatches = FirstRoutes[Index].RiftCoordinate == Generator->GetRoutes()[Index].RiftCoordinate
			&& FirstRoutes[Index].CellsToCore == Generator->GetRoutes()[Index].CellsToCore;
	}
	TestTrue(TEXT("Fixed seed reproduces ordered route data"), bFixedSeedMatches);

	// A small spread of seeds catches route overlaps that may not appear in the
	// fixed-seed baseline without re-testing the earlier terrain increment.
	const TArray<int32> SeedsToCheck = { 1, 4, 8, 13, 21, 32 };
	bool bSeedSweepValid = true;
	for (const int32 Seed : SeedsToCheck)
	{
		Generator->GenerationSettings.Seed = Seed;
		Generator->GenerateLogicalGrid();
		if (!Generator->ValidateGeneratedRoutes(CoreCoordinate, 31, 31))
		{
			AddError(FString::Printf(TEXT("Route validation failed for seed %d."), Seed));
			bSeedSweepValid = false;
			break;
		}
	}
	TestTrue(TEXT("Seed sweep keeps every route connected"), bSeedSweepValid);

	Generator->GenerationSettings = OriginalSettings;
	Generator->Cells = OriginalCells;
	Generator->Routes = OriginalRoutes;
	Generator->ActiveSeed = OriginalActiveSeed;
	return true;
}

#endif
