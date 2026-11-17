// Copyright Plaza Duty. All Rights Reserved.

#include "Player/PDAnimInstance.h"

#include "TwoBoneIK.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "GameFramework/Pawn.h"

namespace PDAnim
{
	const TCHAR* AnimDir = TEXT("/Game/PlazaDuty/Characters/Rocketbox/Anims/");

	TSoftObjectPtr<UAnimSequence> Clip(const TCHAR* Name)
	{
		return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%sSWAT_%s.SWAT_%s"), AnimDir, Name, Name)));
	}

	/** Native Microsoft Rocketbox mocap (MIT) on the avatars' own Bip01 biped: no retarget, so knees keep their captured angles. */
	TSoftObjectPtr<UAnimSequence> RBClip(const TCHAR* Name)
	{
		return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%sRB_%s.RB_%s"), AnimDir, Name, Name)));
	}

	FPDActionClip Action(const TCHAR* ActionName, const TCHAR* ClipName, bool bLoop, float Rate = 1.f, float Start = 0.f, float Hold = 0.f, float GroundOffset = 0.f)
	{
		FPDActionClip A;
		A.GroundOffset = GroundOffset;
		A.Name = ActionName;
		A.Sequence = FString(ClipName).StartsWith(TEXT("RB:")) ? RBClip(ClipName + 3) : Clip(ClipName);
		A.bLoop = bLoop;
		A.PlayRate = Rate;
		A.StartTime = Start;
		A.HoldTime = Hold;
		return A;
	}

	double Wrap(double Time, const UAnimSequence* Seq)
	{
		const double Len = Seq ? FMath::Max<double>(Seq->GetPlayLength(), KINDA_SMALL_NUMBER) : 1.0;
		return FMath::Fmod(FMath::Max(0.0, Time), Len);
	}
}

UPDAnimInstance::UPDAnimInstance()
{
	// Locomotion: native Rocketbox mocap (idle with the feet side by side, fast walk, fast run).
	IdleAnim = PDAnim::RBClip(TEXT("Idle"));
	WalkAnim = PDAnim::RBClip(TEXT("WalkFast"));
	JogAnim = PDAnim::RBClip(TEXT("Run"));
	SprintAnim = PDAnim::RBClip(TEXT("RunFast"));
	Actions = {
		// Guard
		PDAnim::Action(TEXT("Bolt"), TEXT("Fixing_Kneeling"), true),
		PDAnim::Action(TEXT("Knocked"), TEXT("Hit_Chest"), false, 0.8f),
		PDAnim::Action(TEXT("Whistle"), TEXT("Interact"), false, 1.3f),
		PDAnim::Action(TEXT("Victory"), TEXT("RB:Cheer"), true),
		PDAnim::Action(TEXT("Defeat"), TEXT("RB:Angry"), true),
		// Skaters: the rider stands sideways on the deck, feet side by side along the board (native Rocketbox idle).
		PDAnim::Action(TEXT("Stance"), TEXT("RB:Idle"), true),  // + a share of the crouch below
		PDAnim::Action(TEXT("Crouch"), TEXT("RB:Idle"), true),  // grind: deeper crouch share below
		// Skip the deep crouch wind-up: from the side it read as a hunched lunge (round 3 review, 2026-11-10).
		PDAnim::Action(TEXT("Pop"), TEXT("Jump_Start"), false, 1.6f, 0.22f),
		PDAnim::Action(TEXT("Air"), TEXT("Jump_Loop"), true),
		PDAnim::Action(TEXT("Land"), TEXT("Jump_Land"), false, 1.2f, 0.15f),
		PDAnim::Action(TEXT("Bail"), TEXT("Hit_Chest"), false, 1.0f),
		PDAnim::Action(TEXT("Cheer"), TEXT("RB:Cheer"), true, 1.f, 2.f),
		// Ambient people (v2 life pass): native Rocketbox seated and look-around loops.
		PDAnim::Action(TEXT("Sit"), TEXT("RB:SitIdle"), true),
		PDAnim::Action(TEXT("SitLook"), TEXT("RB:SitLook"), true),
		PDAnim::Action(TEXT("SitF"), TEXT("RB:SitIdleF"), true),
		PDAnim::Action(TEXT("LookAround"), TEXT("RB:LookAround"), true),
		PDAnim::Action(TEXT("LookAroundF"), TEXT("RB:LookAroundF"), true),
	};
	for (FPDActionClip& A : Actions)
	{
		if (A.Name == TEXT("Stance"))
		{
			// Riding stance: knees bent, weight low. The crouch clip splits the feet 35 cm front-to-back, so only a
			// share is mixed in to keep both feet on the deck (Tools/Editor/probe_clips.py: feet0).
			A.BlendSequence = PDAnim::RBClip(TEXT("Crouch"));
			A.BlendWeight = 0.3f;
			// Knees pump slowly while rolling (0.12-0.48 crouch share), so the rider is never a statue on the board.
			A.BlendWobble = 0.18f;
			A.BlendWobbleRate = 0.55f;
		}
		if (A.Name == TEXT("Crouch"))
		{
			A.BlendSequence = PDAnim::RBClip(TEXT("Crouch"));
			A.BlendWeight = 0.55f;
		}
		if (A.Name == TEXT("Whistle"))
		{
			// The guard often whistles on the move: the gesture rides on the upper body, the legs keep running
			// (full-body it froze the legs mid-stride and slid them; FactoryQA leg sheet 2026-11-09).
			A.bUpperBodyOnly = true;
		}
	}
}

