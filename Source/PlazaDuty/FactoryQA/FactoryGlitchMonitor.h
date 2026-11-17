// FactoryQA glitch log (design guide 2.5.6 / Section 12): finds the glitches a reviewer notices, during a normal autoplay run.
// Active with -FactoryQA or -GlitchLog. No game code needed. Each finding is logged once per subject with its time and
// place, and the first MaxShotsPerKind of each kind get a screenshot (with UI) in the FactoryQA report folder.
//   hitch        a frame slower than HitchMs (after warm-up)                         -> reported, fails over MaxHitches
//   ref-pose     a visible skeletal mesh in its bind pose (T/A-pose: missing anim, spawn/restart frame)
//   floating     a shadow-casting prop with nothing under or touching it, not even another mesh's bounds
//   support-gap  (warning) a prop that visibly rests on/against a mesh whose collision is more than FloatGapCm away
//   jitter       a simulating body that never settles (awake, crawling speed, for JitterSeconds)
//   camera-clip  the player's camera inside blocking geometry
//   stuck        a character trying to move (acceleration) but not moving, for StuckSeconds
//   spin         a character turning faster than SpinDegPerSec for SpinSeconds
//   log          Error / Warning lines after the world starts (Errors fail; Warnings are listed)
// Output: glitch.json / glitch.txt next to report.json; log lines start with "[FactoryQA] Glitch".

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FactoryGlitchMonitor.generated.h"

class FFactoryLogScanner;
class ACharacter;
class UPrimitiveComponent;

UCLASS(Config = Game)
class UFactoryGlitchMonitor : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	void WriteReport();

	// ---------------- Tuning (QA thresholds; documented defaults, change per project in DefaultGame.ini) ----------------
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float WarmupSeconds = 6.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float HitchMs = 50.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 MaxHitches = 3;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 MaxShotsPerKind = 3;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float RefPoseMaxDeg = 0.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FloatGapCm = 3.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FloatMinSizeCm = 12.0f;
	/** Props whose bottom is higher than this above the player's feet (lamps, signs) are not checked for floating. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FloatMaxHeightCm = 180.0f;
	/** Props wider than this (floors, walls, shells) are not checked for floating. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FloatMaxSizeCm = 800.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FloatCheckInterval = 20.0f;
	/** Only shadow-casting meshes count as physical props (world markers, glows and FX don't cast shadows and float by design). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") bool bFloatShadowCastersOnly = true;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float JitterMinSpeed = 0.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float JitterMaxSpeed = 30.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float JitterSeconds = 3.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float CameraClipRadius = 6.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float StuckSeconds = 2.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float StuckMaxSpeed = 10.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float SpinDegPerSec = 540.0f;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float SpinSeconds = 1.0f;
	/** Log lines containing any of these are ignored (engine self-tests, our own QA lines). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") TArray<FString> LogIgnore = { TEXT("[Factory"), TEXT("UnifiedErrorTest"), TEXT("Error test"), TEXT("Error with "), TEXT("FError that has been") };
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") FName NoCollideTag = TEXT("NoCollide");

private:
	struct FGlitch { FString Kind; FString Subject; FString Detail; float FirstTime = 0.0f; int32 Count = 0; FVector Where = FVector::ZeroVector; };
	struct FCharTrack { float StuckTime = 0.0f; float SpinTime = 0.0f; float PrevYaw = 0.0f; bool bHavePrev = false; bool bInStuck = false; bool bInSpin = false; };

	void Report(const FString& Kind, const FString& Subject, const FString& Detail, const FVector& Where, float Radius, const UPrimitiveComponent* Comp);
	void CheckRefPose();
	void CheckFloating();
	void CheckJitter(float Dt);
	void CheckCamera();
	void CheckCharacters(float Dt);
	float PlayerFeetZ() const;

	TMap<FString, FGlitch> Glitches;
	TMap<FString, int32> ShotsTaken;
	TMap<TWeakObjectPtr<ACharacter>, FCharTrack> Chars;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, float> JitterTime;
	TSharedPtr<FFactoryLogScanner> LogScanner;
	TArray<float> Hitches;
	int32 LastQAShots = 0;
	int32 QuietFrames = 0;
	TArray<FString> PendingShots;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FBox> VisualBounds;
	float Elapsed = 0.0f;
	float SinceFloatCheck = 0.0f;
	float SinceReport = 0.0f;
	bool bFloatChecked = false;
};
