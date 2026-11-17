// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PDTypes.h"
#include "Audio/PDSynth.h"
#include "PDGuardComponents.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDFocusChanged, AActor*, NewFocus);

/**
 * Finds what the guard would use with E right now (P2): the nearest knob socket or loose prop in range,
 * preferring what is in front. Fires each first-time prompt once through the match flow.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDInteractComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "PD|Interact") AActor* GetFocus() const { return Focus.Get(); }
	EPDInteractKind GetFocusKind() const;
	/** Best target of a kind within range of a point (the bot asks this too). */
	AActor* FindBest(EPDInteractKind Kind, const FVector& From, const FVector& Facing) const;
	void ClearFocus();

	UPROPERTY(BlueprintAssignable, Category = "PD|Interact") FPDFocusChanged OnFocusChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoltRange = 190.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GrabRange = 210.0f;
	/** Score bonus (uu) for targets in front of the guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FacingBonus = 60.0f;
	/** First-time prompts fire a little before the guard is in range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PromptRange = 420.0f;

private:
	TWeakObjectPtr<AActor> Focus;
};

/**
 * Hold-to-bolt (P32): kneel at a knob socket for BoltHoldSeconds, right in the skate traffic. Releasing E,
 * moving away or being knocked down resets it. Draws the amber progress ring at the guard's feet.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDBoltComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDBoltComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool StartBolt(AActor* Line);
	/** bInterrupted = knocked down (logged + prompt), otherwise released. */
	void CancelBolt(bool bInterrupted);
	UFUNCTION(BlueprintPure, Category = "PD|Bolt") bool IsBolting() const { return Target.IsValid(); }
	UFUNCTION(BlueprintPure, Category = "PD|Bolt") float GetProgress() const { return Progress; }
	AActor* GetTarget() const { return Target.Get(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.2")) float BoltHoldSeconds = 3.0f;
	/** Seconds to step in and kneel at the stand point before the clock starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float StepInSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.05")) float RatchetInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "1")) int32 RingSegments = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RingRadius = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FPDSynthPreset RatchetSound;

	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> SegmentMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> SegmentMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> SegmentTrackMaterial;

private:
	void UpdateRing();

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segments;
	TWeakObjectPtr<AActor> Target;
	FVector StepFrom = FVector::ZeroVector;
	FVector StepTo = FVector::ZeroVector;
	float StepAge = 0.0f;
	float Progress = 0.0f;
	float RatchetTimer = 0.0f;
};

/** Grab and drag a loose prop (P25): the prop slides in front of the guard, who slows down and pushes. */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDDragComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDDragComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool Grab(AActor* Prop);
	void Release();
	UFUNCTION(BlueprintPure, Category = "PD|Drag") bool IsDragging() const { return Held.IsValid(); }
	AActor* GetHeld() const { return Held.Get(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.1", ClampMax = "1")) float DragSpeedFactor = 0.5f;
	/** Gap between the guard's capsule and the prop's footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HoldGap = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FPDSynthPreset GrabSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FPDSynthPreset DropSound;

	/** Where both hands hold the prop (world) and how far the torso leans. False when nothing is held. */
	bool GetGrip(FVector& OutLeft, FVector& OutRight, float& OutLeanDegrees) const;
	/** True while the held prop is low enough to be lifted and carried rather than dragged on the floor. */
	bool IsCarrying() const { return bCarrying; }

	// ---- Grip (D32; values provisional, C8) ----
	/** Props whose top sits below this (cm above the soles) are carried: lifted to waist height, hands on their sides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryBelowTopHeight = 75.0f;
	/** A carried prop's middle is held at this height above the soles (the hands' height, elbows bent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryHandHeight = 104.0f;
	/** Gap between the capsule and a carried prop (held close to the body). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryGap = -12.0f;
	/** Hands sit this far into a carried prop from its near face (cm), and this far outside its side faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryHandDepth = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryHandSideOffset = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float CarryLeanDegrees = 2.0f;
	/** A dragged (tall) prop: hands on its near top edge, this far past the near face, this far apart at most. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float GripInset = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip", meta = (ClampMin = "0")) float GripHalfSpread = 19.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") float DragLeanDegrees = 12.0f;

private:
	struct FPDPropBox { FVector Center, AxisX, AxisY, Extent; };
	static bool PropBox(const AActor* Prop, FPDPropBox& Out);
	/** World height of the holder's soles. */
	float SoleZ() const;
	TWeakObjectPtr<AActor> Held;
	float CarryLift = 0.0f;
	bool bCarrying = false;
};

/** The whistle: scatters every skater near the guard for a few seconds, then recharges. */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDWhistleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDWhistleComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Returns the number of skaters scattered, or -1 if still recharging. */
	int32 Blow();
	UFUNCTION(BlueprintPure, Category = "PD|Whistle") bool IsReady() const { return Cooldown <= 0.0f; }
	/** 0 = just blown, 1 = ready. */
	UFUNCTION(BlueprintPure, Category = "PD|Whistle") float GetCharge() const { return 1.0f - FMath::Clamp(Cooldown / FMath::Max(0.01f, WhistleCooldown), 0.0f, 1.0f); }
	int32 CountInRange() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "50")) float WhistleRadius = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.5")) float WhistleScatterSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.5")) float WhistleCooldown = 12.0f;
	/** Prompt "Q WHISTLE" when this many skaters are in range and it is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "1")) int32 PromptCrowd = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FPDSynthPreset ReadySound;

private:
	float Cooldown = 0.0f;
	float PromptTimer = 0.0f;
};
