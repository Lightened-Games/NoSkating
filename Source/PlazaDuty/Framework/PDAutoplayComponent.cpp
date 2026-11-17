// Copyright Plaza Duty. All Rights Reserved.

#include "Framework/PDAutoplayComponent.h"
#include "Interfaces/PDInterfaces.h"
#include "Components/PDGuardComponents.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "NavigationPath.h"
#include "Misc/CommandLine.h"
#include "PlazaDuty.h"

UPDAutoplayComponent::UPDAutoplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPDAutoplayComponent::StartBot(bool bInPassive, int32 Seed)
{
	bActive = true;
	bPassive = bInPassive;
	// The showcase run (-PDShowcase, the recorded video) always demonstrates the prop -> launch-ramp conversion.
	bShowcase = FParse::Param(FCommandLine::Get(), TEXT("PDShowcase"));
	bDemoProps = FParse::Param(FCommandLine::Get(), TEXT("PDDemoProps")) || bShowcase;
	Random.Initialize(Seed);
}

APawn* UPDAutoplayComponent::GetGuardPawn() const
{
	const AController* C = Cast<AController>(GetOwner());
	return C ? C->GetPawn() : nullptr;
}

float UPDAutoplayComponent::BoltDanger(const FVector& Stand) const
{
	// Soonest any skater passes the kneeling spot (large = safe).
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	float Soonest = 999.0f;
	if (!Flow)
	{
		return Soonest;
	}
	for (const AActor* S : Flow->GetSkaters())
	{
		const IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(S);
		if (!Agent)
		{
			continue;
		}
		const float T = Agent->GetSecondsToPoint(Stand, ThreatRadius);
		if (T >= 0.0f)
		{
			Soonest = FMath::Min(Soonest, T);
		}
	}
	return Soonest;
}

AActor* UPDAutoplayComponent::PickBoltLine() const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow || Flow->GetKnobsLeft() <= 0)
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	float BestScore = 0.0f;
	const APawn* Pawn = GetGuardPawn();
	for (AActor* A : Flow->GetSkateLines())
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(A);
		if (!L || !L->IsCappable() || L->IsCapped())
		{
			continue;
		}
		int32 Riders = 0;
		for (const AActor* S : Flow->GetSkaters())
		{
			const IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(S);
			if (Agent && Agent->GetTargetLine() == A)
			{
				++Riders;
			}
		}
		// Expected wear: the line's own trick times how busy it is (lines about to open count a little).
		float Score = L->GetBaseWear() * (0.6f + Riders) * (L->IsOpen() ? 1.0f : 0.5f);
		if (Pawn)
		{
			Score /= 1.0f + FVector::Dist2D(Pawn->GetActorLocation(), L->GetBoltStandPoint()) / FMath::Max(1.0f, bShowcase ? ShowcaseBoltDistanceFalloff : BoltDistanceFalloff);
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = A;
		}
	}
	return Best;
}

AActor* UPDAutoplayComponent::PickPropToClear(float& OutWear) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	OutWear = 0.0f;
	if (!Flow)
	{
		return nullptr;
	}
	AActor* Best = nullptr;
	for (AActor* A : Flow->GetSkateLines())
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(A);
		if (!L)
		{
			continue;
		}
		for (const FPDPropTrick& T : L->GetPropTricks())
		{
			if (T.Prop.IsValid() && T.Wear > OutWear)
			{
				OutWear = T.Wear;
				Best = T.Prop.Get();
			}
		}
	}
	return Best;
}

FVector UPDAutoplayComponent::NearestDeadZone(const FVector& From) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	FVector Best = IdlePoint;
	float BestD = TNumericLimits<float>::Max();
	if (Flow)
	{
		for (const FVector& Z : Flow->GetDeadZones())
		{
			const float D = FVector::Dist(From, Z) + FMath::Abs(From.Z - Z.Z) * 4.0f;
			if (D < BestD)
			{
				BestD = D;
				Best = Z;
			}
		}
	}
	return Best;
}

