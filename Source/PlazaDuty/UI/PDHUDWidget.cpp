// Copyright Plaza Duty. All Rights Reserved.

#include "UI/PDHUDWidget.h"
#include "Interfaces/PDInterfaces.h"
#include "Framework/PDMatchEvents.h"
#include "Components/PDGuardComponents.h"
#include "Data/PDMatchConfig.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "PlazaDuty"

namespace
{
	void SetImageSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}

	template <typename SlotT>
	SlotT* Pad(SlotT* Slot, const FMargin& Margin)
	{
		if (Slot) { Slot->SetPadding(Margin); }
		return Slot;
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }

	float EaseOut(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return 1.0f - (1.0f - T) * (1.0f - T); }

	const TCHAR* Dot = TEXT("  ·  ");
}

// ---------------------------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------------------------

void UPDHUDWidget::AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY)
{
	// Nested slices accumulate into a smooth falloff with no seams (Shift Supervisor AddFade).
	const int32 N = FMath::Max(1, Slices);
	const float SliceAlpha = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Colour.A, 0.0f, 0.99f), 1.0f / N);
	for (int32 i = 0; i < N; ++i)
	{
		UImage* Slice = Blk(FLinearColor(Colour.R, Colour.G, Colour.B, SliceAlpha), FVector2D(4.0f));
		Slice->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Slice);
		if (!S)
		{
			continue;
		}
		const float Depth = Extent * (i + 1) / N;
		switch (Edge)
		{
		case 0: S->SetAnchors(FAnchors(0, 0, 1, 0)); S->SetOffsets(FMargin(0, 0, 0, Depth)); break;
		case 1: S->SetAnchors(FAnchors(0, 1, 1, 1)); S->SetOffsets(FMargin(0, -Depth, 0, Depth)); break;
		case 2: S->SetAnchors(FAnchors(0, 0, 0, 1)); S->SetOffsets(FMargin(0, 0, Depth, 0)); break;
		case 3: S->SetAnchors(FAnchors(1, 0, 1, 1)); S->SetOffsets(FMargin(-Depth, 0, Depth, 0)); break;
		default: S->SetAnchors(FAnchors(0, BandAnchorY, 1, BandAnchorY)); S->SetOffsets(FMargin(0, -Depth * 0.5f, 0, Depth)); break;
		}
		if (Out)
		{
			Out->Add(Slice);
		}
	}
}

void UPDHUDWidget::EnsureFonts(const FSlateFontInfo& EngineDefault)
{
	if (bFontsBuilt)
	{
		return;
	}
	bFontsBuilt = true;
	const UFont* Roboto = Cast<UFont>(EngineDefault.FontObject);
	const FCompositeFont* RobotoComposite = Roboto ? Roboto->GetCompositeFont() : nullptr;
	auto Build = [this, RobotoComposite](const TSoftObjectPtr<UFontFace>& FaceRef) -> TSharedPtr<FCompositeFont>
	{
		UFontFace* Face = FaceRef.LoadSynchronous();
		if (!Face)
		{
			return nullptr;
		}
		FontFaces.Add(Face);
		TSharedPtr<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
		Entry.Font = FFontData(Face);
		if (RobotoComposite)
		{
			Composite->FallbackTypeface.Typeface = RobotoComposite->DefaultTypeface;
		}
		return Composite;
	};
	LightFont = Build(LightFace);
	MediumFont = Build(MediumFace);
}

UTextBlock* UPDHUDWidget::Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EPDHudFace Face, int32 Tracking)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	EnsureFonts(Font);
	const TSharedPtr<FCompositeFont>& Composite = Face == EPDHudFace::Light ? LightFont : MediumFont;
	// Big Shoulders is condensed: scale it so it reads at the size the layout was tuned for.
	if (Face != EPDHudFace::Light)
	{
		Size = FMath::RoundToInt(Size * DisplayFaceScale);
	}
	if (Composite.IsValid())
	{
		Font = FSlateFontInfo(Composite, Size, TEXT("Regular"));
	}
	else
	{
		Font.TypefaceFontName = Face == EPDHudFace::Light ? FName(TEXT("Light")) : FName(TEXT("Regular"));
	}
	Font.Size = Size;
	Font.LetterSpacing = Tracking;
	Font.OutlineSettings.OutlineSize = 0;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	// Soft 1-2 px shadow for contrast over the plaza (never an outline or a box).
	Block->SetShadowOffset(TextShadowOffset);
	Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, TextShadowOpacity));
	return Block;
}

UImage* UPDHUDWidget::Blk(const FLinearColor& Colour, const FVector2D& Size)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.ImageSize = Size;
	Img->SetBrush(Brush);
	Img->SetColorAndOpacity(Colour);
	return Img;
}

UHorizontalBox* UPDHUDWidget::MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Out.Width = Width;
	Out.Thickness = Thickness;
	Out.Fill = Blk(FillColour, FVector2D(Width * 0.5f, Thickness));
	Out.Rest = Blk(TrackColour, FVector2D(Width * 0.5f, Thickness));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Fill)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Rest)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void UPDHUDWidget::SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	SetImageSize(Line.Fill, FVector2D(FMath::Max(0.0f, Line.Width * A), Line.Thickness));
	SetImageSize(Line.Rest, FVector2D(FMath::Max(0.0f, Line.Width * (1.0f - A)), Line.Thickness));
	if (Line.Fill) { Line.Fill->SetColorAndOpacity(FillColour); }
}

UCanvasPanelSlot* UPDHUDWidget::Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
{
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Widget);
	if (S)
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return S;
}

