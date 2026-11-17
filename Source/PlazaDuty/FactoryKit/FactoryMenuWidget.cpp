#include "FactoryMenuWidget.h"
#include "Engine/Texture2D.h"
#include "FactoryFlowSubsystem.h"
#include "FactoryKitStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "FactoryKit"

TSharedRef<SWidget> UFactoryMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFactoryMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Opacity = 0.0f;
	SetRenderOpacity(0.0f);
}

void UFactoryMenuWidget::BuildTree()
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	const EFactoryMenuLayout Layout = S->MenuLayout;
	const bool bRight = Layout == EFactoryMenuLayout::RightColumn;
	const bool bCenter = Layout == EFactoryMenuLayout::CenterBottom || Layout == EFactoryMenuLayout::CenterStack;
	HAlign = bRight ? HAlign_Right : (bCenter ? HAlign_Center : HAlign_Left);
	const ETextJustify::Type Justify = bRight ? ETextJustify::Right : (bCenter ? ETextJustify::Center : ETextJustify::Left);

	// Soft readability gradient (strips of falling alpha) on the side the text sits: a scrim, never a box.
	if (S->MenuScrimAlpha > 0.0f)
	{
		const bool bVertical = Layout == EFactoryMenuLayout::CenterBottom || Layout == EFactoryMenuLayout::LowerLeft;
		UPanelWidget* Scrim = bVertical ? static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Scrim")))
		                                : static_cast<UPanelWidget*>(WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Scrim")));
		UCanvasPanelSlot* SS = Root->AddChildToCanvas(Scrim);
		if (bVertical) { SS->SetAnchors(FAnchors(0.0f, 0.38f, 1.0f, 1.0f)); }
		else if (bRight) { SS->SetAnchors(FAnchors(0.38f, 0.0f, 1.0f, 1.0f)); }
		else if (bCenter) { SS->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); }
		else { SS->SetAnchors(FAnchors(0.0f, 0.0f, 0.62f, 1.0f)); }
		SS->SetOffsets(FMargin(0.0f));
		// One smooth gradient texture (the 24 flat strips banded visibly over a bright sunset scene, PlazaDuty 2026-11-10).
		const int32 N = 256;
		UTexture2D* Ramp = UTexture2D::CreateTransient(bVertical ? 1 : N, bVertical ? N : 1, PF_B8G8R8A8);
		if (Ramp)
		{
			Ramp->SRGB = false;
			Ramp->Filter = TF_Bilinear;
			Ramp->AddressX = TA_Clamp;
			Ramp->AddressY = TA_Clamp;
			FTexture2DMipMap& Mip = Ramp->GetPlatformData()->Mips[0];
			FColor* Px = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
			for (int32 i = 0; i < N; ++i)
			{
				const float T = static_cast<float>(i) / (N - 1);
				// Dark at the text edge: left edge (left layouts), right edge (right), bottom (vertical), flat (centre stack).
				const float Edge = bRight || bVertical ? T : 1.0f - T;
				float A = S->MenuScrimAlpha * FMath::Pow(Edge, 1.6f);
				if (Layout == EFactoryMenuLayout::CenterStack) { A = S->MenuScrimAlpha * 0.55f; }
				Px[i] = FColor(0, 0, 0, static_cast<uint8>(FMath::Clamp(A, 0.0f, 1.0f) * 255.0f));
			}
			Mip.BulkData.Unlock();
			Ramp->UpdateResource();
			UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScrimRamp"));
			Img->SetBrushFromTexture(Ramp, false);
			if (bVertical) { Cast<UVerticalBoxSlot>(Cast<UVerticalBox>(Scrim)->AddChildToVerticalBox(Img))->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
			else { Cast<UHorizontalBoxSlot>(Cast<UHorizontalBox>(Scrim)->AddChildToHorizontalBox(Img))->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		}
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	UCanvasPanelSlot* CS = Root->AddChildToCanvas(Column);
	const float M = S->MenuEdgeMargin;
	switch (Layout)
	{
	case EFactoryMenuLayout::LowerLeft:    CS->SetAnchors(FAnchors(0.0f, 1.0f)); CS->SetAlignment(FVector2D(0.0f, 1.0f)); CS->SetPosition(FVector2D(M, -M)); break;
	case EFactoryMenuLayout::RightColumn:  CS->SetAnchors(FAnchors(1.0f, 0.5f)); CS->SetAlignment(FVector2D(1.0f, 0.5f)); CS->SetPosition(FVector2D(-M, 0.0f)); break;
	case EFactoryMenuLayout::CenterBottom: CS->SetAnchors(FAnchors(0.5f, 1.0f)); CS->SetAlignment(FVector2D(0.5f, 1.0f)); CS->SetPosition(FVector2D(0.0f, -M * 0.8f)); break;
	case EFactoryMenuLayout::CenterStack:  CS->SetAnchors(FAnchors(0.5f, 0.5f)); CS->SetAlignment(FVector2D(0.5f, 0.5f)); CS->SetPosition(FVector2D(0.0f, 0.0f)); break;
	default:                               CS->SetAnchors(FAnchors(0.0f, 0.5f)); CS->SetAlignment(FVector2D(0.0f, 0.5f)); CS->SetPosition(FVector2D(M, 0.0f)); break;
	}
	CS->SetAutoSize(true);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	FactoryKitStyle::Apply(Title, S->TitleTypeface, S->TitleSize, S->TextColor, S->TitleLetterSpacing);
	Title->SetText(S->Title);
	Title->SetJustification(Justify);
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign);

	UImage* Motif = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Motif"));
	UTexture2D* MotifTex = S->MotifTexture.LoadSynchronous();
	Motif->SetBrush(FactoryKitStyle::SizedBrush(S->MotifSize, MotifTex ? FLinearColor::White : S->AccentColor, MotifTex));
	UVerticalBoxSlot* MS = Column->AddChildToVerticalBox(Motif);
	MS->SetHorizontalAlignment(HAlign);
	MS->SetPadding(FMargin(4.0f, 6.0f, 0.0f, 14.0f));

	if (!S->Tagline.IsEmpty())
	{
		UTextBlock* Tag = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Tagline"));
		FactoryKitStyle::Apply(Tag, S->BodyTypeface, S->TaglineSize, S->AccentColor, S->TaglineLetterSpacing);
		Tag->SetText(S->Tagline);
		UVerticalBoxSlot* TS = Column->AddChildToVerticalBox(Tag);
		TS->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		TS->SetHorizontalAlignment(HAlign);
	}

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.0f, 54.0f));
	Column->AddChildToVerticalBox(Gap);

	PageTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PageTitle"));
	FactoryKitStyle::Apply(PageTitle, S->BodyTypeface, S->TaglineSize, S->DimColor, S->TaglineLetterSpacing);
	PageTitle->SetText(S->OptionsText);
	UVerticalBoxSlot* PS = Column->AddChildToVerticalBox(PageTitle);
	PS->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 12.0f));
	PS->SetHorizontalAlignment(HAlign);

	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("List"));
	Column->AddChildToVerticalBox(List)->SetHorizontalAlignment(HAlign);

	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MenuHint"));
	FactoryKitStyle::Apply(HintText, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	HintText->SetText(S->MenuHintText);
	if (S->bLegendOnMenu)
	{
		HintText->SetVisibility(ESlateVisibility::Collapsed);  // drawn under the start-screen legend instead, same column
	}
	UCanvasPanelSlot* HS = Root->AddChildToCanvas(HintText);
	// The key hint sits in the corner opposite the menu.
	HS->SetAnchors(FAnchors(bRight ? 0.0f : 1.0f, 1.0f));
	HS->SetAlignment(FVector2D(bRight ? 0.0f : 1.0f, 1.0f));
	HS->SetPosition(FVector2D(bRight ? S->LegendMargin.X : -S->LegendMargin.X, -S->LegendMargin.Y));
	HS->SetAutoSize(true);

	BuildPage(EPage::Main);
}

