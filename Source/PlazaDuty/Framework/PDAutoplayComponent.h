// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PDAutoplayComponent.generated.h"

UENUM()
enum class EPDBotGoal : uint8
{
	Idle,
	Bolt,
	Clear,
	Retreat,
	Guard
};

/**
 * Autoplay bot (P22 + P46). Plays the guard through IPDGuardInput (the same entry points as a human, so the
 * legend lights and sounds play): bolts the line making the most wear when no skater will reach the knob
 * before the bolt finishes, whistles crowds away, and drags converted props back to the walls.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDAutoplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDAutoplayComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void StartBot(bool bInPassive, int32 Seed);
	bool IsBotActive() const { return bActive; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DecisionInterval = 0.25f;
	/** Start bolting only if no skater reaches the knob for this long beyond the bolt itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoltSafetyMargin = 0.5f;
	/** A skater this close in time aborts a bolt in progress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AbortSeconds = 0.8f;
	/** Path radius a skater must pass within to count as a threat to the kneeling guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ThreatRadius = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 WhistleCrowd = 2;
	/** Clear a converted prop first when its trick is worth at least this much wear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ClearFirstWear = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArriveRadius = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunDistance = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RepathDistance = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WaitOffset = 170.0f;
	/** Stuck: moved less than this in StuckWindow seconds while trying to move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckDistance = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StuckWindow = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector IdlePoint = FVector(300.0f, 560.0f, 0.0f);
	/** QA (-PDDemoProps): early in the shift, drag the nearest prop to this point (top of the stairs) to show block -> launch ramp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector DemoDropPoint = FVector(730.0f, 150.0f, 150.0f);
	/** The demo prop is left in place this long after the drop so the runners launch off it and call a friend (showcase). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DemoLeaveSeconds = 16.0f;
	/** Showcase (-PDShowcase, the video) plays the game's best side (user, 2026-11-09: "only show the good things"):
	 *  no long clearing carries across the plaza, and at a busy line the bot whistles the runners off and bolts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") bool bShowcaseClearProps = false;
	/** ...except the demo prop, which is moved a short way to this spot (off every line) once it has been used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") FVector ShowcaseClearPoint = FVector(1150.0f, 650.0f, 150.0f);
	/** After seating a knob the bot steps out of the lane for this long (no knock-downs on camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float StepOutSeconds = 1.2f;
	/** Showcase: a bolt already this close (cm) is finished before anything else; nearer lines score higher. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcaseFinishBoltRadius = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") float ShowcaseBoltDistanceFalloff = 1800.0f;
	/** Line distance (cm) that halves a line's bolt score in normal play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoltDistanceFalloff = 6000.0f;
	/** Never grab a prop a runner reaches sooner than this (s): whistle them off or wait (no knock-downs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GrabSafetySeconds = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Showcase") bool bShowcaseWhistleToBolt = true;
	/** The bot never whistles (user 2026-11-17: "don't use the whistle"); the player still can. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bUseWhistle = false;

private:
	APawn* GetGuardPawn() const;
	void Decide();
	void Execute(float Dt);
	bool MoveTo(const FVector& Point, float Dt, float Tolerance);
	float BoltDanger(const FVector& Stand) const;
	AActor* PickBoltLine() const;
	AActor* PickPropToClear(float& OutWear) const;
	FVector NearestDeadZone(const FVector& From) const;

	EPDBotGoal Goal = EPDBotGoal::Idle;
	TWeakObjectPtr<AActor> Target;
	FVector GoalPoint = FVector::ZeroVector;
	FVector DropPoint = FVector::ZeroVector;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	FVector PathGoal = FVector(1e9f);
	float PathAge = 0.0f;
	bool bShowcase = false;
	bool bWasBolting = false;
	TWeakObjectPtr<AActor> BoltingLine;
	float DecisionTimer = 0.0f;
	float RetreatLeft = 0.0f;
	float StuckTimer = 0.0f;
	float SidestepLeft = 0.0f;
	float SidestepSign = 1.0f;
	FVector StuckFrom = FVector::ZeroVector;
	FRandomStream Random;
	bool bActive = false;
	bool bPassive = false;
	bool bHoldingBolt = false;
	bool bDemoProps = false;
	bool bDemoActive = false;
	bool bDemoDone = false;
	TWeakObjectPtr<AActor> DemoProp;
	float DemoDroppedAt = -1.0f;
};
