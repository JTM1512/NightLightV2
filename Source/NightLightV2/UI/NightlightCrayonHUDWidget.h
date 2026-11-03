#pragma once

#include "CoreMinimal.h"
#include "NightlightCrayonWidgets.h"
#include "NightlightSanityLibrary.h"
#include "NightlightShopWidgets.h"
#include "../Waves/NightlightWaveDirector.h"
#include "NightlightCrayonHUDWidget.generated.h"

class AGameStateBase;
class ANightlightDreamCore;
class ANightlightEnemy;
class ANightlightEnemySpawner;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UMaterialInstanceDynamic;
class URetainerBox;
class UNightlightCrayonScreen;
class UNightlightInkOverlay;
class UNightlightSanityBar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNightlightTokensChangedSignature,
	int32, Tokens,
	int32, Change);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FNightlightSanityStateChangedSignature,
	ENightlightSanityState, NewState);

// The whole crayon HUD: the kid's drawings, paper scraps and stars fighting the nightmare's ink. It reads
// the same functions and events as WBP_GameHUD, so the game logic stays where it is and only the look
// changes. ANightlightCrayonHUD creates it and forwards the keys.
UCLASS()
class NIGHTLIGHTV2_API UNightlightCrayonHUDWidget : public UNightlightCrayonWidget
{
	GENERATED_BODY()

public:
	// The Core's health percent, smoothed, from 0 to 1.
	UFUNCTION(BlueprintPure, Category = "Nightlight|Crayon HUD")
	float GetSanity() const { return DisplayedSanity; }

	UFUNCTION(BlueprintPure, Category = "Nightlight|Crayon HUD")
	ENightlightSanityState GetSanityState() const { return SanityState; }

	// The token pool lives in BP_NightLightGameState, which has no event, so the HUD watches it and fires
	// this whenever it changes. Change is negative when tokens were spent.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightTokensChangedSignature OnTokensChanged;

	// Every widget that changes mood listens to this one place.
	UPROPERTY(BlueprintAssignable, Category = "Nightlight|Crayon HUD")
	FNightlightSanityStateChangedSignature OnSanityStateChanged;

	// Keys forwarded by ANightlightCrayonHUD.
	void StartWaveEarly();
	void ToggleSpeed();
	void TogglePause();
	void ToggleNotebookUnderCursor();
	void SelectUnderCursor();

	// Keys 1 to 3 and the shelf cards call this, so the picked card pops even when the controller's
	// Selected Defender cannot be read. Picking the same card again, or INDEX_NONE, clears the pick.
	// Returns true when a card is picked afterwards.
	bool PickCard(int32 CardIndex);
	void ClearPickedCard();