void UFactoryMenuWidget::BuildPage(EPage NewPage)
{
	Page = NewPage;
	List->ClearChildren();
	Rows.Reset();
	Index = 0;
	PageTitle->SetVisibility(Page == EPage::Options ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Page == EPage::Main)
	{
		AddRow(EItem::Play);
		AddRow(EItem::Options);
		AddRow(EItem::Quit);
	}
	else
	{
		AddRow(EItem::Master);
		if (Flow && Flow->HasMixClasses())
		{
			AddRow(EItem::Music);
			AddRow(EItem::Sfx);
		}
		AddRow(EItem::Sensitivity);
		AddRow(EItem::InvertY);
		AddRow(EItem::Quality);
		AddRow(EItem::Window);
		AddRow(EItem::Back);
	}
	RefreshTexts();
}

void UFactoryMenuWidget::AddRow(EItem Item)
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	const bool bMain = Page == EPage::Main;
	const int32 Size = bMain ? S->MenuSize : FMath::RoundToInt(S->MenuSize * 0.66f);

	UBorder* Hit = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Hit->SetBrushColor(FLinearColor::Transparent); // hit area only: nothing is drawn
	Hit->SetPadding(FMargin(0.0f, bMain ? 7.0f : 5.0f));
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Bar->SetBrush(FactoryKitStyle::SizedBrush(FVector2D(3.0f, Size * 1.05f), S->AccentColor));
	Bar->SetRenderOpacity(0.0f);
	UHorizontalBoxSlot* BS = H->AddChildToHorizontalBox(Bar);
	BS->SetVerticalAlignment(VAlign_Center);
	BS->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FactoryKitStyle::Apply(T, bMain ? S->TitleTypeface : S->BodyTypeface, Size, S->DimColor, S->MenuLetterSpacing);
	UTextBlock* V = nullptr;
	if (bMain || Item == EItem::Back)
	{
		UHorizontalBoxSlot* TS = H->AddChildToHorizontalBox(T);
		TS->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetMinDesiredWidth(Size * 20.0f); // label column wide enough for "MOUSE SENSITIVITY"
		LabelBox->AddChild(T);
		UHorizontalBoxSlot* TS = H->AddChildToHorizontalBox(LabelBox);
		TS->SetVerticalAlignment(VAlign_Center);
		V = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(V, S->TitleTypeface, Size, S->DimColor, S->MenuLetterSpacing);
		UHorizontalBoxSlot* VS = H->AddChildToHorizontalBox(V);
		VS->SetVerticalAlignment(VAlign_Center);
	}
	Hit->SetContent(H);
	List->AddChildToVerticalBox(Hit)->SetHorizontalAlignment(HAlign);
	Rows.Add({ Item, Hit, T, Bar, V, 0.0f });
}