UPDHUDWidget::FTag UPDHUDWidget::MakeTag(int32 TextSize, EPDHudFace Face, const FLinearColor& RuleColour)
{
	FTag Tag;
	Tag.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Tag.Text = Txt(FText::GetEmpty(), TextSize, TextColour, Face, TextSize <= LabelSize ? LabelTracking : 0);
	Tag.Text->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Tag.Box->AddChildToVerticalBox(Tag.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
	Tag.Rule = Blk(RuleColour, FVector2D(34.0f, 2.0f));
	if (UVerticalBoxSlot* S = Pad(Tag.Box->AddChildToVerticalBox(Tag.Rule), FMargin(0.0f, 3.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Center); }
	Tag.Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	Tag.Box->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Tag.Box))
	{
		S->SetAlignment(FVector2D(0.5f, 1.0f));
		S->SetAutoSize(true);
	}
	return Tag;
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> UPDHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPDHUDWidget::BuildTree()
{
	if (LightFace.IsNull())
	{
		LightFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/PlazaDuty/UI/Fonts/FF_PublicSans_Light.FF_PublicSans_Light")));
	}
	if (MediumFace.IsNull())
	{
		MediumFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/PlazaDuty/UI/Fonts/FF_BigShoulders_SemiBold.FF_BigShoulders_SemiBold")));
	}
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	BuildScrims();
	BuildTags();
	BuildCorners();
	BuildLegend();
	if (!bShowOwnLegend && LegendBox)
	{
		LegendBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	BuildBanner();
	BuildEndScreen();
}

void UPDHUDWidget::BuildScrims()
{
	AddFade(0, TopScrimHeight, ScrimColour, 28, nullptr);
	AddFade(1, BottomScrimHeight, ScrimColour, 28, nullptr);
	const FLinearColor Red(0.75f, 0.02f, 0.02f, 0.65f);
	AddFade(2, 240.0f, Red, 18, &Vignette);
	AddFade(3, 240.0f, Red, 18, &Vignette);
	AddFade(0, 160.0f, Red, 14, &Vignette);
	AddFade(1, 160.0f, Red, 14, &Vignette);
	for (UImage* Img : Vignette)
	{
		Img->SetRenderOpacity(0.0f);
	}
}

void UPDHUDWidget::BuildCorners()
{
	// Top-left: the objective (the banner shrinks into it after the countdown).
	ObjectiveBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ObjectiveBox->AddChildToVerticalBox(Txt(LOCTEXT("Brand", "PLAZA DUTY"), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking));
	ObjectiveText = Txt(FText::GetEmpty(), BodySize, TextColour, EPDHudFace::Medium);
	Pad(ObjectiveBox->AddChildToVerticalBox(ObjectiveText), FMargin(0.0f, 4.0f, 0.0f, 2.0f));
	PhaseText = Txt(FText::GetEmpty(), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking);
	Pad(ObjectiveBox->AddChildToVerticalBox(PhaseText), FMargin(0.0f, 6.0f, 0.0f, 0.0f));  // air between the goal and the phase
	Place(ObjectiveBox, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(EdgeMargin, 36.0f));

	// Top-right: time to closing.
	UVerticalBox* ClockCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ClockLabel = Txt(LOCTEXT("ClosingLabel", "CLOSING IN"), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking);
	if (UVerticalBoxSlot* S = ClockCol->AddChildToVerticalBox(ClockLabel)) { S->SetHorizontalAlignment(HAlign_Right); }
	ClockText = Txt(FText::FromString(TEXT("1:30")), HeadlineSize, TextColour, EPDHudFace::Light);
	if (UVerticalBoxSlot* S = Pad(ClockCol->AddChildToVerticalBox(ClockText), FMargin(0.0f, -8.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	Place(ClockCol, FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-EdgeMargin, 36.0f));
	ClockColumn = ClockCol;

	// Bottom-right: wear and the knob box.
	UVerticalBox* WearCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = WearCol->AddChildToVerticalBox(Txt(LOCTEXT("WearLabel", "PLAZA WEAR"), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking))) { S->SetHorizontalAlignment(HAlign_Right); }
	UHorizontalBox* WearRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WearText = Txt(FText::FromString(TEXT("0")), HeadlineSize, TextColour, EPDHudFace::Light);
	WearRow->AddChildToHorizontalBox(WearText);
	WearLimitText = Txt(FText::FromString(TEXT("/ 100")), BodySize, DimColour, EPDHudFace::Light);
	if (UHorizontalBoxSlot* S = Pad(WearRow->AddChildToHorizontalBox(WearLimitText), FMargin(8.0f, 0.0f, 0.0f, 12.0f))) { S->SetVerticalAlignment(VAlign_Bottom); }
	if (UVerticalBoxSlot* S = Pad(WearCol->AddChildToVerticalBox(WearRow), FMargin(0.0f, -8.0f, 0.0f, 4.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	if (UVerticalBoxSlot* S = WearCol->AddChildToVerticalBox(MakeLine(WearLine, WearLineWidth, 2.0f, AccentColour))) { S->SetHorizontalAlignment(HAlign_Right); }
	UHorizontalBox* KnobRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Pad(KnobRow->AddChildToHorizontalBox(Txt(LOCTEXT("KnobLabel", "KNOBS"), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking)), FMargin(0.0f, 0.0f, 10.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
	for (int32 i = 0; i < 3; ++i)
	{
		UImage* D = Blk(AccentColour, FVector2D(8.0f, 8.0f));
		D->SetRenderTransformAngle(45.0f);
		if (UHorizontalBoxSlot* S = Pad(KnobRow->AddChildToHorizontalBox(D), FMargin(6.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		KnobDots.Add(D);
	}
	if (UVerticalBoxSlot* S = Pad(WearCol->AddChildToVerticalBox(KnobRow), FMargin(0.0f, 12.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	// Clear of FactoryKit's bottom-right autoplay hint.
	Place(WearCol, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-EdgeMargin, -WearColumnBottom));
	WearColumn = WearCol;

	AutoplayText = Txt(LOCTEXT("AutoplayTag", "AUTOPLAY"), LabelSize, DimColour, EPDHudFace::Medium, LabelTracking);
	AutoplayText->SetVisibility(ESlateVisibility::Collapsed);
	Place(AutoplayText, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -18.0f));
}

void UPDHUDWidget::BuildLegend()
{
	struct FRowDef { FName Id; FText Key; FText Label; };
	const TArray<FRowDef> Rows = {
		{ PD::KeyMove, LOCTEXT("KeyMove", "WASD"), LOCTEXT("RowMove", "Move") },
		{ PD::KeyLook, LOCTEXT("KeyLook", "MOUSE"), LOCTEXT("RowLook", "Look") },
		{ PD::KeyRun, LOCTEXT("KeyRun", "SHIFT"), LOCTEXT("RowRun", "Run") },
		{ PD::KeyInteract, LOCTEXT("KeyE", "E"), LOCTEXT("RowE", "Bolt a knob  /  drag a prop") },
		{ PD::KeyWhistle, LOCTEXT("KeyQ", "Q"), LOCTEXT("RowQ", "Whistle") },
		{ PD::KeyRestart, LOCTEXT("KeyR", "R"), LOCTEXT("RowR", "Restart") },
	};
	LegendBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FRowDef& Def : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Key = Txt(Def.Key, LabelSize, TextColour, EPDHudFace::Medium, 160);
		USizeBox* KeyCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyCol->SetWidthOverride(LegendKeyWidth);
		KeyCol->SetContent(Key);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(KeyCol)) { S->SetVerticalAlignment(VAlign_Center); }
		UTextBlock* Label = Txt(Def.Label, LabelSize, DimColour, EPDHudFace::Medium, 20);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Label)) { S->SetVerticalAlignment(VAlign_Center); }
		if (Def.Id == PD::KeyWhistle)
		{
			// Recharge: a hairline after the label.
			if (UHorizontalBoxSlot* S = Pad(Line->AddChildToHorizontalBox(MakeLine(WhistleLine, 46.0f, 2.0f, AccentColour)), FMargin(12.0f, 2.0f, 0.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		}
		Pad(LegendBox->AddChildToVerticalBox(Line), FMargin(0.0f, 3.0f));
		Legend.Add({ Def.Id, Key, Label, 0.0f });
	}
	Place(LegendBox, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(EdgeMargin, -40.0f));
}

void UPDHUDWidget::BuildBanner()
{
	AddFade(4, 220.0f, FLinearColor(0.0f, 0.004f, 0.01f, 0.6f), 26, &BannerBand, BannerAnchorY);
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BannerTitle = Txt(FText::GetEmpty(), HeadlineSize, TextColour, EPDHudFace::Medium, 260);
	BannerTitle->SetJustification(ETextJustify::Center);
	BannerRule = Blk(AccentColour, FVector2D(56.0f, 2.0f));
	BannerSub = Txt(FText::GetEmpty(), BodySize, TextColour, EPDHudFace::Light, 20);
	BannerSub->SetJustification(ETextJustify::Center);
	const TPair<UWidget*, FMargin> Parts[] = {
		{ BannerTitle.Get(), FMargin(0.0f) },
		{ BannerRule.Get(), FMargin(0.0f, 6.0f, 0.0f, 10.0f) },
		{ BannerSub.Get(), FMargin(0.0f) },
	};
	for (const TPair<UWidget*, FMargin>& Part : Parts)
	{
		if (UVerticalBoxSlot* S = BannerBox->AddChildToVerticalBox(Part.Key))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(Part.Value);
		}
	}
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerBox->SetRenderOpacity(0.0f);
	for (UImage* Slice : BannerBand) { Slice->SetRenderOpacity(0.0f); }
	Place(BannerBox, FAnchors(0.5f, BannerAnchorY), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UPDHUDWidget::BuildEndScreen()
{
	Dim = Blk(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FVector2D(8.0f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	Dim->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}
	// Shift report card: hi-vis hazard tape across the top, a dark panel, kicker, title, rule, reason, four stat
	// cells and the restart prompt (one palette, one font family, same motif as the guard's vest).
	EndBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Tape = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 i = 0; i < 22; ++i)
	{
		UImage* Seg = Blk(i % 2 ? EndPanelColour : AccentColour, FVector2D(8.0f, 10.0f));
		EndStripe.Add(Seg);
		if (UHorizontalBoxSlot* S = Tape->AddChildToHorizontalBox(Seg))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (UVerticalBoxSlot* S = EndBox->AddChildToVerticalBox(Tape))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}
	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = Card->AddChildToOverlay(Blk(EndPanelColour, FVector2D(8.0f))))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* S = Card->AddChildToOverlay(Inner))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* S = EndBox->AddChildToVerticalBox(Card))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	// Bottom tape too, so the full-screen card reads as one framed report.
	UHorizontalBox* TapeLow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 i = 0; i < 22; ++i)
	{
		UImage* Seg = Blk(i % 2 ? EndPanelColour : AccentColour, FVector2D(8.0f, 10.0f));
		EndStripe.Add(Seg);
		if (UHorizontalBoxSlot* S = TapeLow->AddChildToHorizontalBox(Seg))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	if (UVerticalBoxSlot* S = EndBox->AddChildToVerticalBox(TapeLow))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}

	// User 2026-11-12: "shift the end screen into the middle and remove the other hud and maximize screen coverage".
	const auto Big = [this](int32 Size) { return FMath::RoundToInt(Size * EndScale); };
	EndKicker = Txt(LOCTEXT("EndKicker", "END OF SHIFT REPORT"), Big(LabelSize), AccentColour, EPDHudFace::Medium, LabelTracking);
	EndTitle = Txt(FText::GetEmpty(), Big(HeadlineSize), TextColour, EPDHudFace::Medium, 200);
	EndRule = Blk(AccentColour, FVector2D(72.0f * EndScale, 4.0f * EndScale));
	EndReason = Txt(FText::GetEmpty(), Big(BodySize + 6), TextColour, EPDHudFace::Light, 10);
	UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FText StatNames[] = { LOCTEXT("StatWear", "WEAR"), LOCTEXT("StatTricks", "TRICKS"), LOCTEXT("StatKnobs", "KNOBS"), LOCTEXT("StatWhistles", "WHISTLES") };
	for (int32 i = 0; i < 4; ++i)
	{
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* Value = Txt(FText::GetEmpty(), Big(EndStatSize), TextColour, EPDHudFace::Medium, 40);
		EndStatValues.Add(Value);
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Value)) { S->SetHorizontalAlignment(HAlign_Center); }
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Txt(StatNames[i], Big(LabelSize), DimColour, EPDHudFace::Medium, LabelTracking)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		const float Gap = 26.0f * EndScale;
		if (UHorizontalBoxSlot* S = Stats->AddChildToHorizontalBox(Cell))
		{
			S->SetPadding(FMargin(i == 0 ? 0.0f : Gap, 0.0f, i == 3 ? 0.0f : Gap, 0.0f));
		}
		if (i < 3)
		{
			if (UHorizontalBoxSlot* S = Stats->AddChildToHorizontalBox(Blk(TrackColour, FVector2D(2.0f, 64.0f * EndScale))))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
	EndPrompt = Txt(FText::GetEmpty(), Big(LabelSize), AccentColour, EPDHudFace::Medium, LabelTracking);
	const float K = EndScale;
	const TPair<UWidget*, FMargin> Parts[] = {
		{ EndKicker.Get(), FMargin(0.0f, 0.0f, 0.0f, 6.0f * K) },
		{ EndTitle.Get(), FMargin(0.0f) },
		{ EndRule.Get(), FMargin(0.0f, 14.0f * K, 0.0f, 14.0f * K) },
		{ EndReason.Get(), FMargin(0.0f, 0.0f, 0.0f, 30.0f * K) },
		{ Stats, FMargin(0.0f, 0.0f, 0.0f, 34.0f * K) },
		{ EndPrompt.Get(), FMargin(0.0f) },
	};
	for (const TPair<UWidget*, FMargin>& Part : Parts)
	{
		if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(Part.Key))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(Part.Value);
		}
	}
	EndBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	EndBox->SetRenderOpacity(0.0f);
	EndBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	// Centred and covering most of the screen (EndCoverage of each edge left as margin).
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(EndBox))
	{
		S->SetAnchors(FAnchors(EndMarginX, EndMarginY, 1.0f - EndMarginX, 1.0f - EndMarginY));
		S->SetOffsets(FMargin(0.0f));
	}
}

void UPDHUDWidget::BuildTags()
{
	for (int32 i = 0; i < 6; ++i)
	{
		LineTags.Add(MakeTag(LabelSize, EPDHudFace::Medium, LineColour));
	}
	for (int32 i = 0; i < 12; ++i)
	{
		PropTags.Add(MakeTag(LabelSize, EPDHudFace::Medium, DangerColour));
	}
	for (int32 i = 0; i < 14; ++i)
	{
		FTag Pop = MakeTag(BodySize, EPDHudFace::Medium, FLinearColor::Transparent);
		Pop.Rule->SetVisibility(ESlateVisibility::Collapsed);
		Popups.Add(Pop);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		TeachTags.Add(MakeTag(LabelSize, EPDHudFace::Medium, AccentColour));
	}
	FocusTag = MakeTag(LabelSize, EPDHudFace::Medium, AccentColour);
}

// ---------------------------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------------------------

void UPDHUDWidget::Bind()
{
	if (bBound)
	{
		return;
	}
	IPDMatchFlow* Flow = PD::GetFlow(this);
	UPDMatchEvents* Events = Flow ? Flow->GetMatchEvents() : nullptr;
	if (!Events)
	{
		return;
	}
	Events->OnStateChanged.AddDynamic(this, &UPDHUDWidget::HandleState);
	Events->OnCountdown.AddDynamic(this, &UPDHUDWidget::HandleCountdown);
	Events->OnPhaseChanged.AddDynamic(this, &UPDHUDWidget::HandlePhase);
	Events->OnTrickLanded.AddDynamic(this, &UPDHUDWidget::HandleTrick);
	Events->OnLineCapped.AddDynamic(this, &UPDHUDWidget::HandleCapped);
	Events->OnKnockdown.AddDynamic(this, &UPDHUDWidget::HandleKnockdown);
	Events->OnPrompt.AddDynamic(this, &UPDHUDWidget::HandlePrompt);
	Events->OnClosingSoon.AddDynamic(this, &UPDHUDWidget::HandleClosing);
	Events->OnHUDVisible.AddDynamic(this, &UPDHUDWidget::HandleHUDVisible);
	if (IPDGuardInput* Input = Cast<IPDGuardInput>(GetOwningPlayer()))
	{
		Input->GetInputVisualized().AddUniqueDynamic(this, &UPDHUDWidget::HandleInput);
	}
	bBound = true;
	HandleState(Flow->GetMatchState());
}

void UPDHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Bind();
}

void UPDHUDWidget::NativeDestruct()
{
	if (IPDMatchFlow* Flow = PD::GetFlow(this))
	{
		if (UPDMatchEvents* Events = Flow->GetMatchEvents())
		{
			Events->OnStateChanged.RemoveAll(this);
			Events->OnCountdown.RemoveAll(this);
			Events->OnPhaseChanged.RemoveAll(this);
			Events->OnTrickLanded.RemoveAll(this);
			Events->OnLineCapped.RemoveAll(this);
			Events->OnKnockdown.RemoveAll(this);
			Events->OnPrompt.RemoveAll(this);
			Events->OnClosingSoon.RemoveAll(this);
		}
	}
	if (IPDGuardInput* Input = Cast<IPDGuardInput>(GetOwningPlayer()))
	{
		Input->GetInputVisualized().RemoveAll(this);
	}
	bBound = false;
	Super::NativeDestruct();
}

void UPDHUDWidget::ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Colour, float Seconds)
{
	BannerTitle->SetText(Title);
	BannerTitle->SetColorAndOpacity(FSlateColor(TextColour));
	BannerRule->SetColorAndOpacity(Colour);
	BannerSub->SetText(Sub);
	BannerLeft = Seconds;
	BannerTotal = Seconds;
	BannerAge = 0.0f;
}

void UPDHUDWidget::ShowTeach(const FText& Text, AActor* Anchor, const FVector& World, const FLinearColor& Colour)
{
	FTag& Tag = TeachTags[NextTeach++ % TeachTags.Num()];
	Tag.Text->SetText(Text);
	Tag.Text->SetColorAndOpacity(FSlateColor(TextColour));
	Tag.Rule->SetColorAndOpacity(Colour);
	Tag.Anchor = Anchor;
	Tag.World = World;
	Tag.Age = 0.0f;
	Tag.Life = TeachSeconds;
}

void UPDHUDWidget::HandleState(EPDMatchState NewState)
{
	StateAge = 0.0f;
	LastState = NewState;
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const float Limit = Flow ? Flow->GetWearLimit() : 100.0f;
	if (ObjectiveText)
	{
		ObjectiveText->SetText(FText::Format(LOCTEXT("Objective", "Keep the wear under {0} until closing"), FText::AsNumber(FMath::RoundToInt(Limit))));
	}
	if (NewState == EPDMatchState::Ready)
	{
		ObjectiveShrink = 0.0f;
	}
	if (NewState == EPDMatchState::Defeat)
	{
		VignetteAlpha = 1.0f;
	}
	if (NewState == EPDMatchState::Victory || NewState == EPDMatchState::Defeat)
	{
		BannerLeft = 0.0f;
	}
}

void UPDHUDWidget::HandleCountdown(int32 SecondsLeft)
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const float Limit = Flow ? Flow->GetWearLimit() : 100.0f;
	const float Round = Flow ? Flow->GetClockRemaining() : 90.0f;
	const FText Sub = FText::Format(LOCTEXT("ObjectiveSub", "Keep the plaza under {0} wear for {1} until closing"),
		FText::AsNumber(FMath::RoundToInt(Limit)), FText::FromString(Clock(Round)));
	if (SecondsLeft > 0)
	{
		ShowBanner(FText::AsNumber(SecondsLeft), Sub, AccentColour, 1.05f);
	}
	else
	{
		ShowBanner(LOCTEXT("Go", "SHIFT STARTS"), Sub, AccentColour, ObjectiveBannerSeconds);
	}
}

void UPDHUDWidget::HandlePhase(int32 PhaseIndex, const FPDPhaseRow& Phase)
{
	if (!Phase.BannerText.IsEmpty())
	{
		ShowBanner(Phase.Name, Phase.BannerText, PhaseIndex >= 3 ? DangerColour : AccentColour, PhaseBannerSeconds);
	}
}

void UPDHUDWidget::HandleTrick(const FPDTrickEvent& Trick)
{
	FTag& Pop = Popups[NextPopup++ % Popups.Num()];
	const bool bBig = Trick.Wear >= 7.0f;
	Pop.Text->SetText(FText::FromString(FString::Printf(TEXT("+%d"), FMath::RoundToInt(Trick.Wear))));
	Pop.Text->SetColorAndOpacity(FSlateColor(bBig ? DangerColour : TextColour));
	Pop.World = Trick.Location + FVector(0.0f, 0.0f, 90.0f);
	Pop.Anchor = nullptr;
	Pop.Age = 0.0f;
	Pop.Life = PopupSeconds;
	WearPop = 1.0f;
}

void UPDHUDWidget::HandleCapped(AActor* Line, int32 KnobsLeft)
{
	KnobPop = 1.0f;
}

void UPDHUDWidget::HandleKnockdown(AActor* Guard, AActor* Skater)
{
	VignetteAlpha = 1.0f;
}

void UPDHUDWidget::HandlePrompt(EPDPrompt Prompt, AActor* Anchor)
{
	const FVector Where = Anchor ? Anchor->GetActorLocation() : FVector::ZeroVector;
	switch (Prompt)
	{
	case EPDPrompt::Bolt:
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(Anchor);
		ShowTeach(LOCTEXT("TeachBolt", "HOLD E BY THE KNOB  ·  CAPS THE LEDGE FOR GOOD"), nullptr, L ? L->GetKnobPoint() + FVector(0.0f, 0.0f, L->GetFeatureHeight() + 60.0f) : Where, AccentColour);
		break;
	}
	case EPDPrompt::Drag:
		ShowTeach(LOCTEXT("TeachDrag", "E  DRAG IT INTO THEIR LINE"), Anchor, Where + FVector(0.0f, 0.0f, 60.0f), AccentColour);
		break;
	case EPDPrompt::Converted:
		ShowTeach(LOCTEXT("TeachConverted", "THEY WORKED IT OUT  ·  NOW THEY RUN IT"), Anchor, Where + FVector(0.0f, 0.0f, 120.0f), DangerColour);
		break;
	case EPDPrompt::Friend:
		ShowTeach(LOCTEXT("TeachFriend", "+1 RUNNER  ·  BIG MOVES DRAW A CROWD"), Anchor, Where + FVector(0.0f, 0.0f, 120.0f), DangerColour);
		break;
	case EPDPrompt::Whistle:
		ShowTeach(LOCTEXT("TeachWhistle", "Q  WHISTLE TO SCATTER THEM"), Anchor, Where + FVector(0.0f, 0.0f, 60.0f), AccentColour);
		break;
	case EPDPrompt::Knockdown:
		ShowTeach(LOCTEXT("TeachKnock", "KNOCKED DOWN  ·  BOLT BETWEEN RUNS"), Anchor, Where + FVector(0.0f, 0.0f, 60.0f), DangerColour);
		break;
	case EPDPrompt::Capped:
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(Anchor);
		ShowTeach(LOCTEXT("TeachCapped", "CAPPED  ·  THEY'LL FIND ANOTHER LINE"), nullptr, L ? L->GetKnobPoint() + FVector(0.0f, 0.0f, L->GetFeatureHeight() + 60.0f) : Where, AccentColour);
		break;
	}
	default:
		break;
	}
}

