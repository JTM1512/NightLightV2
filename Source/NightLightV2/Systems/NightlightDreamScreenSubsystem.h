#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightlightDreamScreenSubsystem.generated.h"

class UMaterialInstanceDynamic;

// Drives M_PP_DreamScreen, the custom post process material built by nightlight_dream_screen.py. It plays the
// dream ripple, a ring of wobble that rolls across the whole screen, and keeps the map edge fog lined up with
// the generated map so the seam where the terrain ends is hidden.
UCLASS()
class NIGHTLIGHTV2_API UNightlightDreamScreenSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Starts a ripple from where this world location is on screen, such as the Core. A weaker ripple does not
	// cut off a stronger one that is still rolling out, so a stream of small Core hits cannot hide an upgrade.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Dream Screen")
	void PlayRippleAt(FVector WorldOrigin, float Strength = 1.0f);

	// Starts a ripple from the middle of the screen, used when a wave starts.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Dream Screen")
	void PlayRippleFromScreenCentre(float Strength = 1.0f);

	// Strengths used by the Core and the wave director, so the three triggers are tuned in one place.
	static constexpr float CoreHitRippleStrength = 0.35f;
	static constexpr float CoreUpgradeRippleStrength = 0.8f;
	static constexpr float WaveStartRippleStrength = 1.0f;

protected:
	// Game and play-in-editor worlds only, so editor previews and asset thumbnails never get the effect.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;

	bool bMapBoundsSet = false;
	bool bRippleActive = false;
	bool bRippleFromWorld = false;
	FVector RippleWorldOrigin = FVector::ZeroVector;
	float RippleAge = 0.0f;
	float RippleStrength = 0.0f;

	void StartRipple(float Strength, bool bFromWorld, const FVector& WorldOrigin);
	float GetCurrentRippleStrength() const;
	FVector2D GetRippleCenter() const;
	void TrySetMapBounds();
};
