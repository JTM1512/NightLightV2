#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightlightActorRegistrySubsystem.generated.h"

class ANightlightEnemy;
class ANightlightDefender;

// Dynamic so the HUD and wave director Blueprints can bind to them (Epic Games, Inc., 2026a).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightRegistryEnemySignature,
	ANightlightEnemy*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightRegistryDefenderSignature,
	ANightlightDefender*, Defender);

// Keeps a list of the enemies and defenders in one world, so targeting does not have to search every actor.
UCLASS()
class NIGHTLIGHTV2_API UNightlightActorRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// Enemies and defenders call these themselves from BeginPlay and EndPlay, so they are not exposed to Blueprints.
	void RegisterEnemy(ANightlightEnemy* Enemy);
	void UnregisterEnemy(ANightlightEnemy* Enemy);
	void RegisterDefender(ANightlightDefender* Defender);
	void UnregisterDefender(ANightlightDefender* Defender);

	// Only living enemies are returned. Dead and destroyed entries are skipped.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Actor Registry")
	TArray<ANightlightEnemy*> GetEnemies() const;

	// Only living defenders are returned. Dead and destroyed entries are skipped.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Actor Registry")
	TArray<ANightlightDefender*> GetDefenders() const;

	UFUNCTION(BlueprintPure, Category = "Nightlight|Actor Registry")
	int32 GetLivingEnemyCount() const;

	// Returns the closest living enemy within Range of Location, or null when there is none.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Actor Registry")
	ANightlightEnemy* FindClosestEnemy(FVector Location, float Range) const;

	// Returns the closest living defender within Range of Location, or null when there is none.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Actor Registry")
	ANightlightDefender* FindClosestDefender(FVector Location, float Range) const;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Actor Registry")
	FNightlightRegistryEnemySignature OnEnemyRegistered;

	// Fires when an enemy leaves play, whether it died, reached the Core or the level ended.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Actor Registry")
	FNightlightRegistryEnemySignature OnEnemyRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Actor Registry")
	FNightlightRegistryDefenderSignature OnDefenderRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Actor Registry")
	FNightlightRegistryDefenderSignature OnDefenderRemoved;

private:
	// Weak pointers so the registry never keeps a destroyed actor alive (Epic Games, Inc., 2026c).
	TArray<TWeakObjectPtr<ANightlightEnemy>> Enemies;
	TArray<TWeakObjectPtr<ANightlightDefender>> Defenders;
};
