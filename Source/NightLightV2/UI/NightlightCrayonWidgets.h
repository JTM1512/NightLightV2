#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "NightlightCrayonStyle.h"
#include "NightlightCrayonWidgets.generated.h"

class UButton;
class UOverlay;
class UTextBlock;
class UTexture2D;

// Every crayon widget builds its own tree in C++, so it works as soon as the textures are imported and
// needs no designer layout. A Widget Blueprint child can still add to it.
UCLASS(Abstract)
class NIGHTLIGHTV2_API UNightlightCrayonWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// Called once, when the tree is still empty. Must set WidgetTree->RootWidget.
	virtual void BuildTree() {}

	void EnsureBuilt();

	UTextBlock* MakeText(const FText& Text, ENightlightCrayonFont Font, int32 Size, const FLinearColor& Colour);
	UImage* MakeImage(UTexture2D* Texture, const FVector2D& Size);
};

// An Image that cycles the _F1 to _F3 frames of a drawing, so its lines "boil" like a hand drawn cartoon.
// Every boil image shares one clock, which the HUD speeds up as sanity drops.
UCLASS()
class NIGHTLIGHTV2_API UNightlightBoilImage : public UImage
{
	GENERATED_BODY()

public:
	void SetFrames(const TArray<UTexture2D*>& InFrames, const FVector2D& Size);

	// Advances the shared clock. SpeedScale 1 is the normal 8 frames per second.
	static void TickAll(float DeltaTime, float SpeedScale);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Crayon HUD", meta = (ClampMin = "0.0"))
	float FramesPerSecond = 8.0f;

protected:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Frames;

	int32 ShownFrame = INDEX_NONE;

	void ShowFrameForClock(float Clock);

	static TArray<TWeakObjectPtr<UNightlightBoilImage>>& GetLiveImages();
	static float& GetClock();
};

// A torn paper scrap that stretches to fit its content, with an optional strip of tape and a small
// random tilt, so no two panels sit perfectly straight.
UCLASS()
class NIGHTLIGHTV2_API UNightlightPaperPanel : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	// PaperPath is below the Crayon Textures folder. Call once, straight after creating the panel.
	void Setup(const FString& PaperPath, float Margin, const FMargin& ContentPadding, bool bWithTape, float MaxTiltDegrees = 2.0f);

	void SetContent(UWidget* Content);

	UOverlay* GetOverlay() const { return Root; }

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> Root;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PaperImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TapeImage;

	FMargin PanelPadding = FMargin(18.0f, 12.0f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightCrayonButtonClickedSignature);

// What a crayon button looks like.
UENUM(BlueprintType)
enum class ENightlightCrayonButtonKind : uint8
{
	// Green, for buying and upgrading.
	Buy,

	// Blue, for menus and closing panels.
	Secondary
};

// A button drawn as a fat crayon stroke with the kid's handwriting on it. It drops a few pixels when
// pressed and turns grey when disabled.
UCLASS()
class NIGHTLIGHTV2_API UNightlightCrayonButton : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	void Setup(ENightlightCrayonButtonKind Kind, const FText& Label, int32 FontSize = 26);

	void SetLabel(const FText& Label);

	void SetButtonEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightCrayonButtonClickedSignature OnClicked;

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	UFUNCTION()
	void HandleClicked();
};