	// True while the mouse is over a platform and no defender is picked. ANightlightCrayonHUD then keeps
	// the click from the controller, so defenders are only placed after picking one.
	bool ShouldBlockPlacementClick() const;

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Crayon HUD")
	void OpenNotebook(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Nightlight|Crayon HUD")
	void CloseNotebook();

	// The noise wobble and colour drain over the whole HUD. Turn off on slow machines.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	bool bUseSanityWarp = true;

	// Removes WBP_GameHUD and swaps WBP_PaueMenu and WBP_GameOver for the crayon screens when the player
	// controller creates them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightlight|Crayon HUD")
	bool bReplaceOldWidgets = true;

protected:
	virtual void BuildTree() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	// One drawing on the shelf.
	struct FShelfEntry
	{
		FText Name;
		FString DrawingPath;
		FSoftClassPath DefenderClassPath;
		FName CostProperty;
		FText Hotkey;
	};

	// A "+10" star that floats up from the token count.
	struct FStarPop
	{
		TWeakObjectPtr<UWidget> Widget;
		float Age = 0.0f;
	};

	TArray<FShelfEntry> ShelfEntries;

	TWeakObjectPtr<ANightlightDreamCore> Core;
	TWeakObjectPtr<ANightlightWaveDirector> Director;
	TWeakObjectPtr<ANightlightEnemySpawner> Spawner;
	TWeakObjectPtr<AActor> NotebookTarget;
	TArray<TWeakObjectPtr<AActor>> Platforms;

	UPROPERTY(Transient)
	TObjectPtr<URetainerBox> Retainer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WarpMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MarkerLayer;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightInkOverlay> InkOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightSanityBar> SanityBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TokenText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SlotFigures;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveLabel;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> LampButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LampText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> SpeedNote;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNightlightDrawingCard>> Cards;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotsFullHint;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightNotebookPage> Notebook;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightNightmareText> Warning;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightNightmareText> BigWarning;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> WaveCleared;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveClearedSummary;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RangeRing;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelectedRing;

	UPROPERTY(Transient)
	TObjectPtr<UImage> NotebookArrow;

	// The picked defender's drawing with "next: Shooter", following the mouse so the player always
	// sees what the next click will place.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> NextMarker;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightBoilImage> NextDrawing;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextText;

	int32 ShownNextCard = INDEX_NONE;
	bool bShownNextCorrupted = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> PlatformMarks;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightCrayonScreen> PauseScreen;

	UPROPERTY(Transient)
	TObjectPtr<UNightlightCrayonScreen> GameOverScreen;

	float TargetSanity = 1.0f;
	float DisplayedSanity = 1.0f;
	ENightlightSanityState SanityState = ENightlightSanityState::Brave;
	bool bWarnedThisWave = false;

	int32 Tokens = INDEX_NONE;
	int32 ShownDefenderCount = INDEX_NONE;
	int32 ShownMaxDefenders = INDEX_NONE;
	int32 EnemiesLeft = 0;
	bool bFastForward = false;

	float WaveClearedTimeLeft = 0.0f;
	float SweepTimer = 0.0f;
	float PlatformSearchTimer = 0.0f;
	float ActorSearchTimer = 0.0f;
	float NotebookRefreshTimer = 0.0f;
	bool bWarnedAboutSelection = false;
	int32 PickedCard = INDEX_NONE;
	bool bClearControllerSelection = false;
	bool bHoveringPlatform = false;
	TArray<FStarPop> StarPops;

	void BuildTopLeft();
	void BuildTopRight();
	void BuildBottom();
	void BuildMessages();
	void BuildMarkers();
	UCanvasPanelSlot* AddToCanvas(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);

	void FindGameActors();
	void UpdateSanity(float DeltaTime);
	void UpdateTokens();
	void UpdateSlots();
	void UpdateCountdown();
	void UpdateShelf();
	void UpdateMarkers();
	void UpdateNextMarker(int32 SelectedCard, AActor* Hovered);
	void UpdateNotebook();
	void UpdateStarPops(float DeltaTime);
	void SweepOldWidgets();
	void RefreshWaveLabel();
	void ShowPauseScreen();
	void ShowGameOver();
	void SetGameAndUIInput() const;

	AGameStateBase* GetGameState() const;
	AActor* GetActorUnderCursor() const;
	AActor* FindUpgradeable(AActor* HitActor) const;
	bool CanAfford(int32 Cost) const;
	bool SpendTokens(int32 Cost) const;
	int32 GetSelectedCard() const;
	int32 GetShelfCost(int32 CardIndex) const;
	bool ProjectToHUD(const FVector& WorldLocation, FVector2D& OutPosition) const;
	float ProjectRadius(const FVector& WorldLocation, float WorldRadius) const;
	void PlaceRing(UImage* Ring, const FVector& WorldLocation, float WorldRadius);
	FNightlightNotebookInfo MakeNotebookInfo(AActor* Target) const;

	UFUNCTION()
	void HandleCoreHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION()
	void HandleCoreDestroyed();

	UFUNCTION()
	void HandleBuildPhaseStarted(int32 NextWaveNumber, float BuildSeconds);

	UFUNCTION()
	void HandleWaveStarted(const FNightlightWaveInfo& Info);

	UFUNCTION()
	void HandleEnemiesRemainingChanged(int32 EnemiesRemaining);

	UFUNCTION()
	void HandleWaveCleared(const FNightlightWaveSummary& Summary);

	UFUNCTION()
	void HandleEnemySpawned(ANightlightEnemy* Enemy);

	UFUNCTION()
	void HandleCardClicked(int32 CardIndex);

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandleLampClicked();

	UFUNCTION()
	void HandleSpeedClicked();
};
