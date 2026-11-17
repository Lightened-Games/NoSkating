// Copyright Plaza Duty. All Rights Reserved.

#include "Audio/PDSynth.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

namespace PDSynthDsp
{
	constexpr float StopTailSeconds = 0.1f;
	constexpr float Int16Scale = 32767.0f;
	constexpr float CrackleDecaySeconds = 0.004f;
	constexpr float EndFadeInverseFraction = 20.0f;
	constexpr float RestThreshold = -48.0f;
	constexpr float SmoothingPerSecond = 2.5f;

	float OnePole(float& State, float Input, float CutoffHz, float Rate)
	{
		const float Coeff = 1.0f - FMath::Exp(-UE_TWO_PI * FMath::Max(CutoffHz, 10.0f) / Rate);
		State += Coeff * (Input - State);
		return State;
	}

	float Sine(double Phase) { return FMath::Sin(static_cast<float>(Phase) * UE_TWO_PI); }
	float Saw(double Phase) { return static_cast<float>(2.0 * Phase - 1.0); }
	void Advance(double& Phase, float Freq, float Rate) { Phase += static_cast<double>(Freq) / Rate; Phase -= FMath::FloorToDouble(Phase); }
}

// ---------------------------------------------------------------------------------------------
// One-shot
// ---------------------------------------------------------------------------------------------

UPDSynthSound::UPDSynthSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = false;
}

void UPDSynthSound::InitSynth(const FPDSynthPreset& InPreset, int32 InSeed, float InPitchScale)
{
	Preset = InPreset;
	Random.Initialize(InSeed);
	PitchScale = FMath::Max(InPitchScale, 0.05f);
	SampleCursor = 0;
	Phase = 0.0;
	ToneLpState = LowpassState = HighpassState = HighpassPrevInput = CrackleEnv = 0.0f;
	NumChannels = 1;
	SetSampleRate(static_cast<uint32>(Preset.SampleRate));
	Duration = Preset.Duration;
	bLooping = false;
}