void UPDHUDWidget::HandleClosing()
{
	ShowBanner(LOCTEXT("ClosingTitle", "CLOSING SOON"), LOCTEXT("ClosingSub", "The manager is on the way down"), DangerColour, PhaseBannerSeconds);
}

void UPDHUDWidget::HandleInput(FName KeyId, bool bSimulated)
{
	for (FLegendRow& Row : Legend)
	{
		if (Row.Id == KeyId)
		{
			Row.Highlight = 1.0f;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------------------------

FString UPDHUDWidget::Clock(float Seconds) const
{
	const int32 Whole = FMath::CeilToInt(FMath::Max(0.0f, Seconds));
	return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
}

bool UPDHUDWidget::Project(const FVector& World, FVector2D& Out) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !UGameplayStatics::ProjectWorldToScreen(PC, World, Out, false))
	{
		return false;
	}
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	Out /= Scale;
	return true;
}

void UPDHUDWidget::PinTag(FTag& Tag, const FVector& World, float Opacity)
{
	Tag.Pinned = World;
	FVector2D Screen;
	if (Opacity > 0.01f && Project(World, Screen))
	{
		// Keep tags inside the safe area (long tags near the edge were cut off).
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;
		const float HalfWidth = Tag.Box->GetDesiredSize().X * 0.5f;
		if (Screen.X < 0.0f || Screen.X > View.X)
		{
			Tag.Box->SetRenderOpacity(0.0f);
			return;
		}
		if (View.X > 2.0f * (EdgeMargin + HalfWidth))
		{
			Screen.X = FMath::Clamp(Screen.X, EdgeMargin + HalfWidth, View.X - EdgeMargin - HalfWidth);
		}
		// Never over the corner elements: a tag that would sit in the top or bottom HUD band is hidden.
		if (Screen.Y < TopScrimHeight * 0.75f || Screen.Y > View.Y - BottomScrimHeight * 0.6f)
		{
			Tag.Box->SetRenderOpacity(0.0f);
			return;
		}
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Tag.Box->Slot))
		{
			S->SetPosition(Screen);
		}
		Tag.Box->SetRenderOpacity(Opacity);
	}
	else
	{
		Tag.Box->SetRenderOpacity(0.0f);
	}
}

void UPDHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bBound)
	{
		Bind();
	}
	StateAge += InDeltaTime;
	UpdateCorners(InDeltaTime);
	UpdateBanner(InDeltaTime);
	UpdateLegend(InDeltaTime);
	UpdateWorld(InDeltaTime);
	UpdateEnd(InDeltaTime);
}

void UPDHUDWidget::UpdateCorners(float Dt)
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return;
	}
	const EPDMatchState State = Flow->GetMatchState();
	const bool bActive = State == EPDMatchState::Active;
	if (bActive)
	{
		ActiveAge += Dt;
	}

	// The objective line slides in once the banner has had its 3 seconds.
	const bool bShowObjective = State != EPDMatchState::Ready && (ActiveAge > ObjectiveBannerSeconds || !bActive);
	ObjectiveShrink = FMath::FInterpConstantTo(ObjectiveShrink, bShowObjective ? 1.0f : 0.0f, Dt, 1.0f / FadeSeconds);
	ObjectiveText->SetRenderOpacity(EaseOut(ObjectiveShrink));
	ObjectiveText->SetRenderTranslation(FVector2D(0.0f, 10.0f * (1.0f - EaseOut(ObjectiveShrink))));
	PhaseText->SetText(Flow->GetPhase().Name);
	PhaseText->SetRenderOpacity(bActive ? 1.0f : 0.0f);

	// Clock: m:ss, last ten seconds in the danger colour.
	const float Remaining = Flow->GetClockRemaining();
	const bool bLow = bActive && Remaining <= 10.0f;
	ClockText->SetText(FText::FromString(Clock(Remaining)));
	ClockText->SetColorAndOpacity(FSlateColor(bLow ? DangerColour : TextColour));

	// Wear rolls up; turns red near the limit.
	const float Wear = Flow->GetWear();
	const float Limit = FMath::Max(1.0f, Flow->GetWearLimit());
	DisplayedWear = FMath::FInterpTo(DisplayedWear, Wear, Dt, 10.0f);
	if (FMath::Abs(DisplayedWear - Wear) < 0.5f) { DisplayedWear = Wear; }
	const float DangerAt = Flow->GetMatchConfig() ? Flow->GetMatchConfig()->WearDangerFraction : 0.8f;
	const bool bDanger = Wear >= Limit * DangerAt;
	WearPop = FMath::Max(0.0f, WearPop - Dt * 4.0f);
	WearText->SetText(FText::AsNumber(FMath::FloorToInt(DisplayedWear)));
	WearText->SetColorAndOpacity(FSlateColor(bDanger ? DangerColour : TextColour));
	WearText->SetRenderScale(FVector2D(1.0f + 0.08f * WearPop));
	WearLimitText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), FMath::RoundToInt(Limit))));
	SetLine(WearLine, DisplayedWear / Limit, bDanger ? DangerColour : AccentColour);

	// Knobs left.
	KnobPop = FMath::Max(0.0f, KnobPop - Dt * 3.0f);
	const int32 Knobs = Flow->GetKnobsLeft();
	for (int32 i = 0; i < KnobDots.Num(); ++i)
	{
		const bool bLit = i < Knobs;
		KnobDots[i]->SetColorAndOpacity(bLit ? AccentColour : TrackColour);
		KnobDots[i]->SetRenderScale(FVector2D(i == Knobs ? 1.0f + 0.8f * KnobPop : 1.0f));
	}
	AutoplayText->SetVisibility(bShowAutoplayTag && Flow->IsAutoplayRun() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// Whistle recharge hairline.
	const APawn* Pawn = GetOwningPlayerPawn();
	const UPDWhistleComponent* Whistle = Pawn ? Pawn->FindComponentByClass<UPDWhistleComponent>() : nullptr;
	const float Charge = Whistle ? Whistle->GetCharge() : 1.0f;
	SetLine(WhistleLine, Charge, Charge >= 1.0f ? AccentColour : WithAlpha(TextColour, 0.5f));

	// Red edge: knock-downs flash it; it breathes softly while the wear is critical.
	VignetteAlpha = FMath::Max(0.0f, VignetteAlpha - Dt * 1.8f);
	float V = VignetteAlpha;
	if (State == EPDMatchState::Defeat)
	{
		V = FMath::Max(V, 0.35f);
	}
	else if (bActive && Wear >= Limit * 0.9f)
	{
		V = FMath::Max(V, 0.12f + 0.08f * FMath::Sin(StateAge * 3.0f));
	}
	for (UImage* Img : Vignette)
	{
		Img->SetRenderOpacity(V);
	}
}

