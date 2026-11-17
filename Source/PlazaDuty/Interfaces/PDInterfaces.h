// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/PDTypes.h"
#include "PDInterfaces.generated.h"

class UPDMatchConfig;
class UPDMatchEvents;

/** A converted prop riding on a skate line: where along the path it sits and which trick it became. */
struct FPDPropTrick
{
	TWeakObjectPtr<AActor> Prop;
	float PathDistance = 0.0f;
	FText TrickName;
	float Wear = 0.0f;
	float ClearHeight = 0.0f;
	float RideLength = 0.0f;
	bool bLaunch = false;
};

// ------------------------------------------------------------------------------------------------
// Match flow (implemented by the GameMode). Everything reads the match through this, never by casting.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDMatchFlow : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDMatchFlow
{
	GENERATED_BODY()

public:
	virtual EPDMatchState GetMatchState() const = 0;
	virtual const UPDMatchConfig* GetMatchConfig() const = 0;
	virtual UPDMatchEvents* GetMatchEvents() const = 0;
	virtual float GetElapsed() const = 0;
	virtual float GetTimeRemaining() const = 0;
	/** The closing clock players see (the showcase runs it faster than real time; otherwise the same as GetTimeRemaining). */
	virtual float GetClockRemaining() const { return GetTimeRemaining(); }
	virtual float GetCountdownRemaining() const = 0;
	virtual float GetWear() const = 0;
	virtual float GetWearLimit() const = 0;
	/** Wear per second over the last few seconds (HUD trend, bot). */
	virtual float GetWearRate() const = 0;
	virtual int32 GetKnobsLeft() const = 0;
	virtual int32 GetPhaseIndex() const = 0;
	virtual const FPDPhaseRow& GetPhase() const = 0;
	virtual const FPDMatchStats& GetMatchStats() const = 0;
	virtual FText GetEndReason() const = 0;
	virtual bool IsAutoplayRun() const = 0;
	virtual int32 GetMatchSeed() const = 0;
	virtual void RequestRestart() = 0;

	virtual TArray<AActor*> GetSkaters() const = 0;
	virtual TArray<AActor*> GetSkateLines() const = 0;
	virtual TArray<AActor*> GetProps() const = 0;
	virtual const TArray<FVector>& GetDeadZones() const = 0;
	virtual AActor* GetGuard() const = 0;

	/** Gameplay reports (all broadcast through UPDMatchEvents). */
	virtual void ReportTrick(const FPDTrickEvent& Trick) = 0;
	virtual bool ConsumeKnob() = 0;
	virtual void ReportLineCapped(AActor* Line) = 0;
	virtual void ReportPropConverted(AActor* Line, AActor* Prop, const FText& TrickName) = 0;
	virtual void ReportPropMoved(AActor* Prop, bool bOnLine) = 0;
	virtual void ReportWhistle(AActor* Guard, int32 Scattered) = 0;
	virtual void ReportKnockdown(AActor* Guard, AActor* Skater) = 0;
	virtual void ReportPrompt(EPDPrompt Prompt, AActor* Anchor) = 0;
};

// ------------------------------------------------------------------------------------------------
// Skate line: a fixed route through one plaza feature.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDSkateLine : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDSkateLine
{
	GENERATED_BODY()

public:
	virtual FName GetLineId() const = 0;
	virtual FText GetLineName() const = 0;
	virtual FText GetTrickName() const = 0;
	virtual EPDTrickStyle GetTrickStyle() const = 0;
	/** World-space loop: Path[0] .. Path[Num-1] then back to Path[0]. */
	virtual const TArray<FVector>& GetPath() const = 0;
	virtual float GetPathLength() const = 0;
	/** Distance along the path where the feature starts / ends, and where the run ends (after the landing). */
	virtual float GetFeatureStartDistance() const = 0;
	virtual float GetFeatureEndDistance() const = 0;
	virtual float GetRunEndDistance() const = 0;
	/** Position on the loop at a distance (wraps). */
	virtual FVector GetPointAtDistance(float Distance) const = 0;
	/** Closest distance along the loop to a world point. */
	virtual float GetDistanceClosestTo(const FVector& World) const = 0;
	virtual bool IsOpen() const = 0;
	virtual bool IsCapped() const = 0;
	virtual bool IsCappable() const = 0;
	/** A fresh prop sits on the line and the skaters haven't worked it out yet. */
	virtual bool IsBlocked() const = 0;
	/** How much skaters want this line right now (0 = dead). */
	virtual float GetAppeal() const = 0;
	virtual float GetBaseWear() const = 0;
	/** Expected wear per run with the current props (bot + HUD). */
	virtual float GetWearPerRun() const = 0;
	virtual const TArray<FPDPropTrick>& GetPropTricks() const = 0;
	/** 0..1 recent traffic (lane glow). */
	virtual float GetHeat() const = 0;
	virtual void NotifyRun(AActor* Skater) = 0;
	virtual void NotifyTrickLanded(float Wear) = 0;
	/** Where the knob goes and where the guard kneels to bolt it. */
	virtual FVector GetKnobPoint() const = 0;
	virtual FVector GetBoltStandPoint() const = 0;
	virtual void Bolt() = 0;
	/** Showcase: multiply the line's opening time (lines that open later in the shift). */
	virtual void ScaleOpeningTime(float Scale) = 0;
	/** Height of the feature top above the path (grind height). */
	virtual float GetFeatureHeight() const = 0;
	/** True if the line starts on the upper deck (needs the bank to get back up). */
	virtual bool StartsOnDeck() const = 0;
};