int32 UPDSynthSound::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * static_cast<int32>(sizeof(int16)));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());

	const float Rate = static_cast<float>(Preset.SampleRate);
	const int64 Total = static_cast<int64>(Preset.Duration * Rate);
	const float CrackleDecay = FMath::Exp(-1.0f / (PDSynthDsp::CrackleDecaySeconds * Rate));

	for (int32 i = 0; i < NumSamples; ++i, ++SampleCursor)
	{
		if (SampleCursor >= Total)
		{
			Out[i] = 0;
			continue;
		}
		const float T = static_cast<float>(SampleCursor) / Rate;
		const float Alpha = static_cast<float>(SampleCursor) / static_cast<float>(FMath::Max<int64>(Total, 1));

		float LocalT = T;
		float NoteOffset = 0.0f;
		bool bRest = false;
		if (Preset.NoteSemitones.Num() > 0)
		{
			const int32 NoteIndex = FMath::Min(static_cast<int32>(T / Preset.NoteDuration), Preset.NoteSemitones.Num() - 1);
			LocalT = T - NoteIndex * Preset.NoteDuration;
			NoteOffset = Preset.NoteSemitones[NoteIndex];
			bRest = NoteOffset <= PDSynthDsp::RestThreshold;
		}

		float Env;
		if (Preset.bReverseEnvelope)
		{
			Env = FMath::Pow(Alpha, 2.0f) * FMath::Clamp((1.0f - Alpha) * PDSynthDsp::EndFadeInverseFraction, 0.0f, 1.0f);
		}
		else
		{
			const float Attack = Preset.AttackTime > 0.0f ? FMath::Clamp(LocalT / Preset.AttackTime, 0.0f, 1.0f) : 1.0f;
			const float Decay = FMath::Exp(-Preset.DecayRate * FMath::Max(0.0f, LocalT - Preset.AttackTime));
			const float Fade = FMath::Clamp((1.0f - Alpha) * PDSynthDsp::EndFadeInverseFraction, 0.0f, 1.0f);
			Env = Attack * Decay * Fade;
		}
		if (bRest)
		{
			Env = 0.0f;
		}

		float Sample = 0.0f;
		if (Preset.ToneMix > 0.0f)
		{
			const float Base = Preset.StartFrequency * FMath::Pow(Preset.EndFrequency / Preset.StartFrequency, Alpha);
			const float Wobble = Preset.WobbleRate > 0.0f ? Preset.WobbleDepth * FMath::Sin(UE_TWO_PI * Preset.WobbleRate * T) : 0.0f;
			const float Freq = Base * PitchScale * FMath::Pow(2.0f, ((bRest ? 0.0f : NoteOffset) + Wobble) / 12.0f);
			Phase += static_cast<double>(Freq) / Rate;
			Phase -= FMath::FloorToDouble(Phase);
			const float Angle = static_cast<float>(Phase) * UE_TWO_PI;
			const float Harm = FMath::Sin(Angle * 3.0f) / 3.0f + FMath::Sin(Angle * 5.0f) / 5.0f + FMath::Sin(Angle * 2.0f) * 0.25f;
			float Tone = FMath::Sin(Angle) + Preset.ToneHarmonics * Harm * 1.6f;
			if (Preset.ToneLowpass > 0.0f)
			{
				Tone = PDSynthDsp::OnePole(ToneLpState, Tone, Preset.ToneLowpass * PitchScale, Rate) * 1.4f;
			}
			Sample += Preset.ToneMix * Tone;
		}

		if (Preset.NoiseMix > 0.0f || Preset.CrackleChance > 0.0f)
		{
			float Noise = Random.FRandRange(-1.0f, 1.0f);
			if (Preset.CrackleChance > 0.0f && Random.FRand() < Preset.CrackleChance)
			{
				CrackleEnv = 1.0f;
			}
			Noise += CrackleEnv * Random.FRandRange(-3.0f, 3.0f);
			CrackleEnv *= CrackleDecay;

			const float Cutoff = FMath::Lerp(Preset.NoiseLowpassStart, Preset.NoiseLowpassEnd, Alpha) * PitchScale;
			float Filtered = PDSynthDsp::OnePole(LowpassState, Noise, Cutoff, Rate);
			if (Preset.NoiseHighpass > 0.0f)
			{
				const float Rc = 1.0f / (UE_TWO_PI * Preset.NoiseHighpass);
				const float Dt = 1.0f / Rate;
				const float HpCoeff = Rc / (Rc + Dt);
				HighpassState = HpCoeff * (HighpassState + Filtered - HighpassPrevInput);
				HighpassPrevInput = Filtered;
				Filtered = HighpassState;
			}
			Sample += Preset.NoiseMix * Filtered;
		}

		Out[i] = static_cast<int16>(FMath::Clamp(Sample * Env * Preset.Volume, -1.0f, 1.0f) * PDSynthDsp::Int16Scale);
	}
	return NumSamples;
}

// ---------------------------------------------------------------------------------------------
// Ambience loop
// ---------------------------------------------------------------------------------------------

UPDLoopSynth::UPDLoopSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = true;
}

void UPDLoopSynth::InitLoop(int32 InSeed)
{
	Random.Initialize(InSeed);
	NumChannels = 1;
	SetSampleRate(48000);
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
}