void UPDHUDWidget::UpdateBanner(float Dt)
{
	float Opacity = 0.0f;
	float Slide = 0.0f;
	float Scale = 1.0f;
	if (BannerLeft > 0.0f)
	{
		BannerLeft -= Dt;
		BannerAge += Dt;
		Opacity = EaseOut(BannerAge / FadeSeconds) * FMath::Clamp(BannerLeft / FadeSeconds, 0.0f, 1.0f);
		Slide = 14.0f * (1.0f - EaseOut(BannerAge / FadeSeconds));
		if (LastState == EPDMatchState::Ready)
		{
			// Countdown digit: a quick settle from big to rest.
			Scale = 1.0f + 0.35f * (1.0f - EaseOut(BannerAge / 0.25f));
		}
	}
	BannerBox->SetRenderOpacity(Opacity);
	BannerBox->SetRenderTranslation(FVector2D(0.0f, Slide));
	BannerTitle->SetRenderScale(FVector2D(Scale));
	for (UImage* Slice : BannerBand)
	{
		Slice->SetRenderOpacity(Opacity);
	}
}

void UPDHUDWidget::UpdateLegend(float Dt)
{
	// The legend dims to half once the player has had time to read it; a used key lights up for a moment.
	const float Base = ActiveAge > LegendDimAfter ? 0.5f : 1.0f;
	for (FLegendRow& Row : Legend)
	{
		Row.Highlight = FMath::Max(0.0f, Row.Highlight - Dt / FMath::Max(0.05f, KeyHighlightSeconds));
		const float H = EaseOut(Row.Highlight);
		Row.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(WithAlpha(TextColour, TextColour.A * Base), AccentColour, H)));
		Row.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(WithAlpha(DimColour, DimColour.A * Base), TextColour, H)));
	}
}

