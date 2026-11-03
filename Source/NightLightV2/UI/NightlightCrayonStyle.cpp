#include "NightlightCrayonStyle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"

namespace NightlightCrayonStyle
{
	namespace
	{
		const TCHAR* const CrayonRoot = TEXT("/Game/UI/Crayon");

		// Paths that already failed, so a missing asset only warns once instead of every frame.
		TSet<FString>& GetMissingPaths()
		{
			static TSet<FString> MissingPaths;
			return MissingPaths;
		}

		template <typename AssetType>
		AssetType* LoadCrayonAsset(const FString& Folder, const FString& RelativePath)
		{
			const FString PackagePath = FString::Printf(TEXT("%s/%s/%s"), CrayonRoot, *Folder, *RelativePath);
			if (GetMissingPaths().Contains(PackagePath))
			{
				return nullptr;
			}

			// The object name matches the file name, so the full path is Package.Name
			// (Epic Games, Inc., 2026a).
			const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *FPaths::GetBaseFilename(RelativePath));
			AssetType* const Asset = LoadObject<AssetType>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Asset)
			{
				GetMissingPaths().Add(PackagePath);
				UE_LOG(LogTemp, Warning, TEXT("Crayon HUD asset %s is missing. Import it from Art/CrayonHUD."), *PackagePath);
			}

			return Asset;
		}

		FLinearColor FromHex(const TCHAR* const Hex)
		{
			return FLinearColor(FColor::FromHex(Hex));
		}
	}

	FLinearColor Paper() { return FromHex(TEXT("F4ECD6")); }
	FLinearColor Pencil() { return FromHex(TEXT("3A3A46")); }
	FLinearColor Sun() { return FromHex(TEXT("FFC93C")); }
	FLinearColor Orange() { return FromHex(TEXT("FF8A3D")); }
	FLinearColor Sky() { return FromHex(TEXT("4AA3FF")); }
	FLinearColor Grass() { return FromHex(TEXT("5CC96B")); }
	FLinearColor Ink() { return FromHex(TEXT("0A0509")); }
	FLinearColor NightmareRed() { return FromHex(TEXT("D21F3C")); }

	UTexture2D* LoadTexture(const FString& RelativePath)
	{
		return LoadCrayonAsset<UTexture2D>(TEXT("Textures"), RelativePath);
	}

	UMaterialInterface* LoadMaterial(const FString& Name)
	{
		return LoadCrayonAsset<UMaterialInterface>(TEXT("Materials"), Name);
	}

	FSlateFontInfo GetFont(const ENightlightCrayonFont Font, const int32 Size)
	{
		const TCHAR* FontName = TEXT("F_GochiHand");
		switch (Font)
		{
		case ENightlightCrayonFont::Nightmare: FontName = TEXT("F_RubikWetPaint"); break;
		case ENightlightCrayonFont::Corrupted: FontName = TEXT("F_RubikGlitch"); break;
		default: break;
		}

		if (const UFont* const FontAsset = LoadCrayonAsset<UFont>(TEXT("Fonts"), FontName))
		{
			return FSlateFontInfo(FontAsset, Size);
		}

		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	}

	FSlateFontInfo GetLabelFont(const bool bCorrupted, const int32 Size)
	{
		return GetFont(bCorrupted ? ENightlightCrayonFont::Corrupted : ENightlightCrayonFont::Kid, Size);
	}

	FSlateBrush MakeImageBrush(UTexture2D* const Texture, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		if (!Texture)
		{
			// Keep the space but draw nothing, so a missing texture never shows the white default box.
			Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		}

		return Brush;
	}

	FSlateBrush MakeBoxBrush(UTexture2D* const Texture, const float Margin)
	{
		FSlateBrush Brush = MakeImageBrush(Texture, Texture ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) * 0.5f : FVector2D(64.0f));
		if (Texture)
		{
			// Box draws the corners at their own size and stretches the middle (Epic Games, Inc., 2026b).
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.Margin = FMargin(Margin);
		}

		return Brush;
	}

	FSlateBrush MakeTileBrush(UTexture2D* const Texture, const FVector2D& TileSize)
	{
		FSlateBrush Brush = MakeImageBrush(Texture, TileSize);
		Brush.Tiling = ESlateBrushTileType::Both;
		return Brush;
	}

	TArray<UTexture2D*> LoadBoilFrames(const FString& RelativePathWithoutFrame)
	{
		TArray<UTexture2D*> Frames;
		for (int32 FrameIndex = 1; FrameIndex <= 3; ++FrameIndex)
		{
			if (UTexture2D* const Frame = LoadTexture(FString::Printf(TEXT("%s_F%d"), *RelativePathWithoutFrame, FrameIndex)))
			{
				Frames.Add(Frame);
			}
		}

		return Frames;
	}
}

namespace
{
	template <typename AssetType>
	AssetType* FindOrCreateAsset(const FString& PackagePath)
	{
		UPackage* const Package = CreatePackage(*PackagePath);
		const FString Name = FPackageName::GetShortName(PackagePath);
		if (AssetType* const Existing = FindObject<AssetType>(Package, *Name))
		{
			return Existing;
		}

		if (AssetType* const Loaded = LoadObject<AssetType>(Package, *Name, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Loaded;
		}

		AssetType* const Asset = NewObject<AssetType>(Package, *Name, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}
}

bool UNightlightCrayonEditorLibrary::CreateFontFromFile(
	const FString& SourceFile,
	const FString& FacePackagePath,
	const FString& FontPackagePath)
{
#if WITH_EDITORONLY_DATA
	TArray<uint8> FontBytes;
	if (!FFileHelper::LoadFileToArray(FontBytes, *SourceFile))
	{
		return false;
	}

	UFontFace* const FontFace = FindOrCreateAsset<UFontFace>(FacePackagePath);
	FontFace->InitializeFromBulkData(SourceFile, EFontHinting::Default, FontBytes.GetData(), FontBytes.Num());

	// Inline keeps the font inside its asset, so nothing extra has to be cooked next to it.
	FontFace->LoadingPolicy = EFontLoadingPolicy::Inline;
	FontFace->MarkPackageDirty();

	return SetupFontFromFace(FindOrCreateAsset<UFont>(FontPackagePath), FontFace);
#else
	return false;
#endif
}

bool UNightlightCrayonEditorLibrary::SetupFontFromFace(UFont* const Font, UFontFace* const FontFace)
{
	if (!Font || !FontFace)
	{
		return false;
	}

	// Runtime fonts render straight from the Font Face, so one typeface entry is all the composite
	// font needs (Epic Games, Inc., 2026c).
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& CompositeFont = Font->GetMutableInternalCompositeFont();
	CompositeFont.DefaultTypeface.Fonts.Reset();
	FTypefaceEntry& Entry = CompositeFont.DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Entry.Name = TEXT("Regular");
	Entry.Font = FFontData(FontFace);
	Font->MarkPackageDirty();
	return true;
}

/*
References

Epic Games, Inc., 2026a. Asynchronous Asset Loading in Unreal Engine. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/asynchronous-asset-loading-in-unreal-engine>
[Accessed 3 November 2026].

Epic Games, Inc., 2026b. FSlateBrush. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/Styling/FSlateBrush>
[Accessed 3 November 2026].

Epic Games, Inc., 2026c. Fonts in UMG. [online] Available at:
<https://dev.epicgames.com/documentation/en-us/unreal-engine/using-fonts-with-umg-in-unreal-engine>
[Accessed 3 November 2026].
*/
