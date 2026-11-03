#pragma once

#include "CoreMinimal.h"

class UObject;
class UClass;

// The token pool, the player controller and BPI_Upgradeable live in Blueprint, so the C++ crayon HUD reads
// and calls them by name. Every helper returns false, and changes nothing, when the name is missing.
namespace NightlightBlueprintBridge
{
	bool ReadInt(const UObject* Object, FName PropertyName, int32& OutValue);
	bool ReadBool(const UObject* Object, FName PropertyName, bool& OutValue);
	bool ReadFloat(const UObject* Object, FName PropertyName, float& OutValue);
	bool WriteBool(UObject* Object, FName PropertyName, bool bValue);

	// Object reference variables, such as a platform's Placed Defender.
	UObject* ReadObject(const UObject* Object, FName PropertyName);

	// Integer variables, such as the controller's Selected Defender (0 Patroller, 1 Shooter, 2 Torch).
	bool WriteInt(UObject* Object, FName PropertyName, int32 Value);

	bool HasFunction(const UObject* Object, FName FunctionName);

	// Calls a Blueprint function or interface function by name. IntInput, when given, fills its first int
	// input. OutInt and OutBool receive its first int and bool output or return value.
	bool CallFunction(
		UObject* Target,
		FName FunctionName,
		const int32* IntInput = nullptr,
		int32* OutInt = nullptr,
		bool* OutBool = nullptr);
}
