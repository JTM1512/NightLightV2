#include "NightlightSanityLibrary.h"

ENightlightSanityState UNightlightSanityLibrary::GetSanityState(
	const float Percent,
	const float ScaredBelow,
	const float LosingItBelow)
{
	if (Percent < LosingItBelow)
	{
		return ENightlightSanityState::LosingIt;
	}

	return Percent < ScaredBelow ? ENightlightSanityState::Scared : ENightlightSanityState::Brave;
}

int32 UNightlightSanityLibrary::GetInkStage(const float Percent, const float OverrunBelow)
{
	if (Percent < OverrunBelow)
	{
		return 3;
	}

	return static_cast<int32>(GetSanityState(Percent));
}

FText UNightlightSanityLibrary::CorruptText(const FText& Text)
{
	FString Corrupted = Text.ToString();
	for (TCHAR& Character : Corrupted)
	{
		switch (Character)
		{
		case TEXT('o'): case TEXT('O'): Character = TEXT('0'); break;
		case TEXT('e'): case TEXT('E'): Character = TEXT('3'); break;
		case TEXT('i'): case TEXT('I'): Character = TEXT('1'); break;
		default: break;
		}
	}

	return FText::FromString(Corrupted);
}