void UPDAnimInstance::LoadSequences()
{
	Idle = IdleAnim.LoadSynchronous();
	Walk = WalkAnim.LoadSynchronous();
	Jog = JogAnim.LoadSynchronous();
	Sprint = SprintAnim.IsNull() ? nullptr : SprintAnim.LoadSynchronous();
	Loaded.Reset();
	ActionRefs.Reset();
	for (const FPDActionClip& A : Actions)
	{
		FLoadedAction& L = Loaded.AddDefaulted_GetRef();
		L.Name = A.Name;
		L.Sequence = A.Sequence.LoadSynchronous();
		L.bLoop = A.bLoop;
		L.PlayRate = A.PlayRate;
		L.StartTime = A.StartTime;
		L.HoldTime = A.HoldTime;
		L.Blend = A.BlendSequence.IsNull() ? nullptr : A.BlendSequence.LoadSynchronous();
		L.BlendWeight = A.BlendWeight;
		L.bUpper = A.bUpperBodyOnly;
		L.GroundOffset = A.GroundOffset;
		L.Wobble = A.BlendWobble;
		L.WobbleRate = A.BlendWobbleRate;
		if (L.Sequence && L.Sequence->GetName().StartsWith(RetargetedClipPrefix))
		{
			L.GroundOffset += RetargetedRootOffset;
		}
		ActionRefs.Add(L.Sequence);
		ActionRefs.Add(L.Blend);
	}
	bLoaded = true;
}

void UPDAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	LoadSequences();
	LifePhase = FMath::FRand() * 2.f * PI;
}

void UPDAnimInstance::SetAction(FName Action, bool bRestart, float InBlendTime)
{
	if (!bLoaded)
	{
		LoadSequences();
	}
	BlendTime = InBlendTime >= 0.f ? InBlendTime : DefaultActionBlend;
	if (Action == CurrentAction && Current.Index != INDEX_NONE)
	{
		if (bRestart)
		{
			Current.Time = 0.0;
		}
		return;
	}
	const int32 Index = Loaded.IndexOfByPredicate([Action](const FLoadedAction& L) { return L.Name == Action; });
	Fading = Current;
	Current = FActionLayer();
	Current.Index = Index;
	Current.Alpha = 0.f;
	CurrentAction = Index != INDEX_NONE ? Action : NAME_None;
	ActionRateScale = 1.f;
}

void UPDAnimInstance::ClearAction(float InBlendTime)
{
	BlendTime = InBlendTime >= 0.f ? InBlendTime : DefaultActionBlend;
	if (Current.Index != INDEX_NONE)
	{
		Fading = Current;
	}
	Current = FActionLayer();
	CurrentAction = NAME_None;
	ActionRateScale = 1.f;
}