void UPDAutoplayComponent::Decide()
{
	IPDMatchFlow* Flow = PD::GetFlow(this);
	APawn* Pawn = GetGuardPawn();
	IPDGuardInput* Input = Cast<IPDGuardInput>(GetOwner());
	if (!Flow || !Pawn || !Input)
	{
		return;
	}
	const IPDKnockable* Guard = Cast<IPDKnockable>(Pawn);
	if (Guard && Guard->IsKnockedDown())
	{
		bHoldingBolt = false;
		Goal = EPDBotGoal::Idle;
		return;
	}
	UPDWhistleComponent* Whistle = (bUseWhistle ? Pawn->FindComponentByClass<UPDWhistleComponent>() : nullptr);
	UPDDragComponent* Drag = Pawn->FindComponentByClass<UPDDragComponent>();
	UPDBoltComponent* Bolt = Pawn->FindComponentByClass<UPDBoltComponent>();
	const FVector Here = Pawn->GetActorLocation();

	// Just stood up from a seated knob: step back out of the lane first (no knock-down right after a success).
	if (bWasBolting && !(Bolt && Bolt->IsBolting()))
	{
		bWasBolting = false;
		const IPDSkateLine* Done = Cast<IPDSkateLine>(BoltingLine.Get());
		if (Done && Done->IsCapped() && StepOutSeconds > 0.0f)
		{
			Goal = EPDBotGoal::Retreat;
			RetreatLeft = StepOutSeconds;
			GoalPoint = Here + (Here - Done->GetKnobPoint()).GetSafeNormal2D() * WaitOffset * 1.4f;
			return;
		}
	}
	// Kneeling: hold unless a skater is about to arrive before the knob seats.
	if (Bolt && Bolt->IsBolting())
	{
		bWasBolting = true;
		BoltingLine = Bolt->GetTarget();
		const float Left = (1.0f - Bolt->GetProgress()) * Bolt->BoltHoldSeconds;
		const float Danger = BoltDanger(Pawn->GetActorLocation());
		if (Danger < Left && Danger < AbortSeconds)
		{
			if (Whistle && Whistle->IsReady())
			{
				Input->InputWhistle(true);
			}
			else
			{
				Input->InputInteractReleased(true);
				bHoldingBolt = false;
				Goal = EPDBotGoal::Retreat;
				RetreatLeft = 1.0f;
				const IPDSkateLine* L = Cast<IPDSkateLine>(Bolt->GetTarget());
				const FVector Away = L ? (Here - L->GetKnobPoint()).GetSafeNormal2D() : FVector(-1.0f, 0.0f, 0.0f);
				GoalPoint = Here + Away * WaitOffset * 1.4f;
			}
		}
		return;
	}
	bHoldingBolt = false;
	if (Goal == EPDBotGoal::Retreat && RetreatLeft > 0.0f)
	{
		return;
	}

	// Whistle a crowd away.
	if (Whistle && Whistle->IsReady())
	{
		const int32 Crowd = Whistle->CountInRange();
		const bool bLate = Flow->GetWear() > Flow->GetWearLimit() * 0.6f;
		if (Crowd >= WhistleCrowd || (bLate && Crowd >= WhistleCrowd - 1))
		{
			Input->InputWhistle(true);
		}
	}

	// Dragging: take it to the wall.
	if (Drag && Drag->IsDragging())
	{
		Goal = EPDBotGoal::Clear;
		GoalPoint = DropPoint;
		return;
	}

	if (bDemoProps && !bDemoDone && !bDemoActive && Flow->GetElapsed() > 1.0f)
	{
		AActor* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		// QA: -PDDemoKind=TrashCan (an EPDPropKind name) demos that kind of prop.
		FString Kind;
		FParse::Value(FCommandLine::Get(), TEXT("PDDemoKind="), Kind);
		for (AActor* P : Flow->GetProps())
		{
			const IPDDraggable* Kindable = Cast<IPDDraggable>(P);
			if (!Kind.IsEmpty() && (!Kindable || StaticEnum<EPDPropKind>()->GetNameStringByValue(int64(Kindable->GetPropKind())) != Kind))
			{
				continue;
			}
			const float D = FVector::Dist(P->GetActorLocation(), DemoDropPoint);
			if (D < BestD)
			{
				BestD = D;
				Best = P;
			}
		}
		if (Best)
		{
			bDemoActive = true;
			DemoProp = Best;
			Goal = EPDBotGoal::Clear;
			Target = Best;
			DropPoint = DemoDropPoint;
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] Bot (demo) drags %s to the stair top."), *Best->GetName());
			return;
		}
	}
	if (bDemoActive)
	{
		return;
	}
	if (bShowcase && DemoProp.IsValid() && Flow->GetMatchStats().Friends >= 1)
	{
		// The launch and its friend are on camera: take the ramp away now, before it wears the plaza out.
		float RampWear = 0.0f;
		if (PickPropToClear(RampWear) == DemoProp.Get())
		{
			Goal = EPDBotGoal::Clear;
			Target = DemoProp.Get();
			GoalPoint = Target->GetActorLocation();
			DropPoint = ShowcaseClearPoint;
			return;
		}
	}
	if (bShowcase && Goal == EPDBotGoal::Bolt)
	{
		// Almost there: finish this knob before turning to anything else (no walking back and forth on camera).
		const IPDSkateLine* Cur = Cast<IPDSkateLine>(Target.Get());
		if (Cur && !Cur->IsCapped() && FVector::Dist2D(Here, Cur->GetBoltStandPoint()) < ShowcaseFinishBoltRadius)
		{
			return;
		}
	}

	float ClearWear = 0.0f;
	AActor* Prop = PickPropToClear(ClearWear);
	if (Prop && Prop == DemoProp.Get() && DemoDroppedAt >= 0.0f && Flow->GetElapsed() < DemoDroppedAt + DemoLeaveSeconds)
	{
		Prop = nullptr;  // let the showcase's launch ramp get used first
	}
	bool bShortClear = false;
	if (bShowcase && !bShowcaseClearProps)
	{
		// The video: only the demo prop is cleared, a short carry off the line; no long walks across the plaza.
		bShortClear = Prop && Prop == DemoProp.Get();
		if (!bShortClear)
		{
			Prop = nullptr;
		}
	}
	AActor* Line = PickBoltLine();
	if (Prop && (ClearWear >= ClearFirstWear || !Line))
	{
		Goal = EPDBotGoal::Clear;
		Target = Prop;
		GoalPoint = Prop->GetActorLocation();
		DropPoint = bShortClear ? ShowcaseClearPoint : NearestDeadZone(Prop->GetActorLocation());
		return;
	}
	if (Line)
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(Line);
		Goal = EPDBotGoal::Bolt;
		Target = Line;
		GoalPoint = L->GetBoltStandPoint();
		return;
	}
	Goal = EPDBotGoal::Guard;
	GoalPoint = IdlePoint;
}

