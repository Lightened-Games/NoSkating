// Copyright Plaza Duty. All Rights Reserved.
// Boxless broadcast-style HUD built in C++ (design guide 8.7.0 recipe, adapted from the accepted Shift Supervisor HUD):
// soft edge scrims, large light numerals, small wide-tracked caps labels, hairline progress, a quiet legend.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PDTypes.h"
#include "PDHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UVerticalBox;
class UImage;
class UTextBlock;
class UFontFace;
struct FCompositeFont;

UENUM()
enum class EPDHudFace : uint8 { Light, Medium };

/**
 * Plaza Duty HUD. Four corner elements in play: objective (top-left), closing clock (top-right), wear +
 * knobs (bottom-right), key legend (bottom-left). Everything else is contextual and in-world: line name
 * tags, "+4" wear popups, prop trick tags, the E prompt over the focused target and one-time teaching tags.
 * One banner at a time on a soft band. Listens to the match event bus only.
 */
UCLASS()
class PLAZADUTY_API UPDHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleState(EPDMatchState NewState);
	UFUNCTION() void HandleHUDVisible(bool bVisible);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandlePhase(int32 PhaseIndex, const FPDPhaseRow& Phase);
	UFUNCTION() void HandleTrick(const FPDTrickEvent& Trick);
	UFUNCTION() void HandleCapped(AActor* Line, int32 KnobsLeft);
	UFUNCTION() void HandleKnockdown(AActor* Guard, AActor* Skater);
	UFUNCTION() void HandlePrompt(EPDPrompt Prompt, AActor* Anchor);
	UFUNCTION() void HandleClosing();
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);

	// ---- Palette (art bible) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor TextColour = FLinearColor(0.86f, 0.87f, 0.85f, 0.92f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor DimColour = FLinearColor(0.86f, 0.87f, 0.85f, 0.55f);
	/** Safety amber: the guard's own things (bolt, knobs, prompts, legend highlight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor AccentColour = FLinearColor(0.578f, 0.871f, 0.045f, 1.0f);  // hi-vis safety lime #C8F03C (D25)
	/** Danger red: wear near the limit, big prop tricks, knocked down, fired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor DangerColour = FLinearColor(0.78f, 0.07f, 0.08f, 1.0f);
	/** Chalk blue: the skate lines (matches the lane glow in the world). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor LineColour = FLinearColor(0.16f, 0.55f, 0.69f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor ScrimColour = FLinearColor(0.004f, 0.006f, 0.01f, 0.62f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor TrackColour = FLinearColor(1.0f, 1.0f, 1.0f, 0.16f);

	// ---- Type: one OFL family (Barlow), two weights, three sizes ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") TSoftObjectPtr<UFontFace> LightFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") TSoftObjectPtr<UFontFace> MediumFace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") int32 LabelSize = 21;  // >= 28 px at 1080p (design guide 2.10, round 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") int32 BodySize = 22;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") int32 HeadlineSize = 52;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") int32 LabelTracking = 220;
	/** Minimum screen gap between world labels (px); overlapping farther labels hide for that frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float TagGap = 6.0f;
	/** Horizontal centre of the victory / defeat result block (0-1 of the screen width). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float EndAnchorX = 0.5f;
	/** End report: screen margin on each side (fraction) and type scale vs the play HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float EndMarginX = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float EndMarginY = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float EndScale = 1.35f;
	/** Size multiplier for the condensed display face (Big Shoulders) so it matches the body face's visual size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") float DisplayFaceScale = 1.3f;
	/** End card: panel behind the shift report, the full-screen dim under it, and the stat number size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") FLinearColor EndPanelColour = FLinearColor(0.012f, 0.014f, 0.018f, 0.86f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Palette") float EndDimOpacity = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") int32 EndStatSize = 40;
	/** Text shadow: stronger for the bright golden-hour facades behind the top-left labels (round 3, 2026-11-10). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type") FVector2D TextShadowOffset = FVector2D(1.0f, 2.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Type", meta = (ClampMin = "0", ClampMax = "1")) float TextShadowOpacity = 0.8f;

	// ---- Layout ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float EdgeMargin = 44.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float TopScrimHeight = 190.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float BottomScrimHeight = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float WearLineWidth = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float BannerAnchorY = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float TagHeight = 150.0f;

	// ---- Motion (150-250 ms ease-out) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float FadeSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float ObjectiveBannerSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float PhaseBannerSeconds = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float TeachSeconds = 3.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float PopupSeconds = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float PopupRise = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float KeyHighlightSeconds = 0.3f;
	/** The legend dims to half after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float LegendDimAfter = 10.0f;
	/** Width of the key column in the legend (fits the widest key, MOUSE, with a gap before the label). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float LegendKeyWidth = 86.0f;
	/** Bottom offset of the wear / knobs column (above the kit's autoplay hint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float WearColumnBottom = 72.0f;
	/** FactoryKit draws the legend and the autoplay hint; the game's own copies stay off unless the kit is removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") bool bShowOwnLegend = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") bool bShowAutoplayTag = false;

private:
	struct FLine
	{
		TObjectPtr<UImage> Fill;
		TObjectPtr<UImage> Rest;
		float Width = 100.0f;
		float Thickness = 2.0f;
	};

	struct FLegendRow
	{
		FName Id;
		TObjectPtr<UTextBlock> Key;
		TObjectPtr<UTextBlock> Label;
		float Highlight = 0.0f;
	};

	struct FTag
	{
		TObjectPtr<UVerticalBox> Box;
		TObjectPtr<UTextBlock> Text;
		TObjectPtr<UImage> Rule;
		TWeakObjectPtr<AActor> Anchor;
		FVector World = FVector::ZeroVector;
		FVector Pinned = FVector::ZeroVector;  // world point it was pinned to this frame (declutter ranking)
		float Age = 0.0f;
		float Life = 0.0f;
		float Opacity = 0.0f;
	};

	void BuildTree();
	/** Hide line / prop labels that would overlap a nearer one or a prompt (round 3: bigger text collided). */
	void DeclutterTags();
	void BuildScrims();
	void BuildCorners();
	void BuildLegend();
	void BuildBanner();
	void BuildEndScreen();
	void BuildTags();
	void EnsureFonts(const FSlateFontInfo& EngineDefault);
	void AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY = 0.0f);
	UTextBlock* Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EPDHudFace Face, int32 Tracking = 0);
	UImage* Blk(const FLinearColor& Colour, const FVector2D& Size);
	UHorizontalBox* MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour);
	void SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const;
	UCanvasPanelSlot* Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);
	FTag MakeTag(int32 TextSize, EPDHudFace Face, const FLinearColor& RuleColour);

	void Bind();
	void ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Colour, float Seconds);
	void ShowTeach(const FText& Text, AActor* Anchor, const FVector& World, const FLinearColor& Colour);
	void UpdateCorners(float Dt);
	void UpdateBanner(float Dt);
	void UpdateLegend(float Dt);
	void UpdateWorld(float Dt);
	void UpdateEnd(float Dt);
	bool Project(const FVector& World, FVector2D& Out) const;
	void PinTag(FTag& Tag, const FVector& World, float Opacity);
	FString Clock(float Seconds) const;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TArray<TObjectPtr<UFontFace>> FontFaces;
	TSharedPtr<FCompositeFont> LightFont;
	TSharedPtr<FCompositeFont> MediumFont;
	bool bFontsBuilt = false;

	// Corners
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ObjectiveBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WearText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WearLimitText;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> KnobDots;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AutoplayText;
	FLine WearLine;
	FLine WhistleLine;
	TArray<FLegendRow> Legend;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> LegendBox;

	// Banner
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BannerBand;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> BannerBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY(Transient) TObjectPtr<UImage> BannerRule;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerSub;

	// End screen
	UPROPERTY(Transient) TObjectPtr<UImage> Dim;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EndBox;
	UPROPERTY(Transient) TObjectPtr<UWidget> ClockColumn;
	UPROPERTY(Transient) TObjectPtr<UWidget> WearColumn;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndReason;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndPrompt;
	/** Shift-report card (reviewer round 4: "the ending card could be better looking"). */
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndKicker;
	UPROPERTY(Transient) TObjectPtr<UImage> EndRule;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> EndStripe;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> EndStatValues;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Vignette;

	// World tags
	TArray<FTag> LineTags;
	TArray<FTag> PropTags;
	TArray<FTag> Popups;
	TArray<FTag> TeachTags;
	FTag FocusTag;
	int32 NextPopup = 0;
	int32 NextTeach = 0;

	float BannerLeft = 0.0f;
	float BannerAge = 0.0f;
	float BannerTotal = 0.0f;
	float StateAge = 0.0f;
	float VignetteAlpha = 0.0f;
	float WearPop = 0.0f;
	float KnobPop = 0.0f;
	float DisplayedWear = 0.0f;
	float ActiveAge = 0.0f;
	float ObjectiveShrink = 0.0f;
	EPDMatchState LastState = EPDMatchState::Ready;
	bool bBound = false;
};
