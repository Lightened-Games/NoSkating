// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PDTypes.h"
#include "Audio/PDSynth.h"
#include "Feedback/PDPuffFX.h"
#include "PDGameFeelComponent.generated.h"

class UCameraShakeBase;
class UAudioComponent;
class UPDLoopSynth;

/**
 * Game feel (P20): listens to the match event bus and answers every event with a sound, a puff and, where it
 * matters, a camera shake or the 50 ms hit-stop. Also runs the plaza ambience loop. Never changes gameplay.
 * Attach to the guard (any pawn works).
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDGameFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDGameFeelComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Freeze the world for a moment (CustomTimeDilation-style hit-stop, measured in real time). */
	UFUNCTION(BlueprintCallable, Category = "PD|Feel") void HitStop(float RealSeconds, float Dilation);
	UFUNCTION(BlueprintCallable, Category = "PD|Feel") void Shake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.0f);
	UFUNCTION(BlueprintCallable, Category = "PD|Feel") void Puff(const FVector& Location, const FPDPuffStyle& Style);
	UFUNCTION(BlueprintCallable, Category = "PD|Feel") void Play(const FPDSynthPreset& Preset, const FVector& Location, float Volume = 1.0f, float Pitch = 1.0f, bool b2D = false);

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopDilation = 0.05f;
	/** Landings closer than this to the guard shake the camera a little. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearLandingRadius = 700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float NearLandingShakeScale = 0.45f;
	/** Skaters within this radius drive the wheel rumble bed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RollingRadius = 1100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RollingFullCount = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AmbienceVolume = 0.85f;

	// ---- Looks of each puff ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle LandingDust;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle GraniteChips;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle KnobSparks;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle WhistleRing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle KnockDust;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|FX") FPDPuffStyle ConvertSparkle;

	// ---- Sounds ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset CountdownSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset GoSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset GrindSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset LandSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset WearSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset CappedSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset ConvertedSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset FriendSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset WhistleSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset KnockSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset ClosingSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset VictorySound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset DefeatSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset PromptSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Shakes") TSubclassOf<UCameraShakeBase> ImpactShake;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Shakes") TSubclassOf<UCameraShakeBase> MicroShake;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Shakes") TSubclassOf<UCameraShakeBase> RumbleShake;

private:
	void Bind();
	UFUNCTION() void HandleState(EPDMatchState NewState);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleTrick(const FPDTrickEvent& Trick);
	UFUNCTION() void HandleCapped(AActor* Line, int32 KnobsLeft);
	UFUNCTION() void HandleConverted(AActor* Line, AActor* Prop, const FText& TrickName);
	UFUNCTION() void HandleWhistle(AActor* Guard, int32 Scattered);
	UFUNCTION() void HandleKnockdown(AActor* Guard, AActor* Skater);
	UFUNCTION() void HandleFriend(AActor* Skater);
	UFUNCTION() void HandlePrompt(EPDPrompt Prompt, AActor* Anchor);
	UFUNCTION() void HandleClosing();

	UPROPERTY() TObjectPtr<UAudioComponent> AmbienceComp;
	UPROPERTY() TObjectPtr<UPDLoopSynth> Ambience;
	double HitStopEnd = 0.0;
	bool bHitStopActive = false;
	bool bBound = false;
	int32 PuffSeed = 1;
};