bool UPDAutoplayComponent::MoveTo(const FVector& Point, float Dt, float Tolerance)
{
	APawn* Pawn = GetGuardPawn();
	IPDGuardInput* Input = Cast<IPDGuardInput>(GetOwner());
	if (!Pawn || !Input)
	{
		return false;
	}
	const FVector Here = Pawn->GetActorLocation();
	if (FVector::Dist2D(Here, Point) <= Tolerance)
	{
		Input->InputRun(false, true);
		return true;
	}
	// Re-path only when the goal moved enough or the path is old (P46).
	PathAge += Dt;
	if (FVector::Dist2D(Point, PathGoal) > RepathDistance || PathAge > 2.0f || PathIndex >= Path.Num())
	{
		PathGoal = Point;
		PathAge = 0.0f;
		Path.Reset();
		PathIndex = 0;
		if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Here, Point, Pawn))
		{
			if (NavPath->IsValid() && NavPath->PathPoints.Num() > 1)
			{
				Path = NavPath->PathPoints;
				PathIndex = 1;
			}
		}
		if (Path.Num() == 0)
		{
			Path = { Here, Point };
			PathIndex = 1;
		}
	}
	while (PathIndex < Path.Num() - 1 && FVector::Dist2D(Here, Path[PathIndex]) < 60.0f)
	{
		++PathIndex;
	}
	FVector Next = Path.IsValidIndex(PathIndex) ? Path[PathIndex] : Point;
	if (PathIndex >= Path.Num() - 1)
	{
		Next = Point;
	}
	FVector Dir = (Next - Here).GetSafeNormal2D();

	// Stuck recovery: sidestep 110 degrees, alternating.
	StuckTimer += Dt;
	if (StuckTimer >= StuckWindow)
	{
		if (FVector::Dist2D(Here, StuckFrom) < StuckDistance && SidestepLeft <= 0.0f)
		{
			SidestepLeft = 0.45f;
			SidestepSign = -SidestepSign;
			PathAge = 99.0f;
		}
		StuckTimer = 0.0f;
		StuckFrom = Here;
	}
	if (SidestepLeft > 0.0f)
	{
		SidestepLeft -= Dt;
		Dir = FRotator(0.0f, 110.0f * SidestepSign, 0.0f).RotateVector(Dir);
	}
	const float Dist = FVector::Dist2D(Here, Point);
	Input->InputRun(Dist > RunDistance, true);
	Input->InputMove(Dir, Dist < 120.0f ? FMath::Clamp(Dist / 120.0f, 0.35f, 1.0f) : 1.0f, true);
	return false;
}