double UPDAnimInstance::ActionTime(const FActionLayer& Layer) const
{
	if (!Loaded.IsValidIndex(Layer.Index) || !Loaded[Layer.Index].Sequence)
	{
		return 0.0;
	}
	const FLoadedAction& A = Loaded[Layer.Index];
	const double Len = A.Sequence->GetPlayLength();
	const double T = A.StartTime + Layer.Time;
	if (A.bLoop)
	{
		return PDAnim::Wrap(T, A.Sequence);
	}
	const double End = A.HoldTime > 0.f ? FMath::Min<double>(A.HoldTime, Len) : Len;
	return FMath::Min(T, End);
}

void UPDAnimInstance::SetHandGrip(bool bGrip, FVector LeftWorld, FVector RightWorld, float LeanDegrees)
{
	SetArmGoal(0, bGrip, LeftWorld);
	SetArmGoal(1, bGrip, RightWorld);
	if (bGrip)
	{
		GripLean = LeanDegrees;
	}
}

void UPDAnimInstance::SetArmGoal(int32 Side, bool bActive, const FVector& World, bool bToMouth)
{
	if (Side < 0 || Side > 1)
	{
		return;
	}
	bArmWanted[Side] = bActive;
	if (bActive)
	{
		GripWorld[Side] = World;
		bArmMouth[Side] = bToMouth;
	}
}

void UPDAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	BuildSamples(DeltaSeconds);
	// Dragging grips the prop with both hands (two-bone IK) instead of a retargeted push clip: the UAL push pose
	// raised the guard's hands above his head on the Rocketbox rig (user screenshot, 2026-11-09).
	const float GripStep = DeltaSeconds / FMath::Max(0.01f, GripBlendTime);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		ArmAlphaNow[Side] = FMath::Clamp(ArmAlphaNow[Side] + (bArmWanted[Side] ? GripStep : -GripStep), 0.f, 1.f);
	}
	if (!bArmWanted[0] && !bArmWanted[1])
	{
		GripLean = FMath::FInterpTo(GripLean, 0.f, DeltaSeconds, 8.f);  // lean eases out with the arms
	}
}

