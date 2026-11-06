#pragma once

#include "CoreMinimal.h"
#include "NightlightWaveTypes.generated.h"

// The small wave enums on their own, so the elite types can use them without including the whole settings asset.

// What an enemy type does in a wave, so the play-style counters can boost one role without knowing the class.
UENUM(BlueprintType)
enum class ENightlightWaveEnemyRole : uint8
{
	// Medium-speed melee enemy.
	Walker,

	// Fragile ranged enemy.
	Shade,

	// Slow, heavy area attacker.
	Brute
};

// The play style a wave template is picked to counter.
UENUM(BlueprintType)
enum class ENightlightWaveTemplateCounter : uint8
{
	None,

	// Mostly short-range defenders, such as Pulse.
	ShortRange,

	// Mostly long-range defenders, such as Shooters.
	LongRange,

	// Defenders packed close together.
	Packed
};
