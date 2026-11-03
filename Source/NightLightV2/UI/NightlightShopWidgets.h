#pragma once

#include "CoreMinimal.h"
#include "NightlightCrayonWidgets.h"
#include "NightlightShopWidgets.generated.h"

class UHorizontalBox;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightlightDrawingCardClickedSignature, int32, CardIndex);

// One of the kid's drawings taped to the shelf: a defender (or the Core) with its name, star price and
// hotkey. Unaffordable cards are scribbled out, and the picked card lifts and gets a red circle.
UCLASS()
class NIGHTLIGHTV2_API UNightlightDrawingCard : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	// DrawingPath is the drawing's boil frames without _F1, for example "Icons/Defenders/T_Defender_Torch".
	void Setup(int32 InCardIndex, const FString& DrawingPath, const FText& InName, const FText& Hotkey);

	// A negative cost hides the price, for example when the Core is fully upgraded.
	void SetCost(int32 Cost);

	void SetCardState(bool bAffordable, bool bPicked, bool bDimmed, bool bCorrupted);

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightDrawingCardClickedSignature OnCardClicked;

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNightlightBoilImage> Drawing;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> PriceRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HotkeyText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScribbleOut;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PickCircle;

	int32 CardIndex = 0;
	FText CardName;

	// The HUD sets these every frame, so nothing is rebuilt unless they changed.
	int32 ShownCost = MIN_int32;
	int32 ShownStateBits = INDEX_NONE;

	// The pop when a card is picked: it springs up and grows, overshoots a little, then settles.
	bool bIsPicked = false;
	float TiltDegrees = 0.0f;
	float Lift = 0.0f;
	float LiftVelocity = 0.0f;
	float Scale = 1.0f;
	float ScaleVelocity = 0.0f;
	float Wiggle = 0.0f;
	float WiggleVelocity = 0.0f;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleClicked();
};

// What the notebook page shows for the Core or one defender.
USTRUCT(BlueprintType)
struct FNightlightNotebookInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	FText Title;

	// Boil frames of the portrait, without _F1.
	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	FString PortraitPath;

	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	int32 Level = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	int32 MaxLevel = 0;

	// -1 when there is no next tier.
	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	int32 Cost = -1;

	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	bool bCanAfford = false;

	// Written as "power: 28 -> 38!".
	UPROPERTY(BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	TArray<FText> StatLines;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightlightNotebookActionSignature);

// The notebook page where the kid colours in stars to make a drawing stronger. Works the same for the
// Core and for any defender that implements BPI_Upgradeable.
UCLASS()
class NIGHTLIGHTV2_API UNightlightNotebookPage : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	void SetInfo(const FNightlightNotebookInfo& Info);

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightNotebookActionSignature OnUpgradeClicked;

	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightNotebookActionSignature OnCloseClicked;

protected:
	virtual void BuildTree() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightBoilImage> Portrait;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StarRow;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StatList;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CostRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostText;

	// Says why the button is grey, or that every star is coloured in.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightCrayonButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightCrayonButton> CloseButton;

	FString ShownPortrait;

	// What the page last showed. The HUD refreshes it twice a second, and rebuilding the wrapped text
	// every time made it flicker for a frame, so nothing is rebuilt unless this changes.
	FString ShownSignature;

	UFUNCTION()
	void HandleUpgrade();

	UFUNCTION()
	void HandleClose();
};

// Dripping red nightmare writing that jitters and fades after a moment, for warnings such as
// "SOMETHING BIG IS COMING FROM THE EAST".
UCLASS()
class NIGHTLIGHTV2_API UNightlightNightmareText : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	void ShowMessage(const FText& Message, float Seconds, int32 FontSize);

	bool IsShowing() const { return TimeLeft > 0.0f; }

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	float TimeLeft = 0.0f;
	float JitterClock = 0.0f;
};
