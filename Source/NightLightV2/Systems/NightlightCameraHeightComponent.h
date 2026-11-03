#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NightlightCameraHeightComponent.generated.h"

class APlayerController;

// Lets the player raise and lower the camera pawn with E and Q or the mouse wheel, so hills never block
// the view of the map. It also turns the view towards the Core when play starts, so the camera never
// opens facing away from the map. Added to the player pawn Blueprint.
UCLASS(ClassGroup = (Nightlight), meta = (BlueprintSpawnableComponent))
class NIGHTLIGHTV2_API UNightlightCameraHeightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightlightCameraHeightComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Rotation that looks from ViewLocation towards TargetLocation on the ground plane, tilted down by
	// Pitch degrees. Roll is always zero. Public and static so the automation test can check the maths.
	static FRotator ComputeStartViewRotation(const FVector& ViewLocation, const FVector& TargetLocation, float Pitch);

protected:
	// Turns the view towards the Core on the first frame of play.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera|Start View")
	bool bFaceMapOnStart = true;

	// Downward tilt of the starting view in degrees. Negative values look down at the map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera|Start View", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float StartPitch = -45.0f;

	// Units per second while E or Q is held.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera", meta = (ClampMin = "0.0"))
	float HeightSpeed = 800.0f;

	// Height change for one notch of the mouse wheel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera", meta = (ClampMin = "0.0"))
	float WheelStep = 150.0f;

	// World Z limits for the pawn, so the camera can neither sink into the terrain nor fly off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera")
	float MinHeight = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Camera")
	float MaxHeight = 2500.0f;

private:
	// Set once the starting view has been aimed, so the player's own looking is never overridden.
	bool bHasFacedMap = false;

	// Aims the controller at the Core, or at the middle of the grid when the Core is not ready.
	void FaceMap(APlayerController& Controller);
};
