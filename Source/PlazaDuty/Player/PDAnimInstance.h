// Copyright Plaza Duty. All Rights Reserved. (Locomotion core ported from Wrong Number / Colossus Surgeon, P38.)

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "PDAnimInstance.generated.h"

class UAnimSequence;

struct FPDAnimSample
{
	const UAnimSequence* Sequence = nullptr;
	double Time = 0.0;
	float Weight = 0.f;
	/** Upper-body layer sample: blended over the ground pose from UpperBodyRootBone up only. */
	bool bUpper = false;
};

/** Worker-thread proxy: evaluates and blends the samples prepared on the game thread. */
struct FPDAnimProxy : public FAnimInstanceProxy
{
	FPDAnimProxy() = default;
	explicit FPDAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	bool EvaluateLayers(FPoseContext& Output);
	void ApplyGrip(FPoseContext& Output);

	TArray<FPDAnimSample, TInlineAllocator<6>> Samples;
	float UpperAlpha = 0.f;
	float GroundSink = 0.f;
	FName UpperBodyRootBone;

	/** Hand grip (dragging a prop): torso lean plus two-bone arm IK onto component-space grip points. */
	float GripAlpha = 0.f;           // the strongest arm's blend (drives the torso lean)
	float ArmAlpha[2] = { 0.f, 0.f };
	bool bArmToMouth[2] = { false, false };
	FName HeadBone;
	FVector MouthOffset = FVector::ZeroVector;  // pawn frame: forward, right, up (cm)
	FVector PawnFwd = FVector::ForwardVector, PawnRight = FVector::RightVector, PawnUp = FVector::UpVector;
	float GripLeanDegrees = 0.f;
	FVector GripLeanAxis = FVector::RightVector;
	FVector GripTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector ElbowPole[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FName ArmBones[2][3];
};

/** One named full-body action clip (bolt, drag, knocked, skate stance, ollie...). */
USTRUCT(BlueprintType)
struct FPDActionClip
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") TSoftObjectPtr<UAnimSequence> Sequence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") bool bLoop = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0.05")) float PlayRate = 1.0f;
	/** Start offset into the clip (skip a lead-in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0")) float StartTime = 0.0f;
	/** Stop and hold at this time (0 = clip end). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0")) float HoldTime = 0.0f;
	/** Optional second pose mixed in (e.g. a little crouch in the riding stance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") TSoftObjectPtr<UAnimSequence> BlendSequence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", ClampMax = "1")) float BlendWeight = 0.0f;
	/** Play only on the upper body (spine and up); the legs keep the speed-matched walk/jog cycle so feet never slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") bool bUpperBodyOnly = false;
	/** How far this clip's planted ankle sits above the idle pose's (cm, Tools/Editor/probe_clips.py); the body is lowered by it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action") float GroundOffset = 0.0f;
	/** Riding life: the blend share oscillates by this much (knees pump), at this rate (Hz). 0 = static. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0", ClampMax = "1")) float BlendWobble = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action", meta = (ClampMin = "0")) float BlendWobbleRate = 0.0f;
};

/**
 * Native, graph-free AnimInstance for the Rocketbox humans (guard and skaters).
 * Ground locomotion plays one clip at a time, play rate matched to ground speed (measured reference speeds),
 * with a short crossfade. Named full-body actions (SetAction) blend over it: kneel to bolt, push to drag,
 * stagger when knocked down, and every skater pose (stance, crouch grind, pop, air, land).
 */
