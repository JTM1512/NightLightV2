#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NightlightCameraHeightComponent.generated.h"

// Lets the player raise and lower the camera pawn with E and Q or the mouse wheel, so hills never block
// the view of the map. Added to the player pawn Blueprint.
UCLASS(ClassGroup = (Nightlight), meta = (BlueprintSpawnableComponent))
class NIGHTLIGHTV2_API UNightlightCameraHeightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightlightCameraHeightComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
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
};