// ------------------------------------------------------------------------------------------------
// Things the guard can focus and use with E.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDInteractable : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDInteractable
{
	GENERATED_BODY()

public:
	virtual EPDInteractKind GetInteractKind() const = 0;
	virtual bool CanInteract(const AActor* Guard) const = 0;
	/** Point the guard measures range to and faces. */
	virtual FVector GetInteractPoint() const = 0;
	/** Small caps tag shown next to the target ("BOLT", "DRAG"). */
	virtual FText GetInteractLabel() const = 0;
	/** Focus highlight on/off (amber ring). */
	virtual void SetFocused(bool bFocused) = 0;
};

// ------------------------------------------------------------------------------------------------
// Loose props.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDDraggable : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDDraggable
{
	GENERATED_BODY()

public:
	virtual EPDPropKind GetPropKind() const = 0;
	virtual bool IsHeld() const = 0;
	virtual void BeginDrag(AActor* Guard) = 0;
	/** Move the held prop toward a target ground point (swept, so it stops at walls). */
	virtual void DragTo(const FVector& Target, float Yaw) = 0;
	/** Extra height while held, so a low prop's rim comes up to the holder's hands (cm). */
	virtual void SetCarryLift(float Cm) {}
	virtual void EndDrag() = 0;
	/** Set by the line that learned it; cleared when the prop is dragged off the line. */
	virtual void SetConverted(bool bConverted, const FText& TrickName) = 0;
	virtual bool IsConverted() const = 0;
	virtual float GetFootprintRadius() const = 0;
	/** Seconds this prop has sat still where it is now (0 while held or moving). */
	virtual float GetRestSeconds() const = 0;
};

// ------------------------------------------------------------------------------------------------
// The guard can be knocked over by a skater.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDKnockable : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDKnockable
{
	GENERATED_BODY()

public:
	/** Returns true if the hit landed (not already down / still recovering). */
	virtual bool Knockdown(AActor* By) = 0;
	virtual bool IsKnockedDown() const = 0;
	virtual EPDGuardAction GetGuardAction() const = 0;
};

// ------------------------------------------------------------------------------------------------
// Skaters: scatter from the whistle, report their plans to the bot and the HUD.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDSkaterAgent : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDSkaterAgent
{
	GENERATED_BODY()

public:
	virtual void Scatter(const FVector& From, float Seconds) = 0;
	virtual AActor* GetTargetLine() const = 0;
	/** True once the skater is on its run toward the feature and won't change its mind. */
	virtual bool IsCommitted() const = 0;
	/** Seconds until the skater passes within Radius of Point on its current plan (-1 = not on its way). */
	virtual float GetSecondsToPoint(const FVector& Point, float Radius) const = 0;
	/** Leave the plaza (closing time). */
	virtual void GoHome() = 0;
	/** Showcase: send this skater to a line now if it is free to go. */
	virtual void PreferLine(AActor* Line) = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDInputVisualized, FName, KeyId, bool, bSimulated);

// ------------------------------------------------------------------------------------------------
// The guard's controls. Humans press keys; the autoplay bot calls the same entry points, so the legend
// lights and every sound plays exactly as for a human.
// ------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPDGuardInput : public UInterface
{
	GENERATED_BODY()
};

class PLAZADUTY_API IPDGuardInput
{
	GENERATED_BODY()

public:
	virtual void InputMove(const FVector& WorldDirection, float Scale, bool bSimulated) = 0;
	virtual void InputRun(bool bRun, bool bSimulated) = 0;
	virtual void InputInteractPressed(bool bSimulated) = 0;
	virtual void InputInteractReleased(bool bSimulated) = 0;
	virtual void InputWhistle(bool bSimulated) = 0;
	virtual bool IsRunHeld() const = 0;
	virtual FPDInputVisualized& GetInputVisualized() = 0;
};

namespace PD
{
	/** The match flow of the world (the GameMode), or null. */
	PLAZADUTY_API IPDMatchFlow* GetFlow(const UObject* WorldContext);
}
