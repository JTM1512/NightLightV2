#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightlightDefender.generated.h"

class USceneComponent;

// Dynamic multicast delegates let several Blueprints, such as the health bar, bind to the same
// event (Epic Games, Inc., 2026c).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightDefenderHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightDefenderDiedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightDefenderDamagedSignature,
	float, DamageAmount);

// Every defender Blueprint is parented to this class, so enemies can damage any defender type the same way.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightDefender : public AActor
{
	GENERATED_BODY()

public:
	ANightlightDefender();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	// Enemies only call this function. The defender is destroyed at zero so its platform can be used again.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Defender")
	void ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Defender")
	float GetMaxHealth() const { return MaxHealth; }

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderDiedSignature OnDefenderDied;

	// The Blueprint health bar uses this to show the amount taken by the latest hit.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Defender")
	FNightlightDefenderDamagedSignature OnDamageTaken;

protected:
	virtual void BeginPlay() override;

	// Blueprint children can attach their mesh and other visuals to this root.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// Same names as the old BP_Defender variables so the Blueprint can be reparented without renaming nodes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Defender", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Defender")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Defender")
	bool bIsDead = false;

private:
	void Die();
};
