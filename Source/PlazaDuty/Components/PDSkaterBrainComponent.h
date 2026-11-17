// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Audio/PDSynth.h"
#include "PDSkaterBrainComponent.generated.h"

class UPDAnimInstance;

UENUM(BlueprintType)
enum class EPDSkaterMode : uint8
{
	Idle,
	Transfer,
	Ride,
	Scatter,
	Bail,
	Home,
	Gone
};

/**
 * Skater brain (new system N2). Picks a skate line by appeal (seeded), rides its loop kinematically, lands the
 * feature trick and every converted prop on the way (each landing reports wear), reroutes when a line is
 * capped, blocked or guarded, bails at fresh props, scatters from the whistle and knocks over a guard who
 * stands in a committed run. Transfers between lines follow clear straight legs, hub points and the bank.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDSkaterBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDSkaterBrainComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Start skating: roll in from where the pawn stands. */
	void StartSession(int32 Seed, bool bFriend);
	void Scatter(const FVector& From, float Seconds);
	void GoHome();
	/** Showcase: head for this line now (ignored while committed to a run). */
	void PreferLine(AActor* NewLine);
	AActor* GetLine() const { return Line.Get(); }
	bool IsCommitted() const;
	float GetSecondsToPoint(const FVector& Point, float Radius) const;
	EPDSkaterMode GetMode() const { return Mode; }
	FVector GetMoveVelocity() const { return Velocity; }

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SkaterCruiseSpeed = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SkaterTrickSpeed = 520.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReturnSpeedFactor = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ScatterSpeed = 700.0f;
	/** Speed variation between skaters (fraction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpeedJitter = 0.08f;
	/** Committed this far before the feature (won't change its mind any more). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CommitDistance = 420.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockdownRadius = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BailSeconds = 0.9f;
	/** Pause on the loop start before a run (seeded range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D SkaterRestSeconds = FVector2D(0.4f, 1.4f);
	/** Prefer the line you are on (appeal bonus) so skaters don't flicker between lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StayBonus = 0.5f;
	/** Lines below this appeal are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinAppeal = 0.05f;
	/** Riders a line takes at once; every prop trick the skaters learned there adds one more. Full lines are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 LineCapacity = 2;
	/** Waiting for a free line: seconds before trying again (seeded range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D WaitSeconds = FVector2D(1.5f, 3.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GrindPopLength = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GrindPopExtra = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GrindExitLength = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GapArcHeight = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PropArcLength = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PropClearMargin = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LaunchHeightScale = 1.7f;
	/** Side-step around the guard, props and other skaters when not committed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AvoidRadius = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AvoidLookAhead = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxSideStep = 140.0f;
	/** Body radius used when checking a route leg is clear of fixed objects (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RouteClearance = 38.0f;
	/** Body radius for solid free running (cm) and how long a pinned runner waits before pushing on (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BodyRadius = 38.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckGiveUpSeconds = 1.5f;
	/** On a line, a runner slows to a stop when another is within this distance ahead on the path (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float QueueGap = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SideStepRate = 320.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LeanPerYawRate = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxLean = 14.0f;
	/** Plaza box the scatter run stays inside (centre XY, half extents XY). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector4 PlazaBox = FVector4(0.0f, 0.0f, 2080.0f, 2900.0f);
	/** Height that separates the upper deck from the lower plaza. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DeckLevelZ = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset PopSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset GrindSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Audio") FPDSynthPreset RailSound;

private:
	struct FTrick
	{
		int32 Key = 0;
		float StartD = 0.0f;
		float EndD = 0.0f;
		float GrindStart = 0.0f;
		float GrindEnd = 0.0f;
		float Height = 0.0f;
		float Wear = 0.0f;
		FText Name;
		TWeakObjectPtr<AActor> Prop;
		bool bGrind = false;
		bool bOnProp = false;
		bool bRide = false;
	};

	void CacheWorldPoints();
	AActor* ChooseLine(AActor* Exclude) const;
	void BeginTransferTo(AActor* NewLine);
	void BeginRide(float StartDistance);
	void BuildRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	void AppendLegs(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	bool IsClear(const FVector& A, const FVector& B) const;
	bool IsDeck(const FVector& P) const { return P.Z > DeckLevelZ; }
	void BuildTricks(TArray<FTrick>& Out) const;
	float TrickHeight(const FTrick& T, float D) const;
	float MoveAlong(TArray<FVector>& Route, int32& Index, float Distance, FVector& InOutPos) const;
	float ComputeSideStep(const FVector& Pos, const FVector& Fwd, bool bCommitted) const;
	/** Solid free-running move: sweeps the body From -> To and slides along obstacles. */
	FVector SolidStep(const FVector& From, const FVector& To, float Dt);
	bool FindBlock(const FVector& A, const FVector& B, FVector& OutPoint) const;
	bool AppendDetour(const FVector& From, const FVector& To, TArray<FVector>& Out) const;
	float StuckFor = 0.0f;
	void TickRide(float Dt);
	void TickTransfer(float Dt, bool bHome);
	void TickScatter(float Dt);
	void CheckKnockdown(const FVector& Pos, bool bCommitted);
	void SetPose(FName Action, bool bRestart = false);
	/** Poses that become plain locomotion (run / walk by speed) when the skater is on foot. */
	UPROPERTY(EditAnywhere, Category = "Tuning") TArray<FName> OnFootLocomotionPoses = { TEXT("Stance"), TEXT("Crouch") };
	void ApplyTransform(const FVector& GroundPos, float Yaw, float Height, float Dt, float BoardPitch);
	float PhaseSpeedScale() const;
	void Abandon(bool bBail);

	TWeakObjectPtr<AActor> Line;
	EPDSkaterMode Mode = EPDSkaterMode::Idle;
	FRandomStream Random;
	float SpeedScale = 1.0f;
	float S = 0.0f;
	float Rest = 0.0f;
	float StateTime = 0.0f;
	float ScatterLeft = 0.0f;
	float SideStep = 0.0f;
	/** Ambient walkers (tag PD.Ambient) the runners steer around. */
	TArray<TWeakObjectPtr<AActor>> Walkers;
	float LastYaw = 0.0f;
	float Lean = 0.0f;
	float PoseTimer = 0.0f;
	FVector ScatterDir = FVector::ZeroVector;
	FVector GroundPos = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	TSet<int32> Started;
	TSet<int32> Landed;
	int32 ActiveTrick = INDEX_NONE;
	bool bLoopDecided = false;

	TArray<FVector> Hubs;
	TArray<FVector> Entries;
	FVector RampBottom = FVector::ZeroVector;
	FVector RampTop = FVector::ZeroVector;
	bool bHasRamp = false;
	bool bCached = false;

	UPROPERTY() TObjectPtr<UPDAnimInstance> Anim;
	FName CurrentPose = NAME_None;
	float DebugTimer = 0.0f;
	bool bDebugLogged = false;
};
