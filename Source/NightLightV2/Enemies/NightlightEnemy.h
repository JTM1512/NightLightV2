#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightEnemy.generated.h"

class USceneComponent;

// An enemy follows one generated route from its Rift to the Dream Core.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemy : public AActor
{
	GENERATED_BODY()

public:
	ANightlightEnemy();

	virtual void Tick(float DeltaTime) override;

	// Copies the route, places the enemy at the Rift and starts its movement.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Enemy")
	void AssignRoute(const TArray<FVector>& RoutePoints);

protected:
	// Blueprint children can attach their mesh and other visuals to this root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float MovementSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy", meta = (ClampMin = "0.0"))
	float WaypointAcceptanceDistance = 10.0f;

	// The route is copied so the enemy does not need to keep checking the generator.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	TArray<FVector> AssignedRoutePoints;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	int32 CurrentWaypointIndex = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy")
	bool bHasReachedCore = false;

	// Later Core damage or other gameplay can be connected to this event.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy", meta = (DisplayName = "On Core Reached"))
	void OnCoreReached();

private:
	void MoveAlongRoute(float DeltaTime);
	void ReachNextWaypoint();
	void HandleCoreReached();
};