int32 UPDLoopSynth::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	using namespace PDSynthDsp;
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * static_cast<int32>(sizeof(int16)));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	const float Rate = 48000.0f;
	const float TargetDrive = Drive.load();
	const float TargetEvening = Lights.load();
	const float TargetRolling = Rolling.load();
	const float Vol = LoopVolume.load();
	const float Smooth = 1.0f - FMath::Exp(-SmoothingPerSecond / Rate);

	for (int32 i = 0; i < NumSamples; ++i, ++SampleCursor)
	{
		SmoothedDrive += Smooth * (TargetDrive - SmoothedDrive);
		SmoothedLights += Smooth * (TargetEvening - SmoothedLights);
		SmoothedRolling += Smooth * 3.0f * (TargetRolling - SmoothedRolling);
		const float T = static_cast<float>(SampleCursor) / Rate;

		// Distant traffic: brown noise, low-passed, slow swells.
		Brown = FMath::Clamp(Brown * 0.996f + Random.FRandRange(-1.0f, 1.0f) * 0.02f, -1.0f, 1.0f);
		if ((SampleCursor % 36000) == 0)
		{
			GustTarget = Random.FRandRange(0.25f, 0.9f);
		}
		GustLevel += (GustTarget - GustLevel) * (1.0f / (Rate * 2.0f));
		float S = 0.4f * OnePole(WindLp2, Brown, 180.0f + 160.0f * GustLevel, Rate) * (0.6f + 0.4f * GustLevel);
		S += 0.05f * OnePole(WindLp, Random.FRandRange(-1.0f, 1.0f), 900.0f, Rate) * GustLevel;

		// Birds: short chirp sweeps, fewer as the evening comes in.
		if (SampleCursor >= NextBird)
		{
			NextBird = SampleCursor + static_cast<int64>(Rate * Random.FRandRange(0.6f, 2.4f) * (1.0f + 2.0f * SmoothedLights));
			BirdEnv = 1.0f;
			BirdFreq = Random.FRandRange(2600.0f, 4200.0f);
		}
		BirdEnv *= 0.99965f;
		Advance(PhaseBird, BirdFreq * (1.0f + 0.25f * FMath::Sin(UE_TWO_PI * 18.0f * T)), Rate);
		S += 0.025f * BirdEnv * BirdEnv * Sine(PhaseBird) * (1.0f - 0.7f * SmoothedLights);

		// Wheel rumble bed + joint clicks while skaters roll near the guard.
		const float Roll = OnePole(RollLp2, OnePole(RollLp, Random.FRandRange(-1.0f, 1.0f), 260.0f, Rate), 140.0f, Rate);
		S += 0.55f * SmoothedRolling * Roll;
		if (SampleCursor >= NextClick)
		{
			NextClick = SampleCursor + static_cast<int64>(Rate * Random.FRandRange(0.12f, 0.35f));
			ClickEnv = SmoothedRolling;
		}
		ClickEnv *= 0.9975f;
		S += 0.10f * ClickEnv * Random.FRandRange(-1.0f, 1.0f);

		// Light beat: kick on the quarter, tick on the eighth; louder as the wear climbs.
		const float Bpm = 92.0f + 24.0f * SmoothedDrive;
		const float Beat = T * Bpm / 60.0f;
		const float BeatFrac = Beat - FMath::FloorToFloat(Beat);
		const float Kick = FMath::Exp(-BeatFrac * 18.0f) * FMath::Sin(UE_TWO_PI * (55.0f + 60.0f * FMath::Exp(-BeatFrac * 30.0f)) * BeatFrac * 0.65f);
		const float Half = Beat * 2.0f - FMath::FloorToFloat(Beat * 2.0f);
		const float Tick = FMath::Exp(-Half * 60.0f) * Random.FRandRange(-1.0f, 1.0f);
		Advance(PhaseDrone, 73.42f, Rate);
		Advance(PhaseDrone2, 110.0f, Rate);
		const float Pad = 0.5f * Sine(PhaseDrone) + 0.35f * Sine(PhaseDrone2);
		S += (0.12f + 0.16f * SmoothedDrive) * (0.9f * Kick + 0.12f * Tick) + 0.03f * (0.3f + SmoothedDrive) * Pad;

		Out[i] = static_cast<int16>(FMath::Clamp(S * Vol, -1.0f, 1.0f) * Int16Scale);
	}
	return NumSamples;
}

// ---------------------------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------------------------

UPDSynthSound* UPDAudioLibrary::CreateSynth(const UObject* WorldContextObject, const FPDSynthPreset& Preset, float PitchScale)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Preset.Volume <= 0.0f)
	{
		return nullptr;
	}
	UPDSynthSound* Sound = NewObject<UPDSynthSound>(GetTransientPackage());
	Sound->InitSynth(Preset, FMath::Rand(), PitchScale);
	return Sound;
}

