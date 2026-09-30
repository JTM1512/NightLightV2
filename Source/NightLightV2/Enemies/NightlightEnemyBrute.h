#pragma once

#include "CoreMinimal.h"
#include "NightlightEnemy.h"
#include "NightlightEnemyBrute.generated.h"

// A slow, heavy tank. It walks the route like the walker and hits the Core hard, but takes far longer to kill.
UCLASS(Blueprintable)
class NIGHTLIGHTV2_API ANightlightEnemyBrute : public ANightlightEnemy
{
	GENERATED_BODY()

public:
	ANightlightEnemyBrute();
};