void UPDHUDWidget::UpdateWorld(float Dt)
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return;
	}
	const bool bPlaying = Flow->GetMatchState() == EPDMatchState::Active || Flow->GetMatchState() == EPDMatchState::Ready;

	// Line name tags over each feature (chalk-blue rule = a skate line).
	const TArray<AActor*> Lines = Flow->GetSkateLines();
	for (int32 i = 0; i < LineTags.Num(); ++i)
	{
		FTag& Tag = LineTags[i];
		const IPDSkateLine* L = Lines.IsValidIndex(i) ? Cast<IPDSkateLine>(Lines[i]) : nullptr;
		if (!L || !bPlaying || !L->IsOpen())
		{
			Tag.Box->SetRenderOpacity(0.0f);
			continue;
		}
		FText Text = L->GetLineName();
		float Opacity = 0.85f;
		FLinearColor Rule = LineColour;
		if (L->IsCapped())
		{
			Text = FText::FromString(L->GetLineName().ToString() + Dot + TEXT("CAPPED"));
			Opacity = 0.45f;
			Rule = TrackColour;
		}
		else if (L->IsBlocked())
		{
			Text = FText::FromString(L->GetLineName().ToString() + Dot + TEXT("BLOCKED"));
		}
		Tag.Text->SetText(Text);
		Tag.Rule->SetColorAndOpacity(Rule);
		SetImageSize(Tag.Rule, FVector2D(34.0f + 40.0f * L->GetHeat(), 2.0f));
		const float Mid = 0.5f * (L->GetFeatureStartDistance() + L->GetFeatureEndDistance());
		PinTag(Tag, L->GetPointAtDistance(Mid) + FVector(0.0f, 0.0f, L->GetFeatureHeight() + TagHeight), Opacity);
	}

	// Converted props carry the trick the skaters invented (red rule = it costs you wear).
	const TArray<AActor*> Props = Flow->GetProps();
	for (int32 i = 0; i < PropTags.Num(); ++i)
	{
		FTag& Tag = PropTags[i];
		AActor* PropActor = Props.IsValidIndex(i) ? Props[i] : nullptr;
		const IPDDraggable* Prop = Cast<IPDDraggable>(PropActor);
		if (!Prop || !Prop->IsConverted() || Prop->IsHeld() || !bPlaying)
		{
			Tag.Box->SetRenderOpacity(0.0f);
			continue;
		}
		FText Trick;
		for (AActor* LineActor : Lines)
		{
			if (const IPDSkateLine* L = Cast<IPDSkateLine>(LineActor))
			{
				for (const FPDPropTrick& T : L->GetPropTricks())
				{
					if (T.Prop.Get() == PropActor)
					{
						Trick = FText::FromString(T.TrickName.ToString().ToUpper() + FString::Printf(TEXT("  +%d"), FMath::RoundToInt(T.Wear)));
					}
				}
			}
		}
		Tag.Text->SetText(Trick);
		PinTag(Tag, PropActor->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f), 0.9f);
	}

	// Wear popups rise and fade.
	for (FTag& Pop : Popups)
	{
		if (Pop.Life <= 0.0f)
		{
			Pop.Box->SetRenderOpacity(0.0f);
			continue;
		}
		Pop.Age += Dt;
		const float T = Pop.Age / Pop.Life;
		if (T >= 1.0f)
		{
			Pop.Life = 0.0f;
			Pop.Box->SetRenderOpacity(0.0f);
			continue;
		}
		const float Opacity = EaseOut(Pop.Age / 0.15f) * FMath::Clamp((1.0f - T) * 3.0f, 0.0f, 1.0f);
		PinTag(Pop, Pop.World + FVector(0.0f, 0.0f, PopupRise * EaseOut(T)), Opacity);
		Pop.Box->SetRenderScale(FVector2D(1.0f + 0.25f * (1.0f - EaseOut(Pop.Age / 0.2f))));
	}

	// One-time teaching tags.
	for (FTag& Tag : TeachTags)
	{
		if (Tag.Life <= 0.0f)
		{
			Tag.Box->SetRenderOpacity(0.0f);
			continue;
		}
		Tag.Age += Dt;
		if (Tag.Age >= Tag.Life)
		{
			Tag.Life = 0.0f;
			Tag.Box->SetRenderOpacity(0.0f);
			continue;
		}
		const FVector World = Tag.Anchor.IsValid() ? Tag.Anchor->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f) : Tag.World;
		PinTag(Tag, World, EaseOut(Tag.Age / FadeSeconds) * FMath::Clamp((Tag.Life - Tag.Age) / FadeSeconds, 0.0f, 1.0f));
	}

	// The E prompt over whatever the guard would use right now.
	const APawn* Pawn = GetOwningPlayerPawn();
	const UPDInteractComponent* Interact = Pawn ? Pawn->FindComponentByClass<UPDInteractComponent>() : nullptr;
	const UPDDragComponent* Drag = Pawn ? Pawn->FindComponentByClass<UPDDragComponent>() : nullptr;
	const IPDKnockable* Guard = Cast<IPDKnockable>(Pawn);
	float FocusOpacity = 0.0f;
	FVector FocusWorld = FVector::ZeroVector;
	if (Flow->GetMatchState() == EPDMatchState::Active && Guard)
	{
		if (Drag && Drag->IsDragging() && Drag->GetHeld())
		{
			FocusTag.Text->SetText(LOCTEXT("PromptDrop", "E  DROP"));
			FocusWorld = Drag->GetHeld()->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
			FocusOpacity = 0.8f;
		}
		else if (Guard->GetGuardAction() == EPDGuardAction::None && Interact && Interact->GetFocus())
		{
			const IPDInteractable* I = Cast<IPDInteractable>(Interact->GetFocus());
			FocusTag.Text->SetText(I ? I->GetInteractLabel() : FText::GetEmpty());
			const IPDSkateLine* L = Cast<IPDSkateLine>(Interact->GetFocus());
			FocusWorld = L ? L->GetKnobPoint() + FVector(0.0f, 0.0f, L->GetFeatureHeight() + 60.0f) : Interact->GetFocus()->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
			FocusOpacity = 1.0f;
		}
	}
	// Teaching tags take the spot when both would show.
	for (const FTag& Tag : TeachTags)
	{
		if (Tag.Life > 0.0f)
		{
			FocusOpacity *= 0.0f;
		}
	}
	FocusTag.Opacity = FMath::FInterpConstantTo(FocusTag.Opacity, FocusOpacity, Dt, 1.0f / FadeSeconds);
	if (FocusOpacity > 0.0f)
	{
		FocusTag.World = FocusWorld;
	}
	PinTag(FocusTag, FocusTag.World, FocusTag.Opacity);
	DeclutterTags();
}

