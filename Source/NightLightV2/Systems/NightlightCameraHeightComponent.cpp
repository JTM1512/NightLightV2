#include "NightlightCameraHeightComponent.h"
#include "../ProceduralGeneration/NightlightWorldGenerator.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UNightlightCameraHeightComponent::UNightlightCameraHeightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNightlightCameraHeightComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* const Pawn = Cast<APawn>(GetOwner());
	APlayerController* const Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}

	// The game mode copies the PlayerStart rotation onto the controller after the pawn's BeginPlay,
	// so the start view is aimed on the first tick that has a controller instead (Epic Games, Inc., 2026a).
	if (bFaceMapOnStart && !bHasFacedMap)
	{
		FaceMap(*Controller);
	}

	// Reading the keys straight from the player controller keeps this separate from the
	// controller's own input mappings (Epic Games, Inc., 2026b).
	const float HeldDirection =
		static_cast<float>(Controller->IsInputKeyDown(EKeys::E)) - static_cast<float>(Controller->IsInputKeyDown(EKeys::Q));
	const float WheelDirection =
		static_cast<float>(Controller->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		- static_cast<float>(Controller->WasInputKeyJustPressed(EKeys::MouseScrollDown));
	const float HeightChange = HeldDirection * HeightSpeed * DeltaTime + WheelDirection * WheelStep;
	if (FMath::IsNearlyZero(HeightChange))
	{
		return;
	}

	FVector Location = GetOwner()->GetActorLocation();
	Location.Z = FMath::Clamp(Location.Z + HeightChange, MinHeight, MaxHeight);
	GetOwner()->SetActorLocation(Location);
}

FRotator UNightlightCameraHeightComponent::ComputeStartViewRotation(
	const FVector& ViewLocation,
	const FVector& TargetLocation,
	const float Pitch)
{
	// Only the flat direction sets the yaw, so the tilt stays the same however high the camera is.
	const FVector FlatDirection(TargetLocation.X - ViewLocation.X, TargetLocation.Y - ViewLocation.Y, 0.0);
	const float Yaw = FlatDirection.IsNearlyZero() ? 0.0f : static_cast<float>(FlatDirection.Rotation().Yaw);
	return FRotator(FMath::Clamp(Pitch, -89.0f, 0.0f), Yaw, 0.0f);
}

void UNightlightCameraHeightComponent::FaceMap(APlayerController& Controller)
{
	bHasFacedMap = true;

	// The level holds one generator, so the first one found is the map (Epic Games, Inc., 2026c).
	TActorIterator<ANightlightWorldGenerator> GeneratorIterator(GetWorld());
	if (!GeneratorIterator)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nightlight start view was not aimed because the level has no World Generator."));
		return;
	}

	const ANightlightWorldGenerator* const Generator = *GeneratorIterator;
	FVector Target;
	if (!Generator->GetCoreWorldLocation(Target))
	{
		// The grid grows from the generator's origin along +X and +Y, so its middle is half the size away.
		const FNightlightGenerationSettings& Settings = Generator->GetGenerationSettings();
		const float CellSize = FMath::Max(Settings.CellSize, 10.0f);
		const FVector GridCentre(
			(FMath::Max(Settings.GridWidth, 5) - 1) * CellSize * 0.5f,
			(FMath::Max(Settings.GridDepth, 5) - 1) * CellSize * 0.5f,
			0.0f);
		Target = Generator->GetActorTransform().TransformPosition(GridCentre);
	}

	Controller.SetControlRotation(ComputeStartViewRotation(GetOwner()->GetActorLocation(), Target, StartPitch));
}

/*
References

Epic Games, Inc., 2026a. AGameModeBase::FinishRestartPlayer. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AGameModeBase/FinishRestartPlayer>
[Accessed 3 November 2026].

Epic Games, Inc., 2026b. APlayerController. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerController>
[Accessed 2 October 2026].

Epic Games, Inc., 2026c. TActorIterator. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/TActorIterator>
[Accessed 3 November 2026].
*/
