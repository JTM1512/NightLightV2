#include "NightlightBlueprintBridge.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace NightlightBlueprintBridge
{
	namespace
	{
		template <typename PropertyType>
		PropertyType* FindTypedProperty(const UObject* const Object, const FName PropertyName)
		{
			// FindFProperty walks the class and its parents, so Blueprint variables are found too
			// (Epic Games, Inc., 2026b).
			return IsValid(Object) ? FindFProperty<PropertyType>(Object->GetClass(), PropertyName) : nullptr;
		}
	}

	bool ReadInt(const UObject* const Object, const FName PropertyName, int32& OutValue)
	{
		if (const FIntProperty* const Property = FindTypedProperty<FIntProperty>(Object, PropertyName))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}

		return false;
	}

	bool ReadBool(const UObject* const Object, const FName PropertyName, bool& OutValue)
	{
		if (const FBoolProperty* const Property = FindTypedProperty<FBoolProperty>(Object, PropertyName))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}

		return false;
	}

	bool ReadFloat(const UObject* const Object, const FName PropertyName, float& OutValue)
	{
		// Blueprint "Float" variables are doubles since UE5, so both kinds are accepted.
		if (const FDoubleProperty* const DoubleProperty = FindTypedProperty<FDoubleProperty>(Object, PropertyName))
		{
			OutValue = static_cast<float>(DoubleProperty->GetPropertyValue_InContainer(Object));
			return true;
		}

		if (const FFloatProperty* const FloatProperty = FindTypedProperty<FFloatProperty>(Object, PropertyName))
		{
			OutValue = FloatProperty->GetPropertyValue_InContainer(Object);
			return true;
		}

		return false;
	}

	bool WriteBool(UObject* const Object, const FName PropertyName, const bool bValue)
	{
		if (const FBoolProperty* const Property = FindTypedProperty<FBoolProperty>(Object, PropertyName))
		{
			Property->SetPropertyValue_InContainer(Object, bValue);
			return true;
		}

		return false;
	}

	UObject* ReadObject(const UObject* const Object, const FName PropertyName)
	{
		// Class references are object properties too, so they are skipped here.
		const FObjectPropertyBase* const Property = FindTypedProperty<FObjectPropertyBase>(Object, PropertyName);
		if (!Property || Property->IsA<FClassProperty>())
		{
			return nullptr;
		}

		return Property->GetObjectPropertyValue_InContainer(Object);
	}

	bool WriteInt(UObject* const Object, const FName PropertyName, const int32 Value)
	{
		if (const FIntProperty* const Property = FindTypedProperty<FIntProperty>(Object, PropertyName))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}

		return false;
	}

	bool HasFunction(const UObject* const Object, const FName FunctionName)
	{
		return IsValid(Object) && Object->FindFunction(FunctionName) != nullptr;
	}

	bool CallFunction(
		UObject* const Target,
		const FName FunctionName,
		const int32* const IntInput,
		int32* const OutInt,
		bool* const OutBool)
	{
		if (!IsValid(Target))
		{
			return false;
		}

		UFunction* const Function = Target->FindFunction(FunctionName);
		if (!Function)
		{
			return false;
		}

		// ProcessEvent takes one block of memory laid out like the function's parameters
		// (Epic Games, Inc., 2026a).
		uint8* const Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		FMemory::Memzero(Params, Function->ParmsSize);

		bool bInputSet = false;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* const Property = *It;
			Property->InitializeValue_InContainer(Params);

			const bool bIsOutput = Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)
				&& !Property->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (!bIsOutput && IntInput && !bInputSet)
			{
				if (const FIntProperty* const IntProperty = CastField<FIntProperty>(Property))
				{
					IntProperty->SetPropertyValue_InContainer(Params, *IntInput);
					bInputSet = true;
				}
			}
		}

		Target->ProcessEvent(Function, Params);

		bool bIntRead = false;
		bool bBoolRead = false;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* const Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				if (const FIntProperty* const IntProperty = CastField<FIntProperty>(Property); IntProperty && OutInt && !bIntRead)
				{
					*OutInt = IntProperty->GetPropertyValue_InContainer(Params);
					bIntRead = true;
				}
				else if (const FBoolProperty* const BoolProperty = CastField<FBoolProperty>(Property); BoolProperty && OutBool && !bBoolRead)
				{
					*OutBool = BoolProperty->GetPropertyValue_InContainer(Params);
					bBoolRead = true;
				}
			}

			Property->DestroyValue_InContainer(Params);
		}

		FMemory::Free(Params);
		return true;
	}
}

/*
References

Epic Games, Inc., 2026a. UObject::ProcessEvent. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/UObject/UObject/ProcessEvent>
[Accessed 3 November 2026].

Epic Games, Inc., 2026b. Unreal Engine Reflection System. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/reflection-system-in-unreal-engine>
[Accessed 3 November 2026].
*/
