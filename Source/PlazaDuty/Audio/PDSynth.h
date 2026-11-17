// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <atomic>
#include "PDSynth.generated.h"

class UAudioComponent;
class USoundAttenuation;

/**
 * Parameters for a procedural one-shot. Every sound in Plaza Duty is synthesised from these presets
 * (no third-party audio). Tools/generate_audio.py renders the same presets to WAV for asset-library/.
 */
USTRUCT(BlueprintType)
struct FPDSynthPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.01")) float Duration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0")) float Volume = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float AttackTime = 0.005f;
	/** Exponential amplitude decay (1/s), restarted on every note. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float DecayRate = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0")) float ToneMix = 0.0f;
	/** Odd harmonics on top of the sine (0 = pure sine, 1 = buzzy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0")) float ToneHarmonics = 0.0f;
	/** One-pole low-pass on the tone (Hz). 0 = off. Muffles a buzzy tone into a "PA voice". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float ToneLowpass = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "1.0")) float StartFrequency = 440.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "1.0")) float EndFrequency = 440.0f;
	/** Vibrato rate (Hz) and depth (semitones). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float WobbleRate = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float WobbleDepth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0")) float NoiseMix = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "10.0")) float NoiseLowpassStart = 8000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "10.0")) float NoiseLowpassEnd = 8000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0")) float NoiseHighpass = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0")) float CrackleChance = 0.0f;
	/** Swell instead of decay (reverse-swept sounds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth") bool bReverseEnvelope = false;
	/** Semitone offsets played in sequence. Values <= -48 are rests. Empty = one note. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth") TArray<float> NoteSemitones;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.01")) float NoteDuration = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "8000")) int32 SampleRate = 48000;
};

/** Procedural mono one-shot rendered on the audio thread from an immutable preset. */
UCLASS()
class PLAZADUTY_API UPDSynthSound : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UPDSynthSound(const FObjectInitializer& ObjectInitializer);

	void InitSynth(const FPDSynthPreset& InPreset, int32 InSeed, float InPitchScale);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	FPDSynthPreset Preset;
	FRandomStream Random;
	float PitchScale = 1.0f;
	int64 SampleCursor = 0;
	double Phase = 0.0;
	float ToneLpState = 0.0f;
	float LowpassState = 0.0f;
	float HighpassState = 0.0f;
	float HighpassPrevInput = 0.0f;
	float CrackleEnv = 0.0f;
};

/**
 * Endless procedural plaza ambience: distant city traffic, birds that thin out toward closing (Evening, set
 * through SetLightsOn), a wheel rumble bed that follows how many skaters roll nearby (Rolling 0..1) and a
 * light beat that follows Drive (0..1 = how close the wear is to the limit).
 */
UCLASS()
class PLAZADUTY_API UPDLoopSynth : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UPDLoopSynth(const FObjectInitializer& ObjectInitializer);

	void InitLoop(int32 InSeed);

	void SetDrive(float Value) { Drive.store(FMath::Clamp(Value, 0.0f, 1.0f)); }
	void SetLightsOn(float Value) { Lights.store(FMath::Clamp(Value, 0.0f, 1.0f)); }
	void SetRolling(float Value) { Rolling.store(FMath::Clamp(Value, 0.0f, 1.0f)); }
	void SetLoopVolume(float Value) { LoopVolume.store(FMath::Clamp(Value, 0.0f, 1.5f)); }

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	FRandomStream Random;
	std::atomic<float> Drive{ 0.0f };
	std::atomic<float> Lights{ 0.0f };
	std::atomic<float> Rolling{ 0.0f };
	float SmoothedRolling = 0.0f;
	float RollLp = 0.0f;
	float RollLp2 = 0.0f;
	float ClickEnv = 0.0f;
	float BirdEnv = 0.0f;
	double PhaseBird = 0.0;
	float BirdFreq = 3000.0f;
	int64 NextBird = 0;
	int64 NextClick = 0;
	std::atomic<float> LoopVolume{ 1.0f };
	float SmoothedDrive = 0.0f;
	float SmoothedLights = 0.0f;
	int64 SampleCursor = 0;
	double PhaseHum = 0.0;
	double PhaseDrone = 0.0;
	double PhaseDrone2 = 0.0;
	double PhasePulse = 0.0;
	float Brown = 0.0f;
	float WindLp = 0.0f;
	float WindLp2 = 0.0f;
	float GustLevel = 0.3f;
	float GustTarget = 0.3f;
};

UCLASS()
class PLAZADUTY_API UPDAudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 3D one-shot with distance falloff (skater tricks, prop drops). */
	UFUNCTION(BlueprintCallable, Category = "PD|Audio", meta = (WorldContext = "WorldContextObject"))
	static UAudioComponent* PlaySynthAtLocation(const UObject* WorldContextObject, const FPDSynthPreset& Preset, FVector Location, float VolumeMultiplier = 1.0f, float PitchScale = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "PD|Audio", meta = (WorldContext = "WorldContextObject"))
	static UAudioComponent* PlaySynth2D(const UObject* WorldContextObject, const FPDSynthPreset& Preset, float VolumeMultiplier = 1.0f, float PitchScale = 1.0f);

	// Default presets (used as UPROPERTY defaults; every value stays editable on the owner).
	static FPDSynthPreset CountdownTick();
	static FPDSynthPreset GoWhistle();
	static FPDSynthPreset RollIn();
	static FPDSynthPreset GrindScrape();
	static FPDSynthPreset RailScrape();
	static FPDSynthPreset OlliePop();
	static FPDSynthPreset BoardLand();
	static FPDSynthPreset WearTick();
	static FPDSynthPreset BoltRatchet();
	static FPDSynthPreset BoltClunk();
	static FPDSynthPreset KnockThud();
	static FPDSynthPreset GrabClick();
	static FPDSynthPreset DropThump();
	static FPDSynthPreset PropConverted();
	static FPDSynthPreset FriendChime();
	static FPDSynthPreset Whistle();
	static FPDSynthPreset WhistleReady();
	static FPDSynthPreset ClosingChime();
	static FPDSynthPreset Fanfare();
	static FPDSynthPreset DefeatStinger();
	static FPDSynthPreset Respawn();
	static FPDSynthPreset PromptBlip();

private:
	static UPDSynthSound* CreateSynth(const UObject* WorldContextObject, const FPDSynthPreset& Preset, float PitchScale);
	static USoundAttenuation* GetRoomAttenuation();
};