USoundAttenuation* UPDAudioLibrary::GetRoomAttenuation()
{
	static TStrongObjectPtr<USoundAttenuation> Attenuation;
	if (!Attenuation.IsValid())
	{
		Attenuation.Reset(NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("PD_PlazaAttenuation")));
		FSoundAttenuationSettings& S = Attenuation->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		S.AttenuationShapeExtents = FVector(250.0f);
		S.FalloffDistance = 2600.0f;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	}
	return Attenuation.Get();
}

UAudioComponent* UPDAudioLibrary::PlaySynthAtLocation(const UObject* WorldContextObject, const FPDSynthPreset& Preset, FVector Location, float VolumeMultiplier, float PitchScale)
{
	UPDSynthSound* Sound = CreateSynth(WorldContextObject, Preset, PitchScale);
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(WorldContextObject, Sound, Location, FRotator::ZeroRotator, VolumeMultiplier, 1.0f, 0.0f, GetRoomAttenuation());
	if (Comp)
	{
		Comp->StopDelayed(Preset.Duration + PDSynthDsp::StopTailSeconds);
	}
	return Comp;
}

UAudioComponent* UPDAudioLibrary::PlaySynth2D(const UObject* WorldContextObject, const FPDSynthPreset& Preset, float VolumeMultiplier, float PitchScale)
{
	UPDSynthSound* Sound = CreateSynth(WorldContextObject, Preset, PitchScale);
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(WorldContextObject, Sound, VolumeMultiplier);
	if (Comp)
	{
		Comp->StopDelayed(Preset.Duration + PDSynthDsp::StopTailSeconds);
	}
	return Comp;
}

// ---------------------------------------------------------------------------------------------
// Plaza presets (Tools/generate_audio.py renders the same numbers to WAV for asset-library/)
// ---------------------------------------------------------------------------------------------

