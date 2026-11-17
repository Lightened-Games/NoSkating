// FactoryKit legend: quiet vertical key list bottom-left (key + label, soft shadow, no boxes); a row lights up a little when
// its action fires or one of its keys is pressed. Doubles as the visible input in the submission video (user, 2026-11-09).
// Also shows the small "autoplay - any key to take over" hint bottom-right while the bot drives.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactoryKitSettings.h"
#include "FactoryLegendWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

UCLASS()
class UFactoryLegendWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRows(const TArray<FFactoryLegendRow>& InRows);
	void Pulse(FName ActionId);
	void PulseKey(const FKey& Key);
	void SetAutoplayHint(bool bVisible);
	void SetLegendVisible(bool bVisible);
	/** Menu placement: the corner opposite the menu, above its key hint, with the optional heading. */
	void SetMenuPlacement(bool bMenu);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	struct FRowWidgets { FFactoryLegendRow Data; UTextBlock* Key = nullptr; UTextBlock* Label = nullptr; float Glow = 0.0f; };

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UTextBlock> Hint;
	UPROPERTY() TObjectPtr<UTextBlock> Heading;
	UPROPERTY() TObjectPtr<UTextBlock> MenuKeys;
	UPROPERTY() TObjectPtr<class UVerticalBoxSlot> HeadingSlot;
	UPROPERTY() TObjectPtr<class UVerticalBoxSlot> RowsSlot;
	UPROPERTY() TObjectPtr<class UVerticalBoxSlot> MenuKeysSlot;
	UPROPERTY() TObjectPtr<class UCanvasPanelSlot> ListSlot;
	bool bMenuPlacement = false;
	void ApplyPlacement();
	TArray<FRowWidgets> Rows;
	TArray<FFactoryLegendRow> Pending;
	bool bHint = false;
	float HintTime = 0.0f;
	float HintAlpha = 0.0f;
	bool bLegendVisible = true;
	float LegendAlpha = 0.0f;
};