void UPDAutoplayComponent::Execute(float Dt)
{
	APawn* Pawn = GetGuardPawn();
	IPDGuardInput* Input = Cast<IPDGuardInput>(GetOwner());
	if (!Pawn || !Input)
	{
		return;
	}
	const IPDKnockable* Guard = Cast<IPDKnockable>(Pawn);
	if (Guard && (Guard->IsKnockedDown() || Guard->GetGuardAction() == EPDGuardAction::Bolting))
	{
		return;
	}
	switch (Goal)
	{
	case EPDBotGoal::Retreat:
		RetreatLeft -= Dt;
		MoveTo(GoalPoint, Dt, ArriveRadius);
		break;
	case EPDBotGoal::Clear:
	{
		UPDDragComponent* Drag = Pawn->FindComponentByClass<UPDDragComponent>();
		if (Drag && Drag->IsDragging())
		{
			if (MoveTo(DropPoint, Dt, 70.0f))
			{
				Input->InputInteractPressed(true); // drop it
				Goal = EPDBotGoal::Idle;
				if (bDemoActive)
				{
					bDemoActive = false;
					bDemoDone = true;
					if (const IPDMatchFlow* F = PD::GetFlow(this))
					{
						DemoDroppedAt = F->GetElapsed();
					}
				}
			}
			break;
		}
		AActor* Prop = Target.Get();
		if (!Prop)
		{
			Goal = EPDBotGoal::Idle;
			break;
		}
		const IPDDraggable* D = Cast<IPDDraggable>(Prop);
		const float Reach = (D ? D->GetFootprintRadius() : 40.0f) + 70.0f;
		// A prop in a busy lane: whistle the runners off, or wait beside it, before stepping in to grab it.
		const float PropDanger = BoltDanger(Prop->GetActorLocation());
		if (PropDanger < GrabSafetySeconds && FVector::Dist2D(Pawn->GetActorLocation(), Prop->GetActorLocation()) < Reach + 250.0f)
		{
			UPDWhistleComponent* Whistle = (bUseWhistle ? Pawn->FindComponentByClass<UPDWhistleComponent>() : nullptr);
			if (Whistle && Whistle->IsReady() && Whistle->CountInRange() > 0)
			{
				Input->InputWhistle(true);
			}
			const FVector Away = (Pawn->GetActorLocation() - Prop->GetActorLocation()).GetSafeNormal2D();
			MoveTo(Prop->GetActorLocation() + Away * (Reach + 150.0f), Dt, ArriveRadius);
			break;
		}
		if (MoveTo(Prop->GetActorLocation(), Dt, Reach))
		{
			const UPDInteractComponent* Interact = Pawn->FindComponentByClass<UPDInteractComponent>();
			if (Interact && Interact->GetFocus() == Prop)
			{
				Input->InputInteractPressed(true);
			}
			else
			{
				// Face it so the focus picks it.
				Input->InputMove((Prop->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D(), 0.3f, true);
			}
		}
		break;
	}
	case EPDBotGoal::Bolt:
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(Target.Get());
		if (!L || L->IsCapped())
		{
			Goal = EPDBotGoal::Idle;
			break;
		}
		const FVector Stand = L->GetBoltStandPoint();
		const float Danger = BoltDanger(Stand);
		const UPDBoltComponent* Bolt = Pawn->FindComponentByClass<UPDBoltComponent>();
		const float Need = (Bolt ? Bolt->BoltHoldSeconds + Bolt->StepInSeconds : 3.3f) + BoltSafetyMargin;
		const float Here2Stand = FVector::Dist2D(Pawn->GetActorLocation(), Stand);
		if (Here2Stand > 260.0f)
		{
			MoveTo(Stand, Dt, ArriveRadius);
			break;
		}
		if (Danger < Need || !L->IsOpen())
		{
			// Wait beside the lane; whistle the run clear if the crowd is close.
			UPDWhistleComponent* Whistle = (bUseWhistle ? Pawn->FindComponentByClass<UPDWhistleComponent>() : nullptr);
			const bool bClearForBolt = bShowcase && bShowcaseWhistleToBolt && Here2Stand < 160.0f;
			if (Whistle && Whistle->IsReady() && Whistle->CountInRange() > 0 && Danger < Need && (Danger > 0.6f || bClearForBolt))
			{
				Input->InputWhistle(true);
			}
			const FVector Away = (Stand - L->GetKnobPoint()).GetSafeNormal2D();
			MoveTo(Stand + Away * WaitOffset, Dt, ArriveRadius);
			break;
		}
		if (MoveTo(Stand, Dt, ArriveRadius))
		{
			const UPDInteractComponent* Interact = Pawn->FindComponentByClass<UPDInteractComponent>();
			if (Interact && Interact->GetFocus() == Target.Get())
			{
				Input->InputInteractPressed(true);
				bHoldingBolt = true;
			}
		}
		break;
	}
	case EPDBotGoal::Guard:
	case EPDBotGoal::Idle:
	default:
		MoveTo(GoalPoint.IsNearlyZero() ? IdlePoint : GoalPoint, Dt, 120.0f);
		break;
	}
}

void UPDAutoplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const UFactoryFlowSubsystem* Kit = UFactoryFlowSubsystem::Get(this);
	if (Kit && !Kit->IsBotDriving())
	{
		return;  // the player took over; the bot resumes from the current state after the idle timeout
	}
	if (!bActive || bPassive || !Flow || Flow->GetMatchState() != EPDMatchState::Active)
	{
		return;
	}
	DecisionTimer -= DeltaTime;
	if (DecisionTimer <= 0.0f)
	{
		DecisionTimer = DecisionInterval;
		Decide();
	}
	Execute(DeltaTime);
}
