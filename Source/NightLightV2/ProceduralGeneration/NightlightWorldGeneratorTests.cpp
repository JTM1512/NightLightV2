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
	const TArray<FIntPoint> OriginalAnchorCoordinates = Generator->AnchorCoordinates;
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
	TestEqual(
		TEXT("Every approved anchor has a terrain-aligned world position"),
		Generator->GetAnchorWorldLocations().Num(),
		Generator->GetAnchorCoordinates().Num());

	bool bAnchorsStayOffRoutes = true;
	for (const FIntPoint& Anchor : Generator->GetAnchorCoordinates())
	{
		const FNightlightCellData& Cell = Generator->Cells[Generator->GetCellIndex(Anchor.X, Anchor.Y, 31)];
		if (Cell.Type != ENightlightCellType::PlacementAnchor || !Cell.bBuildable)
		{
			bAnchorsStayOffRoutes = false;
			break;
		}
	}
	TestTrue(TEXT("Approved anchors remain buildable and cannot be path cells"), bAnchorsStayOffRoutes);

	// Every anchor must sit on a level pad close to a route, and every route
	// must receive its minimum share of anchors.
	TArray<int32> PathDistances;
	TArray<int32> ClosestRoutes;
	Generator->BuildRouteDistanceField(31, 31, PathDistances, ClosestRoutes);

	TArray<int32> AnchorsPerRoute;
	AnchorsPerRoute.Init(0, Generator->GetRoutes().Num());
	bool bAnchorsNearRoutes = true;
	bool bAnchorPadsFlat = true;
	for (const FIntPoint& Anchor : Generator->GetAnchorCoordinates())
	{
		const int32 AnchorIndex = Generator->GetCellIndex(Anchor.X, Anchor.Y, 31);
		const int32 PathDistance = PathDistances[AnchorIndex];
		if (PathDistance < 2 || PathDistance > Generator->GenerationSettings.MaxAnchorPathDistance)
		{
			bAnchorsNearRoutes = false;
		}

		if (AnchorsPerRoute.IsValidIndex(ClosestRoutes[AnchorIndex]))
		{
			++AnchorsPerRoute[ClosestRoutes[AnchorIndex]];
		}

		const float AnchorHeight = Generator->Cells[AnchorIndex].Height;
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				const float PadHeight =
					Generator->Cells[Generator->GetCellIndex(Anchor.X + OffsetX, Anchor.Y + OffsetY, 31)].Height;
				if (!FMath::IsNearlyEqual(PadHeight, AnchorHeight))
				{
					bAnchorPadsFlat = false;
				}
			}
		}
	}
	TestTrue(TEXT("Anchors stay within reach of a route"), bAnchorsNearRoutes);
	TestTrue(TEXT("Anchor pads are level so platforms do not sink into slopes"), bAnchorPadsFlat);

	bool bEveryRouteDefended = true;
	for (const int32 RouteAnchorCount : AnchorsPerRoute)
	{
		if (RouteAnchorCount < Generator->GenerationSettings.MinAnchorsPerRoute)
		{
			bEveryRouteDefended = false;
		}
	}
	TestTrue(TEXT("Every route receives its minimum number of anchors"), bEveryRouteDefended);

	// The material reads one mask per channel, so paths, anchors and Ground must
	// not leak into each other's channels.
	FNightlightTerrainMeshData MeshData;
	bool bMasksMatchCells = Generator->BuildTerrainMeshData(MeshData);
	for (int32 Index = 0; bMasksMatchCells && Index < Generator->Cells.Num(); ++Index)
	{
		const ENightlightCellType Type = Generator->Cells[Index].Type;
		const FLinearColor& Mask = MeshData.VertexColors[Index];
		if ((Type == ENightlightCellType::Path && Mask != FLinearColor(1.0f, 0.0f, 0.0f, 0.0f))
			|| (Type == ENightlightCellType::PlacementAnchor && Mask != FLinearColor(0.0f, 1.0f, 0.0f, 0.0f))
			|| (Type == ENightlightCellType::Ground && Mask != FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)))
		{
			bMasksMatchCells = false;
		}
	}
	TestTrue(TEXT("Path, anchor and Ground cells carry only their own vertex colour mask"), bMasksMatchCells);

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
	Generator->AnchorCoordinates = OriginalAnchorCoordinates;
	Generator->ActiveSeed = OriginalActiveSeed;
	return true;
}

#endif
