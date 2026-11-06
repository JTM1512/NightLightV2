#include "NightlightDreamScreenSubsystem.h"
#include "../ProceduralGeneration/NightlightWorldGenerator.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace NightlightDreamScreen
{
	// Built by Content/Python/nightlight_dream_screen.py.
	const TCHAR* const MaterialPath = TEXT("/Game/Materials/PostProcess/M_PP_DreamScreen.M_PP_DreamScreen");

	// How long one ripple takes to roll across the screen, in seconds.
	constexpr float RippleDuration = 1.1f;

	// A ripple no stronger than the one playing waits this long before replacing it, so a stream of Core hits
	// does not keep restarting the ring.
	constexpr float MinimumRippleGap = 0.45f;
}

bool UNightlightDreamScreenSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNightlightDreamScreenSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UMaterialInterface* const BaseMaterial = LoadObject<UMaterialInterface>(nullptr, NightlightDreamScreen::MaterialPath);
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dream screen: %s is missing. Run nightlight_dream_screen.py to build it."),
			NightlightDreamScreen::MaterialPath);
		return;
	}

	// A dynamic instance lets the ripple's parameters change every frame (Epic Games, Inc., 2026a).
	ScreenMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);

	// The level's unbound volume already holds the night exposure and bloom, so the effect joins it. A level
	// without one gets its own, so the effect still shows.
	APostProcessVolume* Volume = nullptr;
	for (TActorIterator<APostProcessVolume> It(&InWorld); It; ++It)
	{
		if (It->bUnbound && It->bEnabled)
		{
			Volume = *It;
			break;
		}
	}
	if (!Volume)
	{
		Volume = InWorld.SpawnActor<APostProcessVolume>();
		if (Volume)
		{
			Volume->bUnbound = true;
		}
	}

	// A post process material is added to a volume as a blendable (Epic Games, Inc., 2026b).
	if (Volume)
	{
		Volume->Settings.AddBlendable(ScreenMaterial, 1.0f);
	}

	TrySetMapBounds();
}

TStatId UNightlightDreamScreenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNightlightDreamScreenSubsystem, STATGROUP_Tickables);
}

void UNightlightDreamScreenSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!ScreenMaterial)
	{
		return;
	}

	// The generator can be spawned or build its terrain after play starts, so the fog keeps looking until the
	// terrain exists.
	if (!bMapBoundsSet)
	{
		TrySetMapBounds();
	}

	if (!bRippleActive)
	{
		return;
	}

	RippleAge += DeltaTime;
	const float Progress = RippleAge / NightlightDreamScreen::RippleDuration;
	if (Progress >= 1.0f)
	{
		bRippleActive = false;
		ScreenMaterial->SetScalarParameterValue(TEXT("RippleStrength"), 0.0f);
		ScreenMaterial->SetScalarParameterValue(TEXT("RippleProgress"), 0.0f);
		return;
	}

	// The centre follows the Core on screen, so the ring still starts at the Core if the camera moves.
	const FVector2D Center = GetRippleCenter();
	ScreenMaterial->SetVectorParameterValue(TEXT("RippleCenter"), FLinearColor(static_cast<float>(Center.X), static_cast<float>(Center.Y), 0.0f, 0.0f));
	ScreenMaterial->SetScalarParameterValue(TEXT("RippleProgress"), Progress);
	ScreenMaterial->SetScalarParameterValue(TEXT("RippleStrength"), RippleStrength);
}

void UNightlightDreamScreenSubsystem::PlayRippleAt(const FVector WorldOrigin, const float Strength)
{
	StartRipple(Strength, true, WorldOrigin);
}

void UNightlightDreamScreenSubsystem::PlayRippleFromScreenCentre(const float Strength)
{
	StartRipple(Strength, false, FVector::ZeroVector);
}

void UNightlightDreamScreenSubsystem::StartRipple(const float Strength, const bool bFromWorld, const FVector& WorldOrigin)
{
	if (!ScreenMaterial || Strength <= 0.0f || Strength < GetCurrentRippleStrength()
		|| (bRippleActive && RippleAge < NightlightDreamScreen::MinimumRippleGap && Strength <= RippleStrength))
	{
		return;
	}

	bRippleActive = true;
	bRippleFromWorld = bFromWorld;
	RippleWorldOrigin = WorldOrigin;
	RippleAge = 0.0f;
	RippleStrength = Strength;
}

float UNightlightDreamScreenSubsystem::GetCurrentRippleStrength() const
{
	// A ripple weakens as it rolls out, so a new one can replace it once it has faded below the new strength.
	return bRippleActive ? RippleStrength * (1.0f - RippleAge / NightlightDreamScreen::RippleDuration) : 0.0f;
}

FVector2D UNightlightDreamScreenSubsystem::GetRippleCenter() const
{
	const APlayerController* const Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!bRippleFromWorld || !Controller)
	{
		return FVector2D(0.5, 0.5);
	}

	// Projects the origin onto the screen and turns the pixel into a 0 to 1 screen position
	// (Epic Games, Inc., 2026c).
	FVector2D ScreenLocation;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0
		|| !Controller->ProjectWorldLocationToScreen(RippleWorldOrigin, ScreenLocation))
	{
		return FVector2D(0.5, 0.5);
	}

	return FVector2D(ScreenLocation.X / ViewportWidth, ScreenLocation.Y / ViewportHeight);
}

void UNightlightDreamScreenSubsystem::TrySetMapBounds()
{
	if (!ScreenMaterial || !GetWorld())
	{
		return;
	}

	TActorIterator<ANightlightWorldGenerator> It(GetWorld());
	if (!It || It->GetCells().Num() == 0)
	{
		return;
	}

	// The fog is fitted to the terrain mesh the generator actually built, not to its settings, so it follows
	// the real edge wherever the generator is placed, turned or scaled. The component's bounds are already in
	// world space (Epic Games, Inc., 2026d).
	const UProceduralMeshComponent* const TerrainMesh = It->FindComponentByClass<UProceduralMeshComponent>();
	if (!TerrainMesh)
	{
		return;
	}

	const FBox TerrainBox = TerrainMesh->Bounds.GetBox();
	if (!TerrainBox.IsValid || TerrainBox.GetExtent().X <= 0.0 || TerrainBox.GetExtent().Y <= 0.0)
	{
		return;
	}

	const FVector Center = TerrainBox.GetCenter();
	const FVector HalfSize = TerrainBox.GetExtent();

	ScreenMaterial->SetVectorParameterValue(TEXT("MapCenter"), FLinearColor(static_cast<float>(Center.X), static_cast<float>(Center.Y), static_cast<float>(Center.Z), 0.0f));
	ScreenMaterial->SetVectorParameterValue(TEXT("MapHalfSize"), FLinearColor(static_cast<float>(HalfSize.X), static_cast<float>(HalfSize.Y), 0.0f, 0.0f));
	bMapBoundsSet = true;

	UE_LOG(LogTemp, Log, TEXT("Dream screen: map edge fog set to centre (%.1f, %.1f), half size (%.1f, %.1f)."),
		Center.X, Center.Y, HalfSize.X, HalfSize.Y);
}

/*
References

Epic Games, Inc., 2026a. Instanced Materials in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine>
[Accessed 6 November 2026].

Epic Games, Inc., 2026b. Post Process Materials in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine>
[Accessed 6 November 2026].

Epic Games, Inc., 2026c. APlayerController::ProjectWorldLocationToScreen. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerController/ProjectWorldLocationToScreen>
[Accessed 6 November 2026].

Epic Games, Inc., 2026d. USceneComponent::Bounds. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneComponent/Bounds>
[Accessed 6 November 2026].
*/
