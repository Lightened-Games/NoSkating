// Copyright Plaza Duty. All Rights Reserved.

#include "Feedback/PDCameraShakes.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace PDShake
{
	UPerlinNoiseCameraShakePattern* Make(UCameraShakeBase* Owner, float Duration, float BlendOut, float Loc, float Rot, float Freq)
	{
		UPerlinNoiseCameraShakePattern* P = Owner->CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
		P->Duration = Duration;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = BlendOut;
		P->LocationAmplitudeMultiplier = 1.0f;
		P->LocationFrequencyMultiplier = 1.0f;
		P->X.Amplitude = Loc * 0.5f; P->X.Frequency = Freq;
		P->Y.Amplitude = Loc;        P->Y.Frequency = Freq * 1.1f;
		P->Z.Amplitude = Loc;        P->Z.Frequency = Freq * 1.3f;
		P->RotationAmplitudeMultiplier = 1.0f;
		P->RotationFrequencyMultiplier = 1.0f;
		P->Pitch.Amplitude = Rot;        P->Pitch.Frequency = Freq * 1.2f;
		P->Yaw.Amplitude = Rot * 0.6f;   P->Yaw.Frequency = Freq;
		P->Roll.Amplitude = Rot * 0.8f;  P->Roll.Frequency = Freq * 0.9f;
		return P;
	}
}

UPDImpactShake::UPDImpactShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(PDShake::Make(this, 0.45f, 0.25f, 8.0f, 1.4f, 22.0f));
}

UPDMicroShake::UPDMicroShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(PDShake::Make(this, 0.2f, 0.12f, 2.0f, 0.4f, 26.0f));
}

UPDRumbleShake::UPDRumbleShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(PDShake::Make(this, 0.9f, 0.4f, 3.0f, 0.4f, 9.0f));
}