FText UFactoryMenuWidget::RowText(EItem Item) const
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	const FFactoryOptions* O = Flow ? &Flow->Options() : nullptr;
	auto Pct = [](float V) { return FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(V * 100.0f))); };
	auto Value = [](const FText& Name, const FText& V) { return FText::Format(LOCTEXT("ValueRow", "{0}|{1}"), Name, V); };
	switch (Item)
	{
	case EItem::Play: return S->PlayText;
	case EItem::Options: return S->OptionsText;
	case EItem::Quit: return S->QuitText;
	case EItem::Back: return S->BackText;
	case EItem::Master: return Value(LOCTEXT("Master", "MASTER VOLUME"), O ? Pct(O->Master) : FText());
	case EItem::Music: return Value(LOCTEXT("Music", "MUSIC"), O ? Pct(O->Music) : FText());
	case EItem::Sfx: return Value(LOCTEXT("Sfx", "EFFECTS"), O ? Pct(O->Sfx) : FText());
	case EItem::Sensitivity: return Value(LOCTEXT("Sens", "MOUSE SENSITIVITY"), O ? FText::FromString(FString::Printf(TEXT("%.2fx"), O->Sensitivity)) : FText());
	case EItem::InvertY: return Value(LOCTEXT("Invert", "INVERT LOOK"), O && O->bInvertY ? LOCTEXT("On", "ON") : LOCTEXT("Off", "OFF"));
	case EItem::Quality:
	{
		static const FText Names[] = { LOCTEXT("Low", "LOW"), LOCTEXT("Med", "MEDIUM"), LOCTEXT("High", "HIGH"), LOCTEXT("Epic", "EPIC") };
		return Value(LOCTEXT("Quality", "QUALITY"), O ? Names[FMath::Clamp(O->Quality, 0, 3)] : FText());
	}
	case EItem::Window: return Value(LOCTEXT("Window", "DISPLAY"), O && O->bFullscreen ? LOCTEXT("Full", "FULLSCREEN") : LOCTEXT("Windowed", "WINDOWED"));
	}
	return FText();
}

void UFactoryMenuWidget::RefreshTexts()
{
	for (FRow& R : Rows)
	{
		FString Label, Val;
		const FString Full = RowText(R.Item).ToString();
		if (R.Value && Full.Split(TEXT("|"), &Label, &Val))
		{
			R.Text->SetText(FText::FromString(Label));
			R.Value->SetText(FText::FromString(Val));
		}
		else
		{
			R.Text->SetText(FText::FromString(Full.Replace(TEXT("|"), TEXT("  "))));
		}
	}
}

void UFactoryMenuWidget::PlayUISound(bool bSelect) const
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	if (USoundBase* Snd = (bSelect ? S->SelectSound : S->HoverSound).LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Snd);
	}
}

void UFactoryMenuWidget::Move(int32 Dir)
{
	if (Rows.Num() == 0) { return; }
	Index = (Index + Dir + Rows.Num()) % Rows.Num();
	PlayUISound(false);
}