UCLASS(Blueprintable)
class PLAZADUTY_API UPDAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPDAnimInstance();

	/** Blend to a named action. Re-calling the same action keeps its time unless bRestart. */
	UFUNCTION(BlueprintCallable, Category = "PD|Animation") void SetAction(FName Action, bool bRestart = false, float BlendTime = -1.f);
	UFUNCTION(BlueprintCallable, Category = "PD|Animation") void ClearAction(float BlendTime = -1.f);
	UFUNCTION(BlueprintPure, Category = "PD|Animation") FName GetAction() const { return CurrentAction; }
	/** Multiplies the current action's play rate (dragging speed, skate push). */
	UFUNCTION(BlueprintCallable, Category = "PD|Animation") void SetActionRateScale(float Scale) { ActionRateScale = Scale; }
	/** Both hands grip these world points (two-bone arm IK over the locomotion pose) and the torso leans in by
	 *  LeanDegrees. Call every frame while holding something; bGrip false eases the arms back to the walk swing. */
	UFUNCTION(BlueprintCallable, Category = "PD|Animation") void SetHandGrip(bool bGrip, FVector LeftWorld, FVector RightWorld, float LeanDegrees);
	/** One arm's IK goal for this frame (0 = left, 1 = right): a world point, or the hand to the mouth (whistle).
	 *  Arms without a goal ease back to the clip. Lean only applies while a goal is active. */
	void SetArmGoal(int32 Side, bool bActive, const FVector& World, bool bToMouth = false);
	void SetGripLean(float LeanDegrees) { GripLean = LeanDegrees; }

	// ---- Hand grip (drag) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip", meta = (ClampMin = "0.01")) float GripBlendTime = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName LeftUpperArmBone = TEXT("Bip01-L-UpperArm");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName LeftForearmBone = TEXT("Bip01-L-Forearm");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName LeftHandBone = TEXT("Bip01-L-Hand");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName RightUpperArmBone = TEXT("Bip01-R-UpperArm");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName RightForearmBone = TEXT("Bip01-R-Forearm");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName RightHandBone = TEXT("Bip01-R-Hand");
	/** Elbow pole direction in the pawn's frame (X forward, Y outward from the body side, Z up): elbows out, back, down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FVector ElbowPoleLocal = FVector(-0.5f, 0.7f, -0.6f);
	/** Hand-to-mouth goal (whistle): this offset from the head bone in the pawn's frame (forward, right, up; cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FName HeadBoneName = TEXT("Bip01-Head");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Grip") FVector MouthOffsetLocal = FVector(12.0f, 0.0f, -9.0f);

	// ---- Locomotion clips (Quaternius UAL retargeted onto Rocketbox) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sequences") TSoftObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sequences") TSoftObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sequences") TSoftObjectPtr<UAnimSequence> JogAnim;
	/** Optional third tier above the jog (Shift run). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sequences") TSoftObjectPtr<UAnimSequence> SprintAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Sequences") TArray<FPDActionClip> Actions;

	/** Ground speed at which each cycle plays at 1x (cm/s, measured from the planted foot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float WalkReferenceSpeed = 149.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float JogReferenceSpeed = 289.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float SprintReferenceSpeed = 584.f;
	/** Planted-ankle height of the walk / jog cycles above the idle pose (cm, measured): the body is lowered by it so
	 *  every clip plants its feet at the same height the idle pose was grounded at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion") float WalkGroundOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion") float JogGroundOffset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "0")) float IdleSpeedThreshold = 10.f;
	/** The Quaternius clips retargeted onto the skeleton (SWAT_*) carry their root this much higher than the native
	 *  Rocketbox clips on these avatars (measured in game: idle ankles 98.9 vs 10.6 cm above the floor). Every
	 *  retargeted clip is lowered by it, so mixing the two families never lifts a body off the floor or the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Action") float RetargetedRootOffset = 88.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Action") FString RetargetedClipPrefix = TEXT("SWAT_");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "0")) float WalkStartSpeed = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "0.1")) float MinPlayRate = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "0.1")) float MaxPlayRate = 1.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "0")) float SpeedSmoothingTime = 0.18f;
	/** Middle of the walk -> jog crossfade (cm/s). Measured (stride / cycle time, probe_clips.py): fast walk 149,
	 *  jog 289, sprint 584 cm/s. The guard moves at 340 (jog at 1.18x) and 620 with Shift (sprint at 1.06x), drags at 170
	 *  (walk at 1.14x): every speed plays the clip whose stride matches it, so feet plant and knees keep their shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float CrossfadeSpeed = 215.f;
	/** Half-width of the walk/jog crossfade (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float CrossfadeHalfWidth = 35.f;
	/** Middle and half-width of the jog -> sprint crossfade (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float SprintCrossfadeSpeed = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Locomotion", meta = (ClampMin = "1")) float SprintCrossfadeHalfWidth = 50.f;
	/** First bone of the upper-body layer (it and its children take bUpperBodyOnly actions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Action") FName UpperBodyRootBone = TEXT("Bip01-Spine1");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Action", meta = (ClampMin = "0.01")) float DefaultActionBlend = 0.15f;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FPDAnimProxy;

	struct FLoadedAction
	{
		FName Name;
		TObjectPtr<UAnimSequence> Sequence;
		bool bLoop = true;
		float PlayRate = 1.f;
		float StartTime = 0.f;
		float HoldTime = 0.f;
		TObjectPtr<UAnimSequence> Blend;
		float BlendWeight = 0.f;
		bool bUpper = false;
		float GroundOffset = 0.f;
		float Wobble = 0.f;
		float WobbleRate = 0.f;
	};

	struct FActionLayer
	{
		int32 Index = INDEX_NONE;
		double Time = 0.0;
		float Alpha = 0.f;
	};

	void LoadSequences();
	void BuildSamples(float DeltaSeconds);
	double ActionTime(const FActionLayer& Layer) const;

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jog;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Sprint;
	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> ActionRefs;
	TArray<FLoadedAction> Loaded;

	TArray<FPDAnimSample, TInlineAllocator<6>> PendingSamples;
	float PendingUpperAlpha = 0.f;
	float PendingSink = 0.f;
	bool bArmWanted[2] = { false, false };
	bool bArmMouth[2] = { false, false };
	FVector GripWorld[2] = { FVector::ZeroVector, FVector::ZeroVector };
	float GripLean = 0.f;
	float ArmAlphaNow[2] = { 0.f, 0.f };
	/** Per-instance phase so a crowd never pumps in sync. */
	float LifePhase = 0.f;
	/** Current action on top, the previous one fading out under it. */
	FActionLayer Current;
	FActionLayer Fading;
	FName CurrentAction = NAME_None;
	float BlendTime = 0.15f;
	float ActionRateScale = 1.f;
	float SmoothedSpeed = 0.f;
	float LocomotionPhase = 0.f;
	double IdleTime = 0.0;
	bool bLoaded = false;
};
