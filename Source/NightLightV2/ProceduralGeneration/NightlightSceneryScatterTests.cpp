#include "NightlightSceneryScatter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NightlightWorldGenerator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNightlightSceneryScatterTest,
	"Nightlight.ProceduralGeneration.Scenery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightlightSceneryScatterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// A game world that never begins play lets the scatter register its instance components.
	UWorld* const World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	ANightlightWorldGenerator* const Generator = World->SpawnActor<ANightlightWorldGenerator>();
	ANightlightSceneryScatter* const Scatter = World->SpawnActor<ANightlightSceneryScatter>();
	UStaticMesh* const Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Generator spawns"), Generator) || !TestNotNull(TEXT("Scatter spawns"), Scatter)
		|| !TestNotNull(TEXT("Cube mesh loads"), Cube))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	Generator->GenerationSettings.bUseRandomSeed = false;
	Generator->bCreateTerrainCollision = false;
	Scatter->WorldGenerator = Generator;
	Scatter->RockMeshes = { Cube };
	Scatter->BorderMeshes = { Cube };

	// A small cube would sit on the edge cells, so push the border clear of the grid for this check.
	Scatter->BorderDistance = 1000.0f;

	// Generates and scatters one seed, then returns every instance's world transform in order.
	auto ScatterSeed = [&](const int32 Seed)
	{
		Generator->GenerationSettings.Seed = Seed;
		Generator->GenerateLogicalGrid();
		Scatter->ScatterScenery();

		TArray<FTransform> Transforms;
		for (const UHierarchicalInstancedStaticMeshComponent* Component : Scatter->SceneryComponents)
		{
			for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
			{
				Component->GetInstanceTransform(Index, Transforms.AddDefaulted_GetRef(), true);
			}
		}
		return Transforms;
	};

	const TArray<FTransform> First = ScatterSeed(1337);
	TestTrue(TEXT("Scenery is scattered"), First.Num() > Scatter->BorderCount);

	// Instances inside the 31 x 31 grid of 200-unit cells must sit on Ground with only Ground beside them.
	// Anything outside the grid belongs to the border ring.
	const TArray<FNightlightCellData>& Cells = Generator->GetCells();
	const FIntPoint Offsets[] = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	bool bClearOfReservedCells = true;
	for (const FTransform& Transform : First)
	{
		const FVector Local = Generator->GetActorTransform().InverseTransformPosition(Transform.GetLocation());
		const FIntPoint Cell(FMath::RoundToInt(Local.X / 200.0), FMath::RoundToInt(Local.Y / 200.0));
		for (const FIntPoint& Offset : Offsets)
		{
			const FIntPoint Check = Cell + Offset;
			if (Check.X >= 0 && Check.X < 31 && Check.Y >= 0 && Check.Y < 31
				&& Cells[Check.Y * 31 + Check.X].Type != ENightlightCellType::Ground)
			{
				bClearOfReservedCells = false;
			}
		}
	}
	TestTrue(TEXT("No instance lies on or next to a path, anchor, Rift or Core cell"), bClearOfReservedCells);

	const TArray<FTransform> Again = ScatterSeed(1337);
	TestEqual(TEXT("The same seed gives the same instance count"), Again.Num(), First.Num());
	TestTrue(TEXT("The same seed gives the same first transforms"),
		Again.Num() > 2 && Again[0].Equals(First[0]) && Again[1].Equals(First[1]) && Again[2].Equals(First[2]));

	const TArray<FTransform> Other = ScatterSeed(42);
	TestTrue(TEXT("A different seed gives different scenery"), Other.Num() != First.Num() || !Other[0].Equals(First[0]));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
