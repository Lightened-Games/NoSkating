// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/PDInterfaces.h"
#include "Data/PDTypes.h"
#include "PDGameMode.generated.h"

class UPDMatchConfig;
class UPDMatchEvents;
class UPDSkaterDirectorComponent;

/**
 * Plaza Duty game mode: owns the strict 4-state loop (Ready 3-2-1 -> Active 90 s -> Victory | Defeat -> R),
 * the wear meter, the skatestopper box, the phases and the event bus. Parses -autoplay and the QA flags
 * (-autoplayexit / -PDExit, -PDDefeat, -PDIdle, -PDSeed=, -PDRuns=, -PDShots=) and writes telemetry.
 */
UCLASS()
class PLAZADUTY_API APDGameMode : public AGameModeBase, public IPDMatchFlow
{
	GENERATED_BODY()

public:
	APDGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IPDMatchFlow
	virtual EPDMatchState GetMatchState() const override { return State; }
	virtual const UPDMatchConfig* GetMatchConfig() const override { return Config; }
	virtual UPDMatchEvents* GetMatchEvents() const override { return Events; }
	virtual float GetElapsed() const override { return Elapsed; }
	virtual float GetTimeRemaining() const override;
	virtual float GetClockRemaining() const override { return GetTimeRemaining() * ClockScale; }
	virtual float GetCountdownRemaining() const override { return CountdownLeft; }
	virtual float GetWear() const override { return Wear; }
	virtual float GetWearLimit() const override;
	virtual float GetWearRate() const override { return WearRate; }
	virtual int32 GetKnobsLeft() const override { return KnobsLeft; }
	virtual int32 GetPhaseIndex() const override { return PhaseIndex; }
	virtual const FPDPhaseRow& GetPhase() const override;
	virtual const FPDMatchStats& GetMatchStats() const override { return Stats; }
	virtual FText GetEndReason() const override { return EndReason; }
	virtual bool IsAutoplayRun() const override { return bAutoplayActive; }
	virtual int32 GetMatchSeed() const override { return RunSeed; }
	virtual void RequestRestart() override;
	virtual TArray<AActor*> GetSkaters() const override;
	virtual TArray<AActor*> GetSkateLines() const override;
	virtual TArray<AActor*> GetProps() const override;
	virtual const TArray<FVector>& GetDeadZones() const override { return DeadZones; }
	virtual AActor* GetGuard() const override { return Guard; }
	virtual void ReportTrick(const FPDTrickEvent& Trick) override;
	virtual bool ConsumeKnob() override;
	virtual void ReportLineCapped(AActor* Line) override;
	virtual void ReportPropConverted(AActor* Line, AActor* Prop, const FText& TrickName) override;
	virtual void ReportPropMoved(AActor* Prop, bool bOnLine) override;
	virtual void ReportWhistle(AActor* InGuard, int32 Scattered) override;
	virtual void ReportKnockdown(AActor* InGuard, AActor* Skater) override;
	virtual void ReportPrompt(EPDPrompt Prompt, AActor* Anchor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDMatchEvents> Events;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDSkaterDirectorComponent> Director;

	UPROPERTY(EditAnywhere, Category = "Config") TSoftObjectPtr<UPDMatchConfig> MatchConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bAutoplayActive = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float AutoplayRestartDelay = 5.0f;

	/** Spec: KillZ -2000 (also written to WorldSettings at runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KillZ = -2000.0f;
	/** Window for the wear-per-second trend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WearRateWindow = 6.0f;
	/** Plaza lamps come on at closing (intensity of lights tagged PD.ClosingLamp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ClosingLampIntensity = 260.0f;
	/** Sunset: the lamp posts are already on at this intensity (cd) and brighten to ClosingLampIntensity at closing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ClosingLampBaseIntensity = 160.0f;
	/** Showcase (-PDShowcase): the shift is compressed to this length (phases and line openings scale with it) and the bot
	 *  demonstrates props early, so the <= 60 s video reaches Victory from the start screen (D25). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcaseShiftSeconds = 50.0f;
	/** Showcase: the closing clock on screen starts here and runs faster to reach 0:00 when the real shift ends
	 *  (user, 2026-11-10: "make the close timer 1:30 but complete the autorun in 50 seconds"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcaseClockSeconds = 90.0f;
	/** Showcase: skaters sent to a freshly converted prop's line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") int32 ShowcaseNudgedSkaters = 2;
	/** Opening crew, already rolling during the countdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 StartSkaters = 3;

private:
	void LoadConfig();
	void ParseCommandLine();
	void CheckCommandLineAutoplay();
	void SetupAutoplayController();
	void ScheduleQAScreenshots();
	void GatherLevel();
	void SetupMatch();
	void SetPlayerInputLocked(bool bLocked);
	void BeginActive();
	void EndMatch(bool bVictory, const FText& Reason);
	void WriteTelemetry(bool bVictory) const;
	void AfterMatch();
	void SetClosingLamps(float Alpha);
	FString FormatClock(float Seconds) const;
	void UpdatePortraitCamera();
	/** FactoryKit: PLAY pressed (or the menu skipped on restart): show the HUD and start the 3-2-1. */
	UFUNCTION() void HandleFlowStart();
	/** The line that took the most wear (the defeat shot looks at it). */
	AActor* MostWornLine() const;
	void ApplyShowcaseTiming();

	UPROPERTY() TObjectPtr<UPDMatchConfig> Config;
	UPROPERTY() TObjectPtr<AActor> Guard;
	UPROPERTY() TArray<TObjectPtr<AActor>> Lines;
	UPROPERTY() TArray<TObjectPtr<AActor>> Props;
	UPROPERTY() TArray<TObjectPtr<AActor>> ClosingLamps;
	TArray<FVector> DeadZones;
	TSet<EPDPrompt> PromptsShown;
	TArray<TPair<float, float>> WearHistory;

	EPDMatchState State = EPDMatchState::Ready;
	FPDMatchStats Stats;
	FText EndReason;
	float CountdownLeft = 0.0f;
	int32 LastCountdownSecond = -1;
	float Elapsed = 0.0f;
	float Wear = 0.0f;
	float WearRate = 0.0f;
	int32 KnobsLeft = 0;
	int32 PhaseIndex = 0;
	int32 RunSeed = 7;
	float LampAlpha = 0.0f;
	bool bDefeatMode = false;
	bool bSetupDone = false;
	bool bClosingWarned = false;
	/** Held at Ready until FactoryKit's menu and intro finish. */
	bool bFlowHold = false;
	bool bShowcase = false;
	/** Displayed clock seconds per real shift second (1 outside the showcase). */
	float ClockScale = 1.0f;
	TMap<FName, float> LineWear;
	/** QA (-PDPerf): real frame times, logged every 10 s as average fps, p95 and worst frame. */
	TArray<float> PerfFrames;
	float PerfClock = 0.0f;
	bool bPerf = false;
	FTimerHandle AfterMatchTimer;
	/** QA: -PDPortrait=N follows skater N (-1 = the guard) with a close camera. */
	UPROPERTY() TObjectPtr<class ACameraActor> PortraitCamera;
	int32 PortraitIndex = -100;
	float PortraitYaw = 0.0f;
};
