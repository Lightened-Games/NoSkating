// FactoryKit shared text styling: the theme font, soft shadow, no boxes.

#pragma once

#include "CoreMinimal.h"
#include "FactoryKitSettings.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

namespace FactoryKitStyle
{
	inline FSlateFontInfo Font(FName Typeface, int32 Size, int32 LetterSpacing = 0)
	{
		const UFactoryKitSettings* S = UFactoryKitSettings::Get();
		FSlateFontInfo Info;
		if (const UObject* FontObj = S->FontObject.IsValid() ? S->FontObject.TryLoad() : nullptr)
		{
			Info = FSlateFontInfo(FontObj, Size, Typeface);
		}
		else
		{
			Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		}
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	inline void Apply(UTextBlock* T, FName Typeface, int32 Size, const FLinearColor& Color, int32 LetterSpacing = 0)
	{
		const UFactoryKitSettings* S = UFactoryKitSettings::Get();
		T->SetFont(Font(Typeface, Size, LetterSpacing));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(S->ShadowOffset);
		T->SetShadowColorAndOpacity(S->ShadowColor);
	}

	/** An image brush of an exact size: a texture, or a flat colour when Texture is null. */
	inline FSlateBrush SizedBrush(const FVector2D& Size, const FLinearColor& Tint, UObject* Texture = nullptr)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::Image;
		B.ImageSize = Size;
		B.TintColor = FSlateColor(Tint);
		if (Texture) { B.SetResourceObject(Texture); }
		return B;
	}

	/** Ease-out step toward Target over about Seconds. */
	inline float EaseTo(float Current, float Target, float DeltaTime, float Seconds)
	{
		const float Rate = 4.6f / FMath::Max(Seconds, 0.01f); // 99 % of the way in Seconds
		return FMath::Lerp(Current, Target, 1.0f - FMath::Exp(-Rate * DeltaTime));
	}
}
