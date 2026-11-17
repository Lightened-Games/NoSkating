// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PDTypes.generated.h"

/** The strict 4-state loop: Ready (3-2-1-GO) -> Active -> Victory | Defeat -> Restart [R]. */
UENUM(BlueprintType)
enum class EPDMatchState : uint8
{
	Ready,
	Active,
	Victory,
	Defeat
};

/** Loose plaza furniture the guard can drag. Each kind turns into a different trick once the skaters learn it. */
UENUM(BlueprintType)
enum class EPDPropKind : uint8
{
	Cone,
	Bench,
	Planter,
	TrashCan
};

/** How a skate line's main feature is skated. */
UENUM(BlueprintType)
enum class EPDTrickStyle : uint8
{
	/** Ride along the top of the feature (ledge, rail, curb). */
	Grind,
	/** Ollie off the top and land below (stair gap). */
	Gap
};

/** What the guard is doing with the E key right now (HUD prompt + animation). */
UENUM(BlueprintType)
enum class EPDGuardAction : uint8
{
	None,
	Bolting,
	Dragging,
	Whistling,
	KnockedDown
};

/** Which kind of target the guard is focused on. */
UENUM(BlueprintType)
enum class EPDInteractKind : uint8
{
	None,
	Bolt,
	Drag
};

/** First-time prompts: each fires once per match (8.7.6 rule 2). */
UENUM(BlueprintType)
enum class EPDPrompt : uint8
{
	Bolt,
	Drag,
	Converted,
	Friend,
	Whistle,
	Knockdown,
	Capped
};

/** Tuning for one prop kind (lives in the match config data asset). */
USTRUCT(BlueprintType)
struct FPDPropTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") EPDPropKind Kind = EPDPropKind::Cone;
	/** Short name shown on the world tag ("CONE"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FText DisplayName;
	/** Trick the skaters invent on it once it has sat on their line long enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FText TrickName;
	/** Wear added each time that trick is landed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0")) float TrickWear = 3.0f;
	/** Trick and wear when the prop sits at the take-off of a drop line (a planter at the stair top becomes a launch ramp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FText LaunchTrickName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0")) float LaunchWear = 0.0f;
	/** Seconds a prop on a line blocks it before the skaters work it out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0.5")) float BlockSeconds = 6.0f;
	/** Extra appeal a converted prop gives its line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0")) float AppealBonus = 1.5f;
	/** Height the skater clears when tricking it (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0")) float ClearHeight = 70.0f;
	/** Ride along the top for this many uu (bench manual). 0 = ollie over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop", meta = (ClampMin = "0")) float RideLength = 0.0f;
};

/** One escalation phase of the 90 s shift. */
USTRUCT(BlueprintType)
struct FPDPhaseRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0")) float StartTime = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") FText Name;
	/** Banner sub-line when the phase begins (empty = no banner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase") FText BannerText;
	/** The director tops the plaza up to this many skaters (friends come on top, up to the cap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0")) int32 SkaterTarget = 3;
	/** Multiplies skater cruise speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.1")) float SpeedScale = 1.0f;
	/** Multiplies the appeal of converted props. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0")) float PropAppealScale = 1.0f;
	/** Seconds between friend arrivals in this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0")) float FriendCooldown = 8.0f;
};

/** End-of-shift statistics (end screen + telemetry). */
USTRUCT(BlueprintType)
struct FPDMatchStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats") float Elapsed = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float FinalWear = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Tricks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 PropTricks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 KnobsBolted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Whistles = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 PropsPlaced = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 PropsConverted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 PropsCleared = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Friends = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 Knockdowns = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") int32 PeakSkaters = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stats") float PeakWearRate = 0.0f;
};

/** A landed trick (HUD popup, audio, juice, telemetry). */
USTRUCT(BlueprintType)
struct FPDTrickEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trick") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Trick") FText TrickName;
	UPROPERTY(BlueprintReadOnly, Category = "Trick") FName LineId;
	UPROPERTY(BlueprintReadOnly, Category = "Trick") float Wear = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Trick") bool bOnProp = false;
	UPROPERTY(BlueprintReadOnly, Category = "Trick") TObjectPtr<AActor> Skater = nullptr;
};

namespace PD
{
	// Actor tags (classification without casts).
	static const FName SkaterTag(TEXT("PD.Skater"));
	/** Visible but intentionally walk-through (markers, decals, FX): skipped by the FactoryQA collision audit. */
	static const FName NoCollideTag(TEXT("NoCollide"));
	static const FName GuardTag(TEXT("PD.Guard"));
	static const FName PropTag(TEXT("PD.Prop"));
	static const FName LineTag(TEXT("PD.SkateLine"));
	static const FName DeadZoneTag(TEXT("PD.DeadZone"));
	static const FName SkaterEntryTag(TEXT("PD.SkaterEntry"));
	static const FName RampTag(TEXT("PD.Ramp"));
	static const FName HubTag(TEXT("PD.Hub"));
	static const FName ClosingLampTag(TEXT("PD.ClosingLamp"));

	// Legend key ids (HUD rows light on input, the bot fires them too).
	static const FName KeyMove(TEXT("Move"));
	static const FName KeyRun(TEXT("Run"));
	static const FName KeyLook(TEXT("Look"));
	static const FName KeyInteract(TEXT("Interact"));
	static const FName KeyWhistle(TEXT("Whistle"));
	static const FName KeyRestart(TEXT("Restart"));
}