void UPDHUDWidget::DeclutterTags()
{
	const float Gap = TagGap;
	auto Rect = [Gap](const FTag& T, FBox2D& Out)
	{
		const UCanvasPanelSlot* S = T.Box ? Cast<UCanvasPanelSlot>(T.Box->Slot) : nullptr;
		if (!S || T.Box->GetRenderOpacity() <= 0.01f)
		{
			return false;
		}
		const FVector2D Size = T.Box->GetDesiredSize();
		const FVector2D Min = S->GetPosition() - S->GetAlignment() * Size;
		Out = FBox2D(Min - FVector2D(Gap), Min + Size + FVector2D(Gap));
		return true;
	};
	TArray<FBox2D> Taken;
	FBox2D R;
	// Prompts and teaching tags always win.
	if (Rect(FocusTag, R)) { Taken.Add(R); }
	for (const FTag& T : TeachTags) { if (Rect(T, R)) { Taken.Add(R); } }
	// Then line and prop labels, nearest to the camera first.
	FVector Eye = FVector::ZeroVector;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		FRotator Unused;
		PC->GetPlayerViewPoint(Eye, Unused);
	}
	TArray<FTag*> Rest;
	for (FTag& T : LineTags) { Rest.Add(&T); }
	for (FTag& T : PropTags) { Rest.Add(&T); }
	Rest.Sort([&Eye](const FTag& A, const FTag& B) { return FVector::DistSquared(A.Pinned, Eye) < FVector::DistSquared(B.Pinned, Eye); });
	for (FTag* T : Rest)
	{
		if (!Rect(*T, R))
		{
			continue;
		}
		bool bHit = false;
		for (const FBox2D& Other : Taken)
		{
			if (Other.Intersect(R)) { bHit = true; break; }
		}
		if (bHit)
		{
			T->Box->SetRenderOpacity(0.0f);
		}
		else
		{
			Taken.Add(R);
		}
	}
}