void UPDAnimInstance::BuildSamples(float DeltaSeconds)
{
	PendingSamples.Reset();
	if (!bLoaded)
	{
		LoadSequences();
	}

	const APawn* Pawn = TryGetPawnOwner();
	const float RawSpeed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
	const float SmoothAlpha = SpeedSmoothingTime > 0.f ? FMath::Clamp(DeltaSeconds / SpeedSmoothingTime, 0.f, 1.f) : 1.f;
	SmoothedSpeed = FMath::Lerp(SmoothedSpeed, RawSpeed, SmoothAlpha);
	const float S = SmoothedSpeed;

	// ---- Ground: one clip at a time with a short crossfade (no 50/50 blends that scissor the legs) ----
	float WIdle = 0.f, WWalk = 0.f, WJog = 0.f, WSprint = 0.f;
	if (S <= IdleSpeedThreshold)
	{
		WIdle = 1.f;
	}
	else if (S < WalkStartSpeed)
	{
		const float T = FMath::SmoothStep(IdleSpeedThreshold, WalkStartSpeed, S);
		WIdle = 1.f - T;
		WWalk = T;
	}
	else
	{
		const float T = FMath::SmoothStep(CrossfadeSpeed - CrossfadeHalfWidth, CrossfadeSpeed + CrossfadeHalfWidth, S);
		const float T2 = Sprint ? FMath::SmoothStep(SprintCrossfadeSpeed - SprintCrossfadeHalfWidth, SprintCrossfadeSpeed + SprintCrossfadeHalfWidth, S) : 0.f;
		WWalk = 1.f - T;
		WJog = T * (1.f - T2);
		WSprint = T * T2;
	}
	float CycleLen = 0.f, Rate = 0.f, CycleW = 0.f;
	if (Walk && WWalk > 0.f) { CycleLen += Walk->GetPlayLength() * WWalk; Rate += (S / WalkReferenceSpeed) * WWalk; CycleW += WWalk; }
	if (Jog && WJog > 0.f) { CycleLen += Jog->GetPlayLength() * WJog; Rate += (S / JogReferenceSpeed) * WJog; CycleW += WJog; }
	if (Sprint && WSprint > 0.f) { CycleLen += Sprint->GetPlayLength() * WSprint; Rate += (S / SprintReferenceSpeed) * WSprint; CycleW += WSprint; }
	if (CycleW > 0.f)
	{
		CycleLen /= CycleW;
		Rate = FMath::Clamp(Rate / CycleW, MinPlayRate, MaxPlayRate);
		LocomotionPhase = FMath::Fmod(LocomotionPhase + DeltaSeconds * Rate / FMath::Max(CycleLen, KINDA_SMALL_NUMBER), 1.f);
	}
	IdleTime += DeltaSeconds;

	// ---- Actions ----
	const float Step = DeltaSeconds / FMath::Max(0.01f, BlendTime);
	if (Current.Index != INDEX_NONE)
	{
		Current.Alpha = FMath::Min(1.f, Current.Alpha + Step);
		Current.Time += DeltaSeconds * Loaded[Current.Index].PlayRate * ActionRateScale;
	}
	if (Fading.Index != INDEX_NONE)
	{
		Fading.Alpha = FMath::Max(0.f, Fading.Alpha - Step);
		Fading.Time += DeltaSeconds * Loaded[Fading.Index].PlayRate;
		if (Fading.Alpha <= 0.f)
		{
			Fading = FActionLayer();
		}
	}
	// Upper-body-only actions take no weight from the ground pose; they are layered from the spine up.
	const bool bCurUpper = Current.Index != INDEX_NONE && Loaded[Current.Index].bUpper;
	const bool bFadeUpper = Fading.Index != INDEX_NONE && Loaded[Fading.Index].bUpper;
	const float ActionW = Current.Index != INDEX_NONE ? Current.Alpha : 0.f;
	const float FadeW = Fading.Index != INDEX_NONE ? Fading.Alpha * (1.f - ActionW) : 0.f;
	const float GroundW = FMath::Max(0.f, 1.f - (bCurUpper ? 0.f : ActionW) - (bFadeUpper ? 0.f : FadeW));
	PendingUpperAlpha = FMath::Min(1.f, (bCurUpper ? ActionW : 0.f) + (bFadeUpper ? FadeW : 0.f));
	// Body sink: weighted planted-ankle offsets of every full-body clip in the mix.
	PendingSink = GroundW * (WWalk * WalkGroundOffset + WJog * JogGroundOffset);
	if (Current.Index != INDEX_NONE && !bCurUpper)
	{
		PendingSink += ActionW * Loaded[Current.Index].GroundOffset;
	}
	if (Fading.Index != INDEX_NONE && !bFadeUpper)
	{
		PendingSink += FadeW * Loaded[Fading.Index].GroundOffset;
	}

	auto AddGround = [this](UAnimSequence* Seq, float W, double Time)
	{
		if (Seq && W > KINDA_SMALL_NUMBER)
		{
			PendingSamples.Add({ Seq, Time, W });
		}
	};
	AddGround(Idle, WIdle * GroundW, PDAnim::Wrap(IdleTime, Idle));
	AddGround(Walk, WWalk * GroundW, Walk ? LocomotionPhase * Walk->GetPlayLength() : 0.0);
	AddGround(Jog, WJog * GroundW, Jog ? LocomotionPhase * Jog->GetPlayLength() : 0.0);
	AddGround(Sprint, WSprint * GroundW, Sprint ? LocomotionPhase * Sprint->GetPlayLength() : 0.0);
	auto AddAction = [this](const FActionLayer& Layer, float W)
	{
		const FLoadedAction& A = Loaded[Layer.Index];
		if (!A.Sequence || W <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const float Life = A.Wobble > 0.f ? A.Wobble * FMath::Sin(2.f * PI * A.WobbleRate * float(Layer.Time) + LifePhase) : 0.f;
		const float BW = A.Blend ? FMath::Clamp(A.BlendWeight + Life, 0.f, 1.f) : 0.f;
		PendingSamples.Add({ A.Sequence, ActionTime(Layer), W * (1.f - BW), A.bUpper });
		if (BW > 0.f)
		{
			PendingSamples.Add({ A.Blend, PDAnim::Wrap(Layer.Time, A.Blend), W * BW, A.bUpper });
		}
	};
	if (Fading.Index != INDEX_NONE)
	{
		AddAction(Fading, FadeW);
	}
	if (Current.Index != INDEX_NONE)
	{
		AddAction(Current, ActionW);
	}
}

FAnimInstanceProxy* UPDAnimInstance::CreateAnimInstanceProxy()
{
	return new FPDAnimProxy(this);
}

void UPDAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void FPDAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UPDAnimInstance* Inst = Cast<UPDAnimInstance>(InAnimInstance))
	{
		Samples = Inst->PendingSamples;
		UpperAlpha = Inst->PendingUpperAlpha;
		UpperBodyRootBone = Inst->UpperBodyRootBone;
		GroundSink = Inst->PendingSink;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float A = Inst->ArmAlphaNow[Side];
			ArmAlpha[Side] = A * A * (3.f - 2.f * A);  // ease in and out
			bArmToMouth[Side] = Inst->bArmMouth[Side];
		}
		GripAlpha = FMath::Max(ArmAlpha[0], ArmAlpha[1]);
		HeadBone = Inst->HeadBoneName;
		MouthOffset = Inst->MouthOffsetLocal;
		const USkeletalMeshComponent* Mesh = Inst->GetSkelMeshComponent();
		const APawn* Pawn = Inst->TryGetPawnOwner();
		if (GripAlpha > 0.f && Mesh && Pawn)
		{
			const FTransform& C = Mesh->GetComponentTransform();
			const FVector Fwd = C.InverseTransformVectorNoScale(Pawn->GetActorForwardVector());
			const FVector Right = C.InverseTransformVectorNoScale(Pawn->GetActorRightVector());
			const FVector Up = C.InverseTransformVectorNoScale(Pawn->GetActorUpVector());
			GripLeanDegrees = Inst->GripLean;
			GripLeanAxis = Right;
			PawnFwd = Fwd; PawnRight = Right; PawnUp = Up;
			const FVector& P = Inst->ElbowPoleLocal;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				GripTarget[Side] = C.InverseTransformPosition(Inst->GripWorld[Side]);
				const float Out = Side == 0 ? -1.f : 1.f;  // left arm's outside is -Right
				ElbowPole[Side] = (Fwd * P.X + Right * (P.Y * Out) + Up * P.Z).GetSafeNormal();
			}
			ArmBones[0][0] = Inst->LeftUpperArmBone; ArmBones[0][1] = Inst->LeftForearmBone; ArmBones[0][2] = Inst->LeftHandBone;
			ArmBones[1][0] = Inst->RightUpperArmBone; ArmBones[1][1] = Inst->RightForearmBone; ArmBones[1][2] = Inst->RightHandBone;
		}
	}
}

