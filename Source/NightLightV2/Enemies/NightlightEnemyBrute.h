#pragma once

#include "CoreMinimal.h"
#include "NightlightEnemy.h"
#include "TimerManager.h"
#include "NightlightEnemyBrute.generated.h"

// A slow, heavy tank. It walks the route like the walker and hits the Core hard, but takes far longer to kill.
// Instead of a single hit it slams the ground after a wind-up, damaging every defender close to it.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemyBrute : public ANightlightEnemy
{
	GENERATED_BODY()

public:
	ANightlightEnemyBrute();

	UFUNCTION(BlueprintPure, Category = "Nightlight|Enemy|Brute")
	bool IsWindingUpSlam() const { return bIsWindingUpSlam; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Every defender within this distance of the Brute takes DefenderAttackDamage when a slam lands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Brute", meta = (ClampMin = "0.0"))
	float SlamRadius = 450.0f;

	// The pause between the wind-up and the hit, so the player can see the slam coming.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Enemy|Brute", meta = (ClampMin = "0.0"))
	float SlamWindUpTime = 0.6f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Nightlight|Enemy|Brute")
	bool bIsWindingUpSlam = false;

	// Lets the Brute Blueprint start its raise animation or warning effect.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy|Brute", meta = (DisplayName = "On Slam Wind Up"))
	void OnSlamWindUp();

	// Lets the Brute Blueprint play the impact effect. HitCount is the number of defenders damaged.
	UFUNCTION(BlueprintImplementableEvent, Category = "Nightlight|Enemy|Brute", meta = (DisplayName = "On Slam Landed"))
	void OnSlamLanded(int32 HitCount);

	// Starts a slam wind-up instead of hitting the one defender directly.
	virtual void AttackDefender(ANightlightDefender* Defender) override;

	// The Brute holds still while it winds up, even if its target dies in the meantime.
	virtual void MoveAlongRoute(float DeltaTime) override;

	virtual void Die() override;

private:
	FTimerHandle SlamWindUpTimerHandle;

	void LandSlam();
	void CancelSlam();
};