void UFactoryMenuWidget::Activate()
{
	if (!Rows.IsValidIndex(Index) || bFadingOut) { return; }
	PlayUISound(true);
	switch (Rows[Index].Item)
	{
	case EItem::Play: if (Flow) { Flow->MenuPlay(); } break;
	case EItem::Options: BuildPage(EPage::Options); break;
	case EItem::Quit: if (Flow) { Flow->MenuQuit(); } break;
	case EItem::Back: Back(); break;
	default: Adjust(+1); break;
	}
}

void UFactoryMenuWidget::Adjust(int32 Dir)
{
	if (!Flow || !Rows.IsValidIndex(Index)) { return; }
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	FFactoryOptions& O = Flow->Options();
	bool bVideo = false;
	auto Step = [Dir](float& V) { V = FMath::Clamp(FMath::RoundToFloat((V + 0.1f * Dir) * 10.0f) / 10.0f, 0.0f, 1.0f); };
	switch (Rows[Index].Item)
	{
	case EItem::Master: Step(O.Master); break;
	case EItem::Music: Step(O.Music); break;
	case EItem::Sfx: Step(O.Sfx); break;
	case EItem::Sensitivity: O.Sensitivity = FMath::Clamp(O.Sensitivity + S->SensitivityStep * Dir, S->SensitivityMin, S->SensitivityMax); break;
	case EItem::InvertY: O.bInvertY = !O.bInvertY; break;
	case EItem::Quality: O.Quality = (O.Quality + Dir + 4) % 4; bVideo = true; break;
	case EItem::Window: O.bFullscreen = !O.bFullscreen; bVideo = true; break;
	default: return;
	}
	PlayUISound(false);
	Flow->ApplyOptions(bVideo);
	O.Save();
	RefreshTexts();
}

void UFactoryMenuWidget::Back()
{
	if (Page == EPage::Options)
	{
		BuildPage(EPage::Main);
		Index = 1; // back on OPTIONS
	}
}

void UFactoryMenuWidget::PressPlayVisibly()
{
	if (Page != EPage::Main) { BuildPage(EPage::Main); }
	Index = 0;
	PressFlash = 1.0f;
	PlayUISound(true);
}

void UFactoryMenuWidget::FadeOut()
{
	bFadingOut = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FReply UFactoryMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up) { Move(-1); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { Move(+1); return FReply::Handled(); }
	if (K == EKeys::Left || K == EKeys::A || K == EKeys::Gamepad_DPad_Left) { Adjust(-1); return FReply::Handled(); }
	if (K == EKeys::Right || K == EKeys::D || K == EKeys::Gamepad_DPad_Right) { Adjust(+1); return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::E || K == EKeys::Gamepad_FaceButton_Bottom) { Activate(); return FReply::Handled(); }
	if (K == EKeys::Escape || K == EKeys::BackSpace || K == EKeys::Gamepad_FaceButton_Right) { Back(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UFactoryMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (Rows[i].HitArea->IsHovered() && i != Index)
		{
			Index = i;
			PlayUISound(false);
		}
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UFactoryMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			if (Rows[i].HitArea->IsHovered())
			{
				Index = i;
				const bool bValueRow = Rows[i].Item >= EItem::Master && Rows[i].Item <= EItem::Window;
				if (bValueRow) { Adjust(InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ? -1 : +1); }
				else { Activate(); }
				return FReply::Handled();
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UFactoryMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Opacity = FactoryKitStyle::EaseTo(Opacity, bFadingOut ? 0.0f : 1.0f, InDeltaTime, S->MotionSeconds * (bFadingOut ? 1.0f : 2.0f));
	SetRenderOpacity(Opacity);
	PressFlash = FMath::Max(0.0f, PressFlash - InDeltaTime / FMath::Max(S->MenuPressSeconds, 0.05f));
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		FRow& R = Rows[i];
		R.Sel = FactoryKitStyle::EaseTo(R.Sel, i == Index ? 1.0f : 0.0f, InDeltaTime, S->MotionSeconds);
		const float Flash = (i == Index) ? PressFlash : 0.0f;
		FLinearColor C = FMath::Lerp(S->DimColor, S->TextColor, R.Sel);
		C = FMath::Lerp(C, S->AccentColor, Flash);
		R.Text->SetColorAndOpacity(FSlateColor(C));
		R.Text->SetRenderTranslation(FVector2D(S->SelectedSlidePx * R.Sel, 0.0f));
		R.Bar->SetRenderOpacity(R.Sel);
		if (R.Value)
		{
			R.Value->SetColorAndOpacity(FSlateColor(FMath::Lerp(S->DimColor, S->AccentColor, R.Sel)));
			R.Value->SetRenderTranslation(FVector2D(S->SelectedSlidePx * R.Sel, 0.0f));
		}
	}
}

#undef LOCTEXT_NAMESPACE