namespace PDAnim
{
	/** Blend the samples of one layer (ground or upper body) into Out. Returns false when the layer is empty. */
	bool BlendLayer(const TArray<FPDAnimSample, TInlineAllocator<6>>& Samples, bool bUpper, const FBoneContainer& Bones, FPoseContext& Out)
	{
		const USkeleton* TargetSkeleton = Bones.GetSkeletonAsset();
		auto Valid = [&](const FPDAnimSample& S)
		{
			return S.bUpper == bUpper && S.Sequence && S.Weight > 0.f && (!TargetSkeleton || S.Sequence->GetSkeleton() == TargetSkeleton);
		};
		float Total = 0.f;
		for (const FPDAnimSample& S : Samples)
		{
			if (Valid(S))
			{
				Total += S.Weight;
			}
		}
		if (Total <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		TArray<FCompactPose, TInlineAllocator<6>> Poses;
		TArray<FBlendedCurve, TInlineAllocator<6>> Curves;
		TArray<UE::Anim::FStackAttributeContainer, TInlineAllocator<6>> Attributes;
		TArray<float, TInlineAllocator<6>> Weights;
		for (const FPDAnimSample& S : Samples)
		{
			if (!Valid(S))
			{
				continue;
			}
			FCompactPose& Pose = Poses.AddDefaulted_GetRef();
			Pose.SetBoneContainer(&Bones);
			FBlendedCurve& Curve = Curves.AddDefaulted_GetRef();
			Curve.InitFrom(Bones);
			UE::Anim::FStackAttributeContainer& Attr = Attributes.AddDefaulted_GetRef();
			FAnimationPoseData PoseData(Pose, Curve, Attr);
			S.Sequence->GetAnimationPose(PoseData, FAnimExtractContext(S.Time, false, {}, true));
			Weights.Add(S.Weight / Total);
		}
		if (Poses.Num() == 1)
		{
			Out.Pose.CopyBonesFrom(Poses[0]);
			Out.Curve.CopyFrom(Curves[0]);
			Out.CustomAttributes.CopyFrom(Attributes[0]);
		}
		else
		{
			FAnimationPoseData OutData(Out);
			FAnimationRuntime::BlendPosesTogether(Poses, Curves, Attributes, Weights, OutData);
		}
		return true;
	}
}

bool FPDAnimProxy::Evaluate(FPoseContext& Output)
{
	EvaluateLayers(Output);
	if (GripAlpha > KINDA_SMALL_NUMBER && Output.Pose.GetNumBones() > 0)
	{
		ApplyGrip(Output);
	}
	return true;
}

void FPDAnimProxy::ApplyGrip(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	auto Compact = [&Bones](FName Name)
	{
		const int32 Skel = Bones.GetPoseBoneIndexForBoneName(Name);
		return Skel == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Skel));
	};
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	// Lean the torso in over the prop (component space, about the pawn's right axis).
	const FCompactPoseBoneIndex Spine = Compact(UpperBodyRootBone);
	if (Spine.IsValid() && FMath::Abs(GripLeanDegrees) > KINDA_SMALL_NUMBER)
	{
		FTransform T = CS.GetComponentSpaceTransform(Spine);
		const FQuat Lean(GripLeanAxis.GetSafeNormal(), FMath::DegreesToRadians(GripLeanDegrees * GripAlpha));
		T.SetRotation((Lean * T.GetRotation()).GetNormalized());
		const FBoneTransform One[] = { FBoneTransform(Spine, T) };
		CS.SafeSetCSBoneTransforms(One);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FCompactPoseBoneIndex U = Compact(ArmBones[Side][0]);
		const FCompactPoseBoneIndex F = Compact(ArmBones[Side][1]);
		const FCompactPoseBoneIndex H = Compact(ArmBones[Side][2]);
		if (!U.IsValid() || !F.IsValid() || !H.IsValid())
		{
			continue;
		}
		FTransform UT = CS.GetComponentSpaceTransform(U);
		FTransform FT = CS.GetComponentSpaceTransform(F);
		FTransform HT = CS.GetComponentSpaceTransform(H);
		const FQuat HandInForearm = FT.GetRotation().Inverse() * HT.GetRotation();
		if (ArmAlpha[Side] <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FVector Goal = GripTarget[Side];
		if (bArmToMouth[Side])
		{
			// Two fingers to the lips: a point just in front of and below the head bone, from this frame's pose.
			const FCompactPoseBoneIndex HeadIdx = Compact(HeadBone);
			if (!HeadIdx.IsValid())
			{
				continue;
			}
			const FVector Head = CS.GetComponentSpaceTransform(HeadIdx).GetLocation();
			Goal = Head + PawnFwd * MouthOffset.X + PawnRight * (MouthOffset.Y * (Side == 0 ? -1.f : 1.f)) + PawnUp * MouthOffset.Z;
		}
		const FVector Effector = FMath::Lerp(HT.GetLocation(), Goal, ArmAlpha[Side]);
		const FVector JointTarget = FT.GetLocation() + ElbowPole[Side] * 40.f;
		AnimationCore::SolveTwoBoneIK(UT, FT, HT, JointTarget, Effector, false, 1.0, 1.0);
		HT.SetRotation((FT.GetRotation() * HandInForearm).GetNormalized());  // the wrist follows the forearm
		const FBoneTransform Arm[] = { FBoneTransform(U, UT), FBoneTransform(F, FT), FBoneTransform(H, HT) };
		CS.SafeSetCSBoneTransforms(Arm);
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(CS, Output.Pose);
}

bool FPDAnimProxy::EvaluateLayers(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	if (!PDAnim::BlendLayer(Samples, false, Bones, Output))
	{
		// No ground sample: use the upper layer alone, else the reference pose.
		if (!PDAnim::BlendLayer(Samples, true, Bones, Output))
		{
			Output.ResetToRefPose();
		}
		return true;
	}
	if (Output.Pose.GetNumBones() > 0)
	{
		// In place: drop the root's horizontal travel (the native walk / run carry it) but keep its height, so the
		// hips sink in a crouch and bob in a run instead of being pinned at standing height (feet lifted, knees odd).
		FTransform& Root = Output.Pose[FCompactPoseBoneIndex(0)];
		const FVector Ref = Output.Pose.GetRefPose(FCompactPoseBoneIndex(0)).GetTranslation();
		const FVector T = Root.GetTranslation();
		// Lower the whole body so every clip plants its feet where the idle pose does.
		Root.SetTranslation(FVector(Ref.X, Ref.Y, T.Z - GroundSink));
	}
	if (UpperAlpha <= KINDA_SMALL_NUMBER)
	{
		return true;
	}
	FPoseContext Upper(Output);
	if (!PDAnim::BlendLayer(Samples, true, Bones, Upper))
	{
		return true;
	}
	// Per-bone layer: the upper-body root and every bone under it take the upper pose; hips and legs keep the cycle.
	const int32 RootSkel = Bones.GetPoseBoneIndexForBoneName(UpperBodyRootBone);
	if (RootSkel == INDEX_NONE)
	{
		return true;
	}
	const FCompactPoseBoneIndex RootCompact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RootSkel));
	if (!RootCompact.IsValid())
	{
		return true;
	}
	const float Alpha = FMath::Clamp(UpperAlpha, 0.f, 1.f);
	// The layer root keeps the upper clip's component-space rotation (a torso bent over a prop stays bent over the
	// walking hips; copying it in local space would tip it back and raise the arms). Bones under it copy local space.
	FTransform RootLocal = Upper.Pose[RootCompact];
	const FCompactPoseBoneIndex RootParent = Bones.GetParentBoneIndex(RootCompact);
	if (RootParent.IsValid())
	{
		FCSPose<FCompactPose> UpperCS;
		UpperCS.InitPose(Upper.Pose);
		FCSPose<FCompactPose> BaseCS;
		BaseCS.InitPose(Output.Pose);
		const FTransform Want = UpperCS.GetComponentSpaceTransform(RootCompact);
		const FTransform Parent = BaseCS.GetComponentSpaceTransform(RootParent);
		RootLocal.SetRotation((Parent.GetRotation().Inverse() * Want.GetRotation()).GetNormalized());
		RootLocal.SetTranslation(Output.Pose[RootCompact].GetTranslation());
	}
	for (FCompactPoseBoneIndex B : Output.Pose.ForEachBoneIndex())
	{
		FCompactPoseBoneIndex P = B;
		while (P.IsValid() && P != RootCompact)
		{
			P = Bones.GetParentBoneIndex(P);
		}
		if (P == RootCompact)
		{
			FTransform Blended;
			Blended.Blend(Output.Pose[B], B == RootCompact ? RootLocal : Upper.Pose[B], Alpha);
			Output.Pose[B] = Blended;
		}
	}
	return true;
}
