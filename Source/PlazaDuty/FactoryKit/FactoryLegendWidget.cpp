#include "FactoryLegendWidget.h"
#include "FactoryKitStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UFactoryLegendWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFactoryLegendWidget::BuildTree()
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegendList"));
	ListSlot = Root->AddChildToCanvas(List);
	ListSlot->SetAutoSize(true);
	Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendHeading"));
	FactoryKitStyle::Apply(Heading, S->TitleTypeface, S->SmallSize, S->AccentColor, 80);
	Heading->SetText(S->LegendMenuHeading);
	HeadingSlot = List->AddChildToVerticalBox(Heading);
	HeadingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	// The key rows as one block, so on the start screen the block can sit centred under the heading.
	UVerticalBox* RowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegendRows"));
	RowsSlot = List->AddChildToVerticalBox(RowsBox);

	Rows.Reset();
	for (const FFactoryLegendRow& R : Pending)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyBox->SetMinDesiredWidth(S->SmallSize * 6.0f); // key column fits "MOUSE" / "SPACE" (PlazaDuty lesson)
		UTextBlock* K = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(K, S->TitleTypeface, S->SmallSize, S->TextColor, 80);
		K->SetText(R.KeyText);
		KeyBox->AddChild(K);
		H->AddChildToHorizontalBox(KeyBox);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(L, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
		L->SetText(R.Label);
		H->AddChildToHorizontalBox(L);
		UVerticalBoxSlot* VS = RowsBox->AddChildToVerticalBox(H);
		VS->SetPadding(FMargin(0.0f, 3.0f));
		Rows.Add({ R, K, L, 0.0f });
	}

	// Start screen: the menu's key hint closes the same centred column (the menu widget hides its own copy).
	MenuKeys = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendMenuKeys"));
	FactoryKitStyle::Apply(MenuKeys, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	MenuKeys->SetText(S->MenuHintText);
	MenuKeysSlot = List->AddChildToVerticalBox(MenuKeys);
	MenuKeysSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	ApplyPlacement();

	Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoplayHint"));
	FactoryKitStyle::Apply(Hint, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	Hint->SetText(S->AutoplayHintText);
	UCanvasPanelSlot* HS = Root->AddChildToCanvas(Hint);
	HS->SetAnchors(FAnchors(1.0f, 1.0f));
	HS->SetAlignment(FVector2D(1.0f, 1.0f));
	HS->SetPosition(FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y));
	HS->SetAutoSize(true);
	Hint->SetRenderOpacity(0.0f);
}

void UFactoryLegendWidget::SetRows(const TArray<FFactoryLegendRow>& InRows)
{
	Pending = InRows; // call before AddToViewport (the flow subsystem does)
}

void UFactoryLegendWidget::Pulse(FName ActionId)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.ActionId == ActionId) { R.Glow = 1.0f; }
	}
}

void UFactoryLegendWidget::PulseKey(const FKey& Key)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.Keys.Contains(Key.GetFName())) { R.Glow = 1.0f; }
	}
}

void UFactoryLegendWidget::SetAutoplayHint(bool bVisible)
{
	if (bVisible && !bHint) { HintTime = 0.0f; }
	bHint = bVisible;
}

void UFactoryLegendWidget::SetLegendVisible(bool bVisible)
{
	bLegendVisible = bVisible;
}

void UFactoryLegendWidget::SetMenuPlacement(bool bMenu)
{
	bMenuPlacement = bMenu;
	ApplyPlacement();
}

void UFactoryLegendWidget::ApplyPlacement()
{
	if (!ListSlot)
	{
		return;
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	// On the start screen: the corner opposite the menu (as the menu's key hint), one hint line above it.
	const bool bRight = bMenuPlacement && S->MenuLayout != EFactoryMenuLayout::RightColumn;
	ListSlot->SetAnchors(FAnchors(bRight ? 1.0f : 0.0f, 1.0f));
	ListSlot->SetAlignment(FVector2D(bRight ? 1.0f : 0.0f, 1.0f));
	ListSlot->SetPosition(FVector2D(bRight ? -S->LegendMargin.X : S->LegendMargin.X, -S->LegendMargin.Y));
	// Start screen: heading, key block and menu keys share one centre line (symmetric, user 2026-11-09). In play: left.
	const EHorizontalAlignment Align = bMenuPlacement ? HAlign_Center : HAlign_Left;
	if (HeadingSlot) { HeadingSlot->SetHorizontalAlignment(Align); }
	if (RowsSlot) { RowsSlot->SetHorizontalAlignment(Align); }
	if (MenuKeysSlot) { MenuKeysSlot->SetHorizontalAlignment(Align); }
	if (Heading)
	{
		Heading->SetVisibility(bMenuPlacement && !S->LegendMenuHeading.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (MenuKeys)
	{
		MenuKeys->SetVisibility(bMenuPlacement ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFactoryLegendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LegendAlpha = FactoryKitStyle::EaseTo(LegendAlpha, bLegendVisible ? 1.0f : 0.0f, InDeltaTime, S->MotionSeconds);
	for (FRowWidgets& R : Rows)
	{
		R.Glow = FMath::Max(0.0f, R.Glow - InDeltaTime / FMath::Max(S->LegendPulseSeconds, 0.05f));
		const float G = FMath::InterpEaseOut(0.0f, 1.0f, R.Glow, 2.0f);
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(S->TextColor, S->AccentColor, G)));
		R.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(bMenuPlacement ? S->TextColor : S->DimColor, S->TextColor, G)));  // on the start screen the list is the lesson: full contrast
		R.Key->SetRenderOpacity(LegendAlpha);
		R.Label->SetRenderOpacity(LegendAlpha);
	}
	if (Heading) { Heading->SetRenderOpacity(LegendAlpha); }
	if (MenuKeys) { MenuKeys->SetRenderOpacity(LegendAlpha); }
	if (Hint)
	{
		HintTime += InDeltaTime;
		const float Target = !bHint ? 0.0f : (HintTime < S->AutoplayHintFullSeconds ? 1.0f : S->AutoplayHintDimAlpha);
		HintAlpha = FactoryKitStyle::EaseTo(HintAlpha, Target, InDeltaTime, S->MotionSeconds);
		Hint->SetRenderOpacity(HintAlpha);
	}
}