FPDSynthPreset UPDAudioLibrary::CountdownTick()
{
	FPDSynthPreset P;
	P.Duration = 0.22f; P.Volume = 0.35f; P.DecayRate = 9.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.4f; P.StartFrequency = P.EndFrequency = 660.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::GoWhistle()
{
	// Shift start: two-tone guard whistle.
	FPDSynthPreset P;
	P.Duration = 0.7f; P.Volume = 0.42f; P.AttackTime = 0.01f; P.DecayRate = 1.5f;
	P.ToneMix = 0.8f; P.ToneHarmonics = 0.2f; P.StartFrequency = P.EndFrequency = 2100.0f; P.WobbleRate = 32.0f; P.WobbleDepth = 0.6f;
	P.NoiseMix = 0.25f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 5000.0f; P.NoiseHighpass = 1800.0f;
	P.NoteSemitones = { 0.0f, 0.0f, 4.0f, 4.0f }; P.NoteDuration = 0.17f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::RollIn()
{
	// A skater rolling up: rising wheel rumble.
	FPDSynthPreset P;
	P.Duration = 1.1f; P.Volume = 0.35f; P.bReverseEnvelope = true;
	P.NoiseMix = 0.9f; P.NoiseLowpassStart = 150.0f; P.NoiseLowpassEnd = 700.0f; P.CrackleChance = 0.0015f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::GrindScrape()
{
	// Steel trucks on granite.
	FPDSynthPreset P;
	P.Duration = 0.9f; P.Volume = 0.34f; P.AttackTime = 0.01f; P.DecayRate = 1.2f;
	P.NoiseMix = 0.9f; P.NoiseLowpassStart = 5200.0f; P.NoiseLowpassEnd = 2600.0f; P.NoiseHighpass = 700.0f; P.CrackleChance = 0.004f;
	P.ToneMix = 0.12f; P.ToneHarmonics = 1.0f; P.StartFrequency = 380.0f; P.EndFrequency = 330.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::RailScrape()
{
	// Trucks on a steel handrail: a ringing grind.
	FPDSynthPreset P;
	P.Duration = 0.9f; P.Volume = 0.32f; P.AttackTime = 0.01f; P.DecayRate = 1.0f;
	P.NoiseMix = 0.6f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 6000.0f; P.NoiseHighpass = 1500.0f;
	P.ToneMix = 0.35f; P.ToneHarmonics = 0.7f; P.StartFrequency = 1240.0f; P.EndFrequency = 1180.0f; P.WobbleRate = 9.0f; P.WobbleDepth = 0.2f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::OlliePop()
{
	// Tail snap.
	FPDSynthPreset P;
	P.Duration = 0.18f; P.Volume = 0.5f; P.AttackTime = 0.001f; P.DecayRate = 32.0f;
	P.NoiseMix = 0.8f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 3500.0f; P.NoiseHighpass = 250.0f;
	P.ToneMix = 0.5f; P.StartFrequency = 320.0f; P.EndFrequency = 120.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::BoardLand()
{
	// Four wheels and the deck slapping down.
	FPDSynthPreset P;
	P.Duration = 0.32f; P.Volume = 0.55f; P.AttackTime = 0.001f; P.DecayRate = 14.0f;
	P.NoiseMix = 0.7f; P.NoiseLowpassStart = 2400.0f; P.NoiseLowpassEnd = 300.0f;
	P.ToneMix = 0.6f; P.ToneHarmonics = 0.3f; P.StartFrequency = 140.0f; P.EndFrequency = 60.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::WearTick()
{
	// A chip off the granite: a dry tick.
	FPDSynthPreset P;
	P.Duration = 0.12f; P.Volume = 0.3f; P.AttackTime = 0.001f; P.DecayRate = 40.0f;
	P.NoiseMix = 0.5f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 7000.0f; P.NoiseHighpass = 2500.0f;
	P.ToneMix = 0.5f; P.StartFrequency = P.EndFrequency = 1800.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::BoltRatchet()
{
	// One ratchet click (played every quarter second while bolting).
	FPDSynthPreset P;
	P.Duration = 0.09f; P.Volume = 0.42f; P.AttackTime = 0.001f; P.DecayRate = 45.0f;
	P.NoiseMix = 0.7f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 6500.0f; P.NoiseHighpass = 1800.0f;
	P.ToneMix = 0.4f; P.ToneHarmonics = 0.8f; P.StartFrequency = P.EndFrequency = 2600.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::BoltClunk()
{
	// The knob seats into the granite: a heavy metal clunk.
	FPDSynthPreset P;
	P.Duration = 0.7f; P.Volume = 0.6f; P.AttackTime = 0.001f; P.DecayRate = 6.0f;
	P.ToneMix = 0.7f; P.ToneHarmonics = 0.7f; P.StartFrequency = 220.0f; P.EndFrequency = 150.0f;
	P.NoiseMix = 0.5f; P.NoiseLowpassStart = 3000.0f; P.NoiseLowpassEnd = 400.0f;
	P.NoteSemitones = { 0.0f, 7.0f }; P.NoteDuration = 0.12f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::KnockThud()
{
	// Body hit and a whoosh.
	FPDSynthPreset P;
	P.Duration = 0.6f; P.Volume = 0.62f; P.AttackTime = 0.002f; P.DecayRate = 6.0f;
	P.ToneMix = 0.7f; P.StartFrequency = 110.0f; P.EndFrequency = 40.0f;
	P.NoiseMix = 0.6f; P.NoiseLowpassStart = 1800.0f; P.NoiseLowpassEnd = 200.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::GrabClick()
{
	FPDSynthPreset P;
	P.Duration = 0.12f; P.Volume = 0.35f; P.AttackTime = 0.001f; P.DecayRate = 30.0f;
	P.ToneMix = 0.6f; P.ToneHarmonics = 0.3f; P.StartFrequency = 700.0f; P.EndFrequency = 900.0f;
	P.NoiseMix = 0.3f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 4000.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::DropThump()
{
	FPDSynthPreset P;
	P.Duration = 0.35f; P.Volume = 0.5f; P.AttackTime = 0.001f; P.DecayRate = 12.0f;
	P.ToneMix = 0.6f; P.StartFrequency = 160.0f; P.EndFrequency = 70.0f;
	P.NoiseMix = 0.5f; P.NoiseLowpassStart = 1500.0f; P.NoiseLowpassEnd = 250.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::PropConverted()
{
	// "They worked it out": a bright rising two-note chime.
	FPDSynthPreset P;
	P.Duration = 0.5f; P.Volume = 0.36f; P.AttackTime = 0.004f; P.DecayRate = 5.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.15f; P.StartFrequency = P.EndFrequency = 784.0f;
	P.NoteSemitones = { 0.0f, 5.0f }; P.NoteDuration = 0.14f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::FriendChime()
{
	FPDSynthPreset P;
	P.Duration = 0.6f; P.Volume = 0.3f; P.AttackTime = 0.004f; P.DecayRate = 4.5f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.1f; P.StartFrequency = P.EndFrequency = 659.25f;
	P.NoteSemitones = { 0.0f, 4.0f, 7.0f }; P.NoteDuration = 0.11f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::Whistle()
{
	// The guard whistle: a shrill trill.
	FPDSynthPreset P;
	P.Duration = 0.85f; P.Volume = 0.5f; P.AttackTime = 0.008f; P.DecayRate = 1.4f;
	P.ToneMix = 0.8f; P.ToneHarmonics = 0.25f; P.StartFrequency = 2500.0f; P.EndFrequency = 2350.0f; P.WobbleRate = 38.0f; P.WobbleDepth = 0.9f;
	P.NoiseMix = 0.3f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 6000.0f; P.NoiseHighpass = 2000.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::WhistleReady()
{
	FPDSynthPreset P;
	P.Duration = 0.1f; P.Volume = 0.2f; P.AttackTime = 0.002f; P.DecayRate = 25.0f;
	P.ToneMix = 1.0f; P.StartFrequency = P.EndFrequency = 1320.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::ClosingChime()
{
	// Building chime: ding-dong.
	FPDSynthPreset P;
	P.Duration = 1.2f; P.Volume = 0.42f; P.AttackTime = 0.004f; P.DecayRate = 2.6f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.2f; P.StartFrequency = P.EndFrequency = 659.25f;
	P.NoteSemitones = { 0.0f, -4.0f }; P.NoteDuration = 0.5f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::Fanfare()
{
	// Still employed: a bright brass-like arpeggio.
	FPDSynthPreset P;
	P.Duration = 1.5f; P.Volume = 0.45f; P.AttackTime = 0.01f; P.DecayRate = 2.2f;
	P.ToneMix = 0.9f; P.ToneHarmonics = 0.6f; P.ToneLowpass = 2600.0f; P.StartFrequency = P.EndFrequency = 392.0f;
	P.NoteSemitones = { 0.0f, 4.0f, 7.0f, 12.0f, 12.0f }; P.NoteDuration = 0.16f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::DefeatStinger()
{
	// Fired: a descending sting.
	FPDSynthPreset P;
	P.Duration = 1.6f; P.Volume = 0.5f; P.AttackTime = 0.01f; P.DecayRate = 1.4f;
	P.ToneMix = 0.9f; P.ToneHarmonics = 0.7f; P.ToneLowpass = 1600.0f; P.StartFrequency = 330.0f; P.EndFrequency = 260.0f;
	P.NoteSemitones = { 0.0f, -1.0f, -3.0f, -8.0f }; P.NoteDuration = 0.3f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::Respawn()
{
	FPDSynthPreset P;
	P.Duration = 0.4f; P.Volume = 0.3f; P.AttackTime = 0.01f; P.DecayRate = 5.0f;
	P.ToneMix = 1.0f; P.StartFrequency = 300.0f; P.EndFrequency = 900.0f;
	return P;
}

FPDSynthPreset UPDAudioLibrary::PromptBlip()
{
	FPDSynthPreset P;
	P.Duration = 0.14f; P.Volume = 0.18f; P.AttackTime = 0.002f; P.DecayRate = 18.0f;
	P.ToneMix = 1.0f; P.StartFrequency = P.EndFrequency = 990.0f;
	return P;
}
