#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightlightCrayonStyle.generated.h"

class UFont;
class UFontFace;
class UMaterialInterface;
class UTexture2D;

// The three text styles from the plan: the kid's handwriting, the nightmare's dripping text, and the
// glitched text that replaces the kid's writing below 30% sanity.
UENUM(BlueprintType)
enum class ENightlightCrayonFont : uint8
{
	Kid,
	Nightmare,
	Corrupted
};

// Shared colours, fonts and brushes for every crayon widget. Assets live in /Game/UI/Crayon and are
// imported from Art/CrayonHUD, so real crayon scans saved over the PNGs with the same names drop
// straight in.
namespace NightlightCrayonStyle
{
	// Colours from the asset pack breakdown.
	FLinearColor Paper();
	FLinearColor Pencil();
	FLinearColor Sun();
	FLinearColor Orange();
	FLinearColor Sky();
	FLinearColor Grass();
	FLinearColor Ink();
	FLinearColor NightmareRed();

	// RelativePath is below /Game/UI/Crayon/Textures, for example "SanityBar/T_SanityBar_Frame".
	// Null, with one warning, when the texture has not been imported yet.
	UTexture2D* LoadTexture(const FString& RelativePath);

	// Name is below /Game/UI/Crayon/Materials, for example "M_UI_InkOverlay".
	UMaterialInterface* LoadMaterial(const FString& Name);

	// Falls back to the engine font until the crayon fonts are imported.
	FSlateFontInfo GetFont(ENightlightCrayonFont Font, int32 Size);

	// The kid's text stays readable, so it only switches to the corrupted font when asked.
	FSlateFontInfo GetLabelFont(bool bCorrupted, int32 Size);

	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& Size);

	// 9-slice brush. Margin is a fraction of the texture, as UMG's Margin field expects.
	FSlateBrush MakeBoxBrush(UTexture2D* Texture, float Margin);

	FSlateBrush MakeTileBrush(UTexture2D* Texture, const FVector2D& TileSize);

	// The three line boil frames of a drawing, for example "Icons/Defenders/T_Defender_Shooter".
	TArray<UTexture2D*> LoadBoilFrames(const FString& RelativePathWithoutFrame);
}

// Makes the Font Face and Font assets from a font file, because the editor's font importer opens a
// dialog that a headless import cannot answer.
UCLASS()
class NIGHTLIGHTV2_API UNightlightCrayonEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Makes or refreshes FacePackagePath as a Font Face holding SourceFile, and FontPackagePath as a Font
	// that uses it. Both are left unsaved. Editor only; returns false in a packaged game.
	UFUNCTION(BlueprintCallable, Category = "Nightlight|Crayon HUD", meta = (DevelopmentOnly))
	static bool CreateFontFromFile(const FString& SourceFile, const FString& FacePackagePath, const FString& FontPackagePath);

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Crayon HUD", meta = (DevelopmentOnly))
	static bool SetupFontFromFace(UFont* Font, UFontFace* FontFace);
};