void UPDHUDWidget::UpdateEnd(float Dt)
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return;
	}
	const EPDMatchState State = Flow->GetMatchState();
	const bool bEnd = State == EPDMatchState::Victory || State == EPDMatchState::Defeat;
	const float Alpha = bEnd ? EaseOut(StateAge / 0.25f) : 0.0f;
	Dim->SetRenderOpacity(Alpha * EndDimOpacity / 0.6f);
	EndBox->SetRenderOpacity(Alpha);
	EndBox->SetRenderTranslation(FVector2D(0.0f, 18.0f * (1.0f - Alpha)));
	EndBox->SetRenderScale(FVector2D(0.96f + 0.04f * Alpha));
	// The play HUD fades out under the report.
	const float HudAlpha = 1.0f - Alpha;
	LegendBox->SetRenderOpacity(HudAlpha);
	UWidget* const HudParts[] = { ObjectiveBox.Get(), ClockColumn.Get(), WearColumn.Get() };
	for (UWidget* W : HudParts)
	{
		if (W) { W->SetRenderOpacity(HudAlpha); }
	}
	if (!bEnd)
	{
		return;
	}
	const bool bWin = State == EPDMatchState::Victory;
	const FPDMatchStats& Stats = Flow->GetMatchStats();
	EndTitle->SetText(bWin ? LOCTEXT("WinTitle", "STILL EMPLOYED") : LOCTEXT("LoseTitle", "YOU'RE FIRED"));
	EndTitle->SetColorAndOpacity(FSlateColor(bWin ? TextColour : DangerColour));
	EndReason->SetText(Flow->GetEndReason());
	const FLinearColor Mood = bWin ? AccentColour : DangerColour;
	EndKicker->SetColorAndOpacity(FSlateColor(Mood));
	EndRule->SetColorAndOpacity(Mood);
	for (int32 i = 0; i < EndStripe.Num(); i += 2)
	{
		EndStripe[i]->SetColorAndOpacity(Mood);
	}
	const int32 Values[] = { FMath::FloorToInt(Stats.FinalWear), Stats.Tricks, Stats.KnobsBolted, Stats.Whistles };
	for (int32 i = 0; i < EndStatValues.Num(); ++i)
	{
		EndStatValues[i]->SetText(FText::AsNumber(Values[i]));
		EndStatValues[i]->SetColorAndOpacity(FSlateColor(i == 0 ? Mood : TextColour));
	}
	EndPrompt->SetText(bWin ? LOCTEXT("WinPrompt", "R  RESTART      ESC  MENU") : LOCTEXT("LosePrompt", "R  TRY AGAIN      ESC  MENU"));
}

void UPDHUDWidget::HandleHUDVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
