// Copyright Plaza Duty. All Rights Reserved.

#include "Components/PDSkaterBrainComponent.h"
#include "Actors/PDSkaterPawn.h"
#include "Interfaces/PDInterfaces.h"
#include "Player/PDAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Engine/BlockingVolume.h"
#include "PlazaDuty.h"

namespace PDSkate
{
	constexpr int32 FeatureKey = 1;
	constexpr float TangentProbe = 40.0f;
	constexpr float PopPoseSeconds = 0.2f;
	constexpr float LandPoseSeconds = 0.35f;
	constexpr float TraceHeight = 12.0f;
	constexpr float SearchStep = 40.0f;

	int32 PropKey(const AActor* Prop) { return 1000 + static_cast<int32>(GetTypeHash(Prop) % 100000); }
}

UPDSkaterBrainComponent::UPDSkaterBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PopSound = UPDAudioLibrary::OlliePop();
	GrindSound = UPDAudioLibrary::GrindScrape();
	RailSound = UPDAudioLibrary::RailScrape();
}

void UPDSkaterBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const APDSkaterPawn* Pawn = Cast<APDSkaterPawn>(GetOwner()))
	{
		Anim = Pawn->Rider ? Cast<UPDAnimInstance>(Pawn->Rider->GetAnimInstance()) : nullptr;
	}
	GroundPos = GetOwner()->GetActorLocation();
	LastPos = GroundPos;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PD.Ambient")))
		{
			Walkers.Add(*It);
		}
	}
}

void UPDSkaterBrainComponent::CacheWorldPoints()
{
	if (bCached)
	{
		return;
	}
	bCached = true;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* A = *It;
		if (A->ActorHasTag(PD::HubTag))
		{
			Hubs.Add(A->GetActorLocation());
		}
		if (A->ActorHasTag(PD::SkaterEntryTag))
		{
			Entries.Add(A->GetActorLocation());
		}
		if (A->ActorHasTag(PD::RampTag))
		{
			bHasRamp = true;
			if (A->ActorHasTag(TEXT("Top")))
			{
				RampTop = A->GetActorLocation();
			}
			else
			{
				RampBottom = A->GetActorLocation();
			}
		}
	}
}

float UPDSkaterBrainComponent::PhaseSpeedScale() const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	return Flow ? Flow->GetPhase().SpeedScale : 1.0f;
}

void UPDSkaterBrainComponent::StartSession(int32 Seed, bool bFriend)
{
	Random.Initialize(Seed);
	SpeedScale = 1.0f + Random.FRandRange(-SpeedJitter, SpeedJitter);
	CacheWorldPoints();
	GroundPos = GetOwner()->GetActorLocation();
	LastPos = GroundPos;
	LastYaw = GetOwner()->GetActorRotation().Yaw;
	SetPose(TEXT("Stance"));
	if (AActor* First = ChooseLine(nullptr))
	{
		BeginTransferTo(First);
	}
	else
	{
		Mode = EPDSkaterMode::Idle;
		Rest = 1.0f;
	}
}

bool UPDSkaterBrainComponent::IsCommitted() const
{
	const IPDSkateLine* L = Cast<IPDSkateLine>(Line.Get());
	return Mode == EPDSkaterMode::Ride && L && Rest <= 0.0f && S >= L->GetFeatureStartDistance() - CommitDistance && S <= L->GetRunEndDistance();
}

void UPDSkaterBrainComponent::PreferLine(AActor* NewLine)
{
	const IPDSkateLine* L = Cast<IPDSkateLine>(NewLine);
	if (L && L->IsOpen() && !IsCommitted() && NewLine != Line.Get() && (Mode == EPDSkaterMode::Ride || Mode == EPDSkaterMode::Idle))
	{
		BeginTransferTo(NewLine);
	}
}

AActor* UPDSkaterBrainComponent::ChooseLine(AActor* Exclude) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return nullptr;
	}
	TArray<TPair<AActor*, float>> Options;
	float Total = 0.0f;
	const FVector Here = GetOwner()->GetActorLocation();
	for (AActor* A : Flow->GetSkateLines())
	{
		const IPDSkateLine* L = Cast<IPDSkateLine>(A);
		if (!L || A == Exclude || !L->IsOpen())
		{
			continue;
		}
		float W = L->GetAppeal();
		if (W < MinAppeal)
		{
			continue;
		}
		// Crowding: a line only takes so many riders (more once the skaters invent prop tricks on it).
		int32 Riders = 0;
		for (const AActor* Other : Flow->GetSkaters())
		{
			const IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(Other);
			if (Other != GetOwner() && Agent && Agent->GetTargetLine() == A)
			{
				++Riders;
			}
		}
		if (A != Line.Get() && Riders >= LineCapacity + L->GetPropTricks().Num())
		{
			continue;
		}
		if (A == Line.Get())
		{
			W += StayBonus;
		}
		// Nearby lines are a little more tempting.
		W /= 1.0f + FVector::Dist2D(L->GetPointAtDistance(L->GetFeatureStartDistance()), Here) / 5000.0f;
		Options.Add({ A, W });
		Total += W;
	}
	if (Options.Num() == 0 || Total <= 0.0f)
	{
		return nullptr;
	}
	float Pick = Random.FRandRange(0.0f, Total);
	for (const TPair<AActor*, float>& O : Options)
	{
		Pick -= O.Value;
		if (Pick <= 0.0f)
		{
			return O.Key;
		}
	}
	return Options.Last().Key;
}

bool UPDSkaterBrainComponent::IsClear(const FVector& A, const FVector& B) const
{
	FVector Unused;
	return !FindBlock(A, B, Unused);
}

bool UPDSkaterBrainComponent::FindBlock(const FVector& A, const FVector& B, FVector& OutPoint) const
{
	// A body-wide sweep, not a thin line (user 2026-11-13: "still so many collisions between the npcs and objects"): the
	// line slipped past the island ring, plants, bollards and posts that the runner's body then clipped. The bottom
	// stays at TraceHeight so kerbs remain crossable; overlaps at the ends (a route starting on a ledge) are ignored.
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PDSkaterRoute), false, GetOwner());
	const float HalfHeight = 70.0f;
	const FVector Up(0.0f, 0.0f, PDSkate::TraceHeight + 2.0f + HalfHeight);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, A + Up, B + Up, FQuat::Identity, Objects, FCollisionShape::MakeCapsule(RouteClearance, HalfHeight), Params);
	const float Len = FVector::Dist2D(A, B);
	for (const FHitResult& H : Hits)
	{
		if (H.bBlockingHit && !H.bStartPenetrating && H.Distance < Len - 40.0f && !(H.GetActor() && H.GetActor()->IsA<ABlockingVolume>()))
		{
			OutPoint = FVector(H.Location.X, H.Location.Y, A.Z);
			return true;
		}
	}
	return false;
}

bool UPDSkaterBrainComponent::AppendDetour(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	// Step round whatever blocks the straight leg: try points beside the obstacle, nearest first, either side.
	FVector Block;
	if (!FindBlock(From, To, Block))
	{
		Out.Add(To);
		return true;
	}
	const FVector Dir = (To - From).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
	for (const float Offset : { 220.0f, 340.0f, 480.0f, 650.0f, 850.0f })
	{
		for (const float Sign : { 1.0f, -1.0f })
		{
			const FVector D = Block + Side * Sign * Offset - Dir * 60.0f;
			if (IsClear(From, D) && IsClear(D, To))
			{
				Out.Add(D);
				Out.Add(To);
				return true;
			}
		}
	}
	return false;
}

void UPDSkaterBrainComponent::AppendLegs(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	if (IsClear(From, To))
	{
		Out.Add(To);
		return;
	}
	const bool bDeck = IsDeck(From);
	float Best = TNumericLimits<float>::Max();
	int32 BestHub = INDEX_NONE;
	for (int32 i = 0; i < Hubs.Num(); ++i)
	{
		const FVector& H = Hubs[i];
		if (IsDeck(H) != bDeck)
		{
			continue;
		}
		const float Len = FVector::Dist(From, H) + FVector::Dist(H, To);
		if (Len < Best && IsClear(From, H) && IsClear(H, To))
		{
			Best = Len;
			BestHub = i;
		}
	}
	if (BestHub != INDEX_NONE)
	{
		Out.Add(Hubs[BestHub]);
		Out.Add(To);
		return;
	}
	// Two hubs.
	for (int32 i = 0; i < Hubs.Num(); ++i)
	{
		for (int32 j = 0; j < Hubs.Num(); ++j)
		{
			if (i == j || IsDeck(Hubs[i]) != bDeck || IsDeck(Hubs[j]) != bDeck)
			{
				continue;
			}
			const float Len = FVector::Dist(From, Hubs[i]) + FVector::Dist(Hubs[i], Hubs[j]) + FVector::Dist(Hubs[j], To);
			if (Len < Best && IsClear(From, Hubs[i]) && IsClear(Hubs[i], Hubs[j]) && IsClear(Hubs[j], To))
			{
				Best = Len;
				BestHub = i * 1000 + j;
			}
		}
	}
	if (BestHub != INDEX_NONE)
	{
		Out.Add(Hubs[BestHub / 1000]);
		Out.Add(Hubs[BestHub % 1000]);
		Out.Add(To);
		return;
	}
	// No clear hub path: detour round the obstacle (or, from the nearest clear hub, detour the rest of the way).
	if (AppendDetour(From, To, Out))
	{
		return;
	}
	for (int32 i = 0; i < Hubs.Num(); ++i)
	{
		if (IsDeck(Hubs[i]) == bDeck && IsClear(From, Hubs[i]))
		{
			TArray<FVector> Tail;
			if (AppendDetour(Hubs[i], To, Tail))
			{
				Out.Add(Hubs[i]);
				Out.Append(Tail);
				return;
			}
		}
	}
	Out.Add(To);
}

void UPDSkaterBrainComponent::BuildRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
	Out.Reset();
	Out.Add(From);
	if (bHasRamp && IsDeck(From) != IsDeck(To))
	{
		if (!IsDeck(From))
		{
			AppendLegs(From, RampBottom, Out);
			Out.Add(RampTop);
			AppendLegs(RampTop, To, Out);
		}
		else
		{
			AppendLegs(From, RampTop, Out);
			Out.Add(RampBottom);
			AppendLegs(RampBottom, To, Out);
		}
		return;
	}
	AppendLegs(From, To, Out);
}

void UPDSkaterBrainComponent::BeginTransferTo(AActor* NewLine)
{
	const IPDSkateLine* L = Cast<IPDSkateLine>(NewLine);
	if (!L)
	{
		return;
	}
	Line = NewLine;
	// Join the new loop on its ride-back section (or at its start), never in the middle of a run.
	float Join = L->GetDistanceClosestTo(GroundPos);
	if (Join < L->GetRunEndDistance() || IsDeck(L->GetPointAtDistance(Join)) != IsDeck(GroundPos))
	{
		Join = 0.0f;
	}
	S = Join;
	BuildRoute(GroundPos, L->GetPointAtDistance(Join), Route);
	RouteIndex = 0;
	Mode = EPDSkaterMode::Transfer;
	StateTime = 0.0f;
	ActiveTrick = INDEX_NONE;
	SetPose(TEXT("Stance"));
}

void UPDSkaterBrainComponent::BeginRide(float StartDistance)
{
	S = StartDistance;
	Mode = EPDSkaterMode::Ride;
	Started.Reset();
	Landed.Reset();
	ActiveTrick = INDEX_NONE;
	bLoopDecided = false;
	Rest = StartDistance <= 1.0f ? Random.FRandRange(SkaterRestSeconds.X, SkaterRestSeconds.Y) : 0.0f;
}

void UPDSkaterBrainComponent::Scatter(const FVector& From, float Seconds)
{
	if (Mode == EPDSkaterMode::Home || Mode == EPDSkaterMode::Gone)
	{
		return;
	}
	Mode = EPDSkaterMode::Scatter;
	ScatterLeft = Seconds;
	ScatterDir = (GroundPos - From).GetSafeNormal2D();
	if (ScatterDir.IsNearlyZero())
	{
		ScatterDir = FVector(1.0f, 0.0f, 0.0f);
	}
	ActiveTrick = INDEX_NONE;
	SetPose(TEXT("Pop"), true);
	PoseTimer = PDSkate::PopPoseSeconds;
}

void UPDSkaterBrainComponent::GoHome()
{
	if (Mode == EPDSkaterMode::Home || Mode == EPDSkaterMode::Gone)
	{
		return;
	}
	CacheWorldPoints();
	FVector Exit = GroundPos + FVector(-4000.0f, 0.0f, 0.0f);
	float Best = TNumericLimits<float>::Max();
	for (const FVector& E : Entries)
	{
		const float D = FVector::Dist(E, GroundPos);
		if (D < Best)
		{
			Best = D;
			Exit = E;
		}
	}
	BuildRoute(GroundPos, Exit, Route);
	RouteIndex = 0;
	Mode = EPDSkaterMode::Home;
	ActiveTrick = INDEX_NONE;
	SetPose(TEXT("Stance"));
}

void UPDSkaterBrainComponent::Abandon(bool bBail)
{
	ActiveTrick = INDEX_NONE;
	if (bBail)
	{
		Mode = EPDSkaterMode::Bail;
		StateTime = 0.0f;
		SetPose(TEXT("Bail"), true);
		return;
	}
	if (AActor* Next = ChooseLine(Line.Get()))
	{
		BeginTransferTo(Next);
	}
	else
	{
		Mode = EPDSkaterMode::Idle;
		Rest = Random.FRandRange(WaitSeconds.X, WaitSeconds.Y);
		Line = nullptr;
		SetPose(TEXT("Stance"));
	}
}

void UPDSkaterBrainComponent::BuildTricks(TArray<FTrick>& Out) const
{
	Out.Reset();
	const IPDSkateLine* L = Cast<IPDSkateLine>(Line.Get());
	if (!L)
	{
		return;
	}
	const float FS = L->GetFeatureStartDistance();
	const float FE = L->GetFeatureEndDistance();
	if (!L->IsCapped())
	{
		FTrick& T = Out.AddDefaulted_GetRef();
		T.Key = PDSkate::FeatureKey;
		T.Name = L->GetTrickName();
		T.Wear = L->GetBaseWear();
		if (L->GetTrickStyle() == EPDTrickStyle::Grind)
		{
			T.bGrind = true;
			T.StartD = FS - GrindPopLength;
			T.GrindStart = FS;
			T.GrindEnd = FE;
			T.EndD = FE + GrindExitLength;
			T.Height = L->GetFeatureHeight();
		}
		else
		{
			T.StartD = FS;
			T.EndD = FE;
			T.Height = GapArcHeight;
		}
	}
	for (const FPDPropTrick& P : L->GetPropTricks())
	{
		if (!P.Prop.IsValid())
		{
			continue;
		}
		FTrick& T = Out.AddDefaulted_GetRef();
		T.Key = PDSkate::PropKey(P.Prop.Get());
		T.Name = P.TrickName;
		T.Wear = P.Wear;
		T.Prop = P.Prop;
		T.bOnProp = true;
		const float C = P.PathDistance;
		if (P.RideLength > 0.0f)
		{
			// Manual along the top of the bench.
			T.bGrind = true;
			T.bRide = true;
			T.GrindStart = C - P.RideLength * 0.5f;
			T.GrindEnd = C + P.RideLength * 0.5f;
			T.StartD = T.GrindStart - PropArcLength * 0.6f;
			T.EndD = T.GrindEnd + PropArcLength * 0.6f;
			T.Height = P.ClearHeight;
		}
		else
		{
			T.StartD = C - PropArcLength;
			T.EndD = C + PropArcLength * (P.bLaunch ? 2.4f : 1.0f);
			T.Height = (P.ClearHeight + PropClearMargin) * (P.bLaunch ? LaunchHeightScale : 1.0f);
		}
	}
}

float UPDSkaterBrainComponent::TrickHeight(const FTrick& T, float D) const
{
	if (D < T.StartD || D > T.EndD)
	{
		return 0.0f;
	}
	if (T.bGrind)
	{
		if (D < T.GrindStart)
		{
			const float V = (D - T.StartD) / FMath::Max(1.0f, T.GrindStart - T.StartD);
			return T.Height * V + GrindPopExtra * FMath::Sin(PI * V);
		}
		if (D <= T.GrindEnd)
		{
			return T.Height;
		}
		const float V = (D - T.GrindEnd) / FMath::Max(1.0f, T.EndD - T.GrindEnd);
		return T.Height * (1.0f - V) * (1.0f - V) + GrindPopExtra * 0.6f * FMath::Sin(PI * V);
	}
	const float U = (D - T.StartD) / FMath::Max(1.0f, T.EndD - T.StartD);
	return T.Height * FMath::Sin(PI * U);
}

void UPDSkaterBrainComponent::SetPose(FName Action, bool bRestart)
{
	if (!Anim)
	{
		if (const APDSkaterPawn* Pawn = Cast<APDSkaterPawn>(GetOwner()))
		{
			Anim = Pawn->Rider ? Cast<UPDAnimInstance>(Pawn->Rider->GetAnimInstance()) : nullptr;
		}
	}
	if (Anim && (Action != CurrentPose || bRestart))
	{
		const APDSkaterPawn* Pawn = Cast<APDSkaterPawn>(GetOwner());
		if (Pawn && !Pawn->bRideBoard && OnFootLocomotionPoses.Contains(Action))
		{
			Anim->ClearAction(0.15f);  // runs on the speed-matched locomotion clips
		}
		else
		{
			Anim->SetAction(Action, bRestart, 0.12f);
		}
		CurrentPose = Action;
	}
}

float UPDSkaterBrainComponent::ComputeSideStep(const FVector& Pos, const FVector& Fwd, bool bCommitted) const
{
	if (bCommitted)
	{
		return 0.0f;
	}
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return 0.0f;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	float Target = 0.0f;
	auto Consider = [&](const FVector& P, float Radius)
	{
		const FVector To = P - Pos;
		if (FMath::Abs(To.Z) > 150.0f)
		{
			return;
		}
		const float Ahead = FVector::DotProduct(To, Fwd);
		const float Side = FVector::DotProduct(To, Right) - SideStep;
		const float Reach = AvoidRadius + Radius;
		if (Ahead > -40.0f && Ahead < AvoidLookAhead && FMath::Abs(Side) < Reach)
		{
			const float Sign = Side >= 0.0f ? -1.0f : 1.0f;
			Target += Sign * (Reach - FMath::Abs(Side));
		}
	};
	if (const AActor* Guard = Flow->GetGuard())
	{
		Consider(Guard->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f), 30.0f);
	}
	for (const AActor* Other : Flow->GetSkaters())
	{
		if (Other && Other != GetOwner())
		{
			Consider(Other->GetActorLocation(), 0.0f);
		}
	}
	// Walkers and fixed dressing (benches, bins, lamps, trees) too, so a runner never passes through them.
	for (const TWeakObjectPtr<AActor>& W : Walkers)
	{
		if (W.IsValid())
		{
			Consider(W->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f), 0.0f);
		}
	}
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("PDSkaterAvoid"), false, GetOwner());
		// Bottom 60 cm up: ledges, curbs and bench seats stay trick features; posts, bins, trunks and backrests count.
		const FVector From = Pos + Right * SideStep + FVector(0.0f, 0.0f, 115.0f);
		if (World->SweepSingleByObjectType(Hit, From, From + Fwd * AvoidLookAhead, FQuat::Identity,
				FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeCapsule(40.0f, 55.0f), Q) && Hit.GetActor() && !Hit.bStartPenetrating)
		{
			Consider(FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Pos.Z), 10.0f);
		}
	}
	for (const AActor* Prop : Flow->GetProps())
	{
		const IPDDraggable* D = Cast<IPDDraggable>(Prop);
		if (D && !D->IsConverted())
		{
			Consider(Prop->GetActorLocation(), D->GetFootprintRadius() - 40.0f);
		}
	}
	return FMath::Clamp(SideStep + Target, -MaxSideStep, MaxSideStep);
}

void UPDSkaterBrainComponent::CheckKnockdown(const FVector& Pos, bool bCommitted)
{
	if (!bCommitted)
	{
		return;
	}
	IPDMatchFlow* Flow = PD::GetFlow(this);
	AActor* Guard = Flow ? Flow->GetGuard() : nullptr;
	IPDKnockable* Knock = Cast<IPDKnockable>(Guard);
	if (!Knock || Knock->IsKnockedDown())
	{
		return;
	}
	const FVector G = Guard->GetActorLocation();
	const FVector To = G - Pos;
	if (To.Size2D() < KnockdownRadius && To.Z > -60.0f && To.Z < 260.0f && FVector::DotProduct(To.GetSafeNormal2D(), Velocity.GetSafeNormal2D()) > -0.2f)
	{
		if (Knock->Knockdown(GetOwner()))
		{
			Abandon(true);
		}
	}
}

void UPDSkaterBrainComponent::ApplyTransform(const FVector& InGround, float Yaw, float Height, float Dt, float BoardPitch)
{
	AActor* Owner = GetOwner();
	const FVector NewPos = InGround + FVector(0.0f, 0.0f, Height);
	const float YawRate = Dt > 0.0f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Dt : 0.0f;
	const float SmoothYaw = FMath::FixedTurn(LastYaw, Yaw, 540.0f * Dt);
	LastYaw = SmoothYaw;
	Lean = FMath::FInterpTo(Lean, FMath::Clamp(YawRate * LeanPerYawRate, -MaxLean, MaxLean), Dt, 6.0f);
	Owner->SetActorLocationAndRotation(NewPos, FRotator(0.0f, SmoothYaw, 0.0f));
	if (const APDSkaterPawn* Pawn = Cast<APDSkaterPawn>(Owner))
	{
		if (Pawn->Pivot)
		{
			// A runner leans a little into turns; no board pitch on ramps and ollies.
			Pawn->Pivot->SetRelativeRotation(Pawn->bRideBoard ? FRotator(BoardPitch, 0.0f, Lean) : FRotator(0.0f, 0.0f, Lean * 0.5f));
		}
	}
	Velocity = Dt > 0.0f ? (NewPos - LastPos) / Dt : FVector::ZeroVector;
	LastPos = NewPos;
	if (APDSkaterPawn* Pawn = Cast<APDSkaterPawn>(Owner))
	{
		Pawn->CurrentVelocity = Velocity;
	}
}

float UPDSkaterBrainComponent::MoveAlong(TArray<FVector>& InRoute, int32& Index, float Distance, FVector& InOutPos) const
{
	while (Distance > 0.0f && Index + 1 < InRoute.Num())
	{
		const FVector Next = InRoute[Index + 1];
		const float Gap = FVector::Dist(InOutPos, Next);
		if (Gap <= Distance)
		{
			InOutPos = Next;
			Distance -= Gap;
			++Index;
		}
		else
		{
			InOutPos += (Next - InOutPos) / Gap * Distance;
			Distance = 0.0f;
		}
	}
	return Distance;
}

FVector UPDSkaterBrainComponent::SolidStep(const FVector& From, const FVector& To, float Dt)
{
	// Free running (not on a trick line) is solid: sweep the body and slide along whatever it meets, like a walking
	// character, so a runner never passes through a wall, the island ring, a planter, a bollard or another person.
	// Walkable slopes (the bank, the stair ramp) and anything below knee height (kerbs, coping) are not obstacles.
	const FVector Delta = To - From;
	if (Delta.SizeSquared2D() < 0.01f)
	{
		return To;
	}
	const float HalfHeight = 60.0f;
	const FVector Lift(0.0f, 0.0f, 16.0f + HalfHeight);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PDSkaterSolid), false, GetOwner());
	const FCollisionShape Body = FCollisionShape::MakeCapsule(BodyRadius, HalfHeight);
	auto Blocked = [&](const FVector& A, const FVector& B, FHitResult& Out) -> bool
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, A + Lift, B + Lift, FQuat::Identity, Objects, Body, Params);
		for (const FHitResult& H : Hits)
		{
			if (!H.bBlockingHit || H.bStartPenetrating || H.ImpactNormal.Z > 0.5f || H.ImpactPoint.Z < A.Z + 30.0f)
			{
				continue;
			}
			const UPrimitiveComponent* C = H.GetComponent();
			// Invisible play-area bounds are for the guard, not for runners arriving from the street.
			if ((C && C->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) || (H.GetActor() && H.GetActor()->IsA<ABlockingVolume>()))
			{
				continue;
			}
			Out = H;
			return true;
		}
		return false;
	};
	FHitResult Hit;
	if (!Blocked(From, To, Hit))
	{
		StuckFor = 0.0f;
		return To;
	}
	// Slide along the obstacle.
	const FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
	const FVector Slide = FVector(Delta.X, Delta.Y, 0.0f) - FVector::DotProduct(FVector(Delta.X, Delta.Y, 0.0f), N) * N;
	FHitResult Hit2;
	if (!Slide.IsNearlyZero() && !Blocked(From, From + Slide, Hit2))
	{
		StuckFor = 0.0f;
		return From + Slide + FVector(0.0f, 0.0f, Delta.Z);
	}
	// Head-on into something round (a trunk, a hydrant): turn up to 90 degrees either way and walk round it.
	const FVector Flat(Delta.X, Delta.Y, 0.0f);
	for (const float Angle : { 35.0f, -35.0f, 60.0f, -60.0f, 90.0f, -90.0f })
	{
		const FVector Turned = Flat.RotateAngleAxis(Angle, FVector::UpVector);
		FHitResult Hit3;
		if (!Blocked(From, From + Turned, Hit3))
		{
			StuckFor = FMath::Max(0.0f, StuckFor - Dt);
			return From + Turned + FVector(0.0f, 0.0f, Delta.Z);
		}
	}
	// Pinned: hold still; the caller replans after StuckGiveUpSeconds (never push through).
	StuckFor += Dt;
	return From;
}

void UPDSkaterBrainComponent::TickTransfer(float Dt, bool bHome)
{
	const FVector StartGround = GroundPos;
	const float Speed = SkaterCruiseSpeed * SpeedScale * PhaseSpeedScale() * FMath::Clamp(StateTime / 0.6f, 0.35f, 1.0f);
	FVector Pos = GroundPos;
	// Undo the previous side step before moving along the route, then re-apply it.
	const FVector PrevFwd = FRotator(0.0f, LastYaw, 0.0f).Vector();
	const FVector PrevRight = FVector::CrossProduct(FVector::UpVector, PrevFwd);
	Pos -= PrevRight * SideStep;
	const FVector Before = Pos;
	MoveAlong(Route, RouteIndex, Speed * Dt, Pos);
	FVector Dir = (Pos - Before).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = PrevFwd;
	}
	SideStep = FMath::FInterpConstantTo(SideStep, ComputeSideStep(Pos, Dir, false), Dt, SideStepRate);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	GroundPos = SolidStep(StartGround, Pos + Right * SideStep, Dt);
	if (StuckFor > StuckGiveUpSeconds && Route.Num() > 0)
	{
		// Pinned against something: plan a fresh route from here instead of pushing through it.
		const FVector Goal = Route.Last();
		BuildRoute(GroundPos, Goal, Route);
		RouteIndex = 0;
		SideStep = 0.0f;
		StuckFor = 0.0f;
	}
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2((Pos - Before).Z, FMath::Max(1.0f, (Pos - Before).Size2D())));
	ApplyTransform(GroundPos, Dir.Rotation().Yaw, 0.0f, Dt, Pitch);
	if (RouteIndex + 1 >= Route.Num())
	{
		SideStep = 0.0f;  // keep the solid position (no snap back onto the route line)
		if (bHome)
		{
			Mode = EPDSkaterMode::Gone;
			GetOwner()->SetActorHiddenInGame(true);
			GetOwner()->SetActorEnableCollision(false);
			return;
		}
		BeginRide(S);
	}
}

void UPDSkaterBrainComponent::TickScatter(float Dt)
{
	ScatterLeft -= Dt;
	FVector Pos = GroundPos;
	FVector Next = Pos + ScatterDir * ScatterSpeed * Dt * FMath::Clamp(ScatterLeft, 0.3f, 1.0f);
	// Stay inside the plaza; turn along walls.
	if (FMath::Abs(Next.X - PlazaBox.X) > PlazaBox.Z || FMath::Abs(Next.Y - PlazaBox.Y) > PlazaBox.W || !IsClear(Pos, Next + ScatterDir * 60.0f))
	{
		ScatterDir = FRotator(0.0f, Random.FRandRange(100.0f, 160.0f) * (Random.FRand() > 0.5f ? 1.0f : -1.0f), 0.0f).RotateVector(ScatterDir);
		Next = Pos;
	}
	// Follow the floor (deck edges drop to the lower plaza).
	FHitResult Hit;
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PDSkaterFloor), false, GetOwner());
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Next + FVector(0.0f, 0.0f, 60.0f), Next - FVector(0.0f, 0.0f, 400.0f), Objects, Params))
	{
		if (FMath::Abs(Hit.ImpactPoint.Z - Pos.Z) > 40.0f)
		{
			// A deck edge (1.5 m drop): turn back rather than sink down the wall (2026-11-14 collision pass).
			ScatterDir = -ScatterDir;
			Next = Pos;
		}
		else
		{
			Next.Z = FMath::FInterpTo(Pos.Z, Hit.ImpactPoint.Z, Dt, 12.0f);
		}
	}
	GroundPos = SolidStep(Pos, Next, Dt);
	ApplyTransform(GroundPos, ScatterDir.Rotation().Yaw, 0.0f, Dt, 0.0f);
	if (ScatterLeft <= 0.0f)
	{
		Abandon(false);
		if (Mode == EPDSkaterMode::Idle)
		{
			// Nothing worth skating: hang around.
			Rest = 1.0f;
		}
	}
}

void UPDSkaterBrainComponent::TickRide(float Dt)
{
	IPDSkateLine* L = Cast<IPDSkateLine>(Line.Get());
	IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!L || !Flow || L->GetPathLength() <= 0.0f)
	{
		Abandon(false);
		return;
	}
	const float Total = L->GetPathLength();
	const float FS = L->GetFeatureStartDistance();
	const float RE = L->GetRunEndDistance();

	// Resting at the loop start: decide whether this line is still worth it.
	if (Rest > 0.0f)
	{
		Rest -= Dt;
		SetPose(TEXT("Stance"));
		const FVector P = L->GetPointAtDistance(S);
		const FVector Fwd = (L->GetPointAtDistance(S + PDSkate::TangentProbe) - P).GetSafeNormal2D();
		// Two runners resting at the same loop start stand side by side, not inside each other.
		const float Slot = static_cast<float>(static_cast<int32>(GetOwner()->GetUniqueID() % 3) - 1) * 80.0f;
		GroundPos = P + FVector::CrossProduct(FVector::UpVector, Fwd) * Slot;
		ApplyTransform(GroundPos, Fwd.Rotation().Yaw, 0.0f, Dt, 0.0f);
		if (Rest <= 0.0f && (L->GetAppeal() < MinAppeal || Flow->GetTimeRemaining() < 2.0f))
		{
			Abandon(false);
		}
		return;
	}

	const bool bCommitted = IsCommitted();
	// Before committing, a dead or guarded line is dropped for a better one.
	if (!bCommitted && S < FS && (L->GetAppeal() < MinAppeal || !L->IsOpen()))
	{
		AActor* Other = ChooseLine(Line.Get());
		if (Other)
		{
			BeginTransferTo(Other);
			return;
		}
	}
	// A fresh prop in the way: kick-turn out of the run (bail if already committed).
	if (S < RE && L->IsBlocked() && ActiveTrick == INDEX_NONE)
	{
		UE_LOG(LogPlaza, Verbose, TEXT("[Plaza] %s turns away from blocked %s."), *GetOwner()->GetName(), *L->GetLineId().ToString());
		Abandon(bCommitted);
		return;
	}

	TArray<FTrick> Tricks;
	BuildTricks(Tricks);
	const bool bOnRun = S >= FS - CommitDistance && S <= RE;
	float Speed = (bOnRun ? SkaterTrickSpeed : (S > RE ? SkaterCruiseSpeed * ReturnSpeedFactor : SkaterCruiseSpeed)) * SpeedScale * PhaseSpeedScale();
	// Queue behind a runner just ahead on the same path instead of running through them (2026-11-14 collision pass).
	{
		const FVector Here = GroundPos;
		const FVector Fwd0 = (L->GetPointAtDistance(S + PDSkate::TangentProbe) - L->GetPointAtDistance(S)).GetSafeNormal2D();
		for (const AActor* Other : Flow->GetSkaters())
		{
			if (Other && Other != GetOwner() && !Other->IsHidden())
			{
				const FVector To = Other->GetActorLocation() - Here;
				const float Ahead = FVector::DotProduct(To, Fwd0);
				if (Ahead > 0.0f && Ahead < QueueGap && FMath::Abs(To.Z) < 150.0f && FVector::DistSquared2D(Other->GetActorLocation(), Here + Fwd0 * Ahead) < FMath::Square(70.0f))
				{
					Speed *= FMath::Clamp((Ahead - 80.0f) / (QueueGap - 80.0f), 0.0f, 1.0f);
				}
			}
		}
	}
	const float PrevS = S;
	S += Speed * Dt;

	// Trick events.
	float Height = 0.0f;
	bool bInTrick = false;
	bool bGrinding = false;
	for (const FTrick& T : Tricks)
	{
		if (!Started.Contains(T.Key) && PrevS < T.StartD && S >= T.StartD)
		{
			Started.Add(T.Key);
			ActiveTrick = T.Key;
			UPDAudioLibrary::PlaySynthAtLocation(this, PopSound, GroundPos, 1.0f, Random.FRandRange(0.92f, 1.08f));
			SetPose(TEXT("Pop"), true);
			PoseTimer = PDSkate::PopPoseSeconds;
		}
		if (T.bGrind && Started.Contains(T.Key) && PrevS < T.GrindStart && S >= T.GrindStart)
		{
			const bool bRail = L->GetFeatureHeight() > 70.0f && !T.bOnProp;
			UPDAudioLibrary::PlaySynthAtLocation(this, bRail ? RailSound : GrindSound, GroundPos, T.bRide ? 0.4f : 1.0f);
		}
		if (Started.Contains(T.Key) && !Landed.Contains(T.Key) && PrevS < T.EndD && S >= T.EndD)
		{
			Landed.Add(T.Key);
			if (ActiveTrick == T.Key)
			{
				ActiveTrick = INDEX_NONE;
			}
			SetPose(TEXT("Land"), true);
			PoseTimer = PDSkate::LandPoseSeconds;
			FPDTrickEvent Event;
			Event.Location = L->GetPointAtDistance(T.EndD);
			Event.TrickName = T.Name;
			Event.LineId = L->GetLineId();
			Event.Wear = T.Wear;
			Event.bOnProp = T.bOnProp;
			Event.Skater = GetOwner();
			L->NotifyTrickLanded(T.Wear);
			Flow->ReportTrick(Event);
		}
		if (Started.Contains(T.Key) && S >= T.StartD && S <= T.EndD)
		{
			Height = FMath::Max(Height, TrickHeight(T, S));
			bInTrick = true;
			bGrinding |= T.bGrind && S >= T.GrindStart && S <= T.GrindEnd;
		}
	}

	// Poses: pop -> air / crouch on the grind -> land -> stance.
	PoseTimer -= Dt;
	if (bGrinding)
	{
		SetPose(TEXT("Crouch"));
	}
	else if (PoseTimer <= 0.0f)
	{
		SetPose(bInTrick ? FName(TEXT("Air")) : FName(TEXT("Stance")));
	}

	// End of the run: keep this line or go elsewhere.
	if (!bLoopDecided && PrevS < RE && S >= RE)
	{
		bLoopDecided = true;
		AActor* Next = ChooseLine(nullptr);
		if (Next && Next != Line.Get())
		{
			GroundPos = L->GetPointAtDistance(S);
			BeginTransferTo(Next);
			return;
		}
	}
	if (S >= Total)
	{
		S -= Total;
		BeginRide(0.0f);
	}

	const FVector P = L->GetPointAtDistance(S);
	const FVector Ahead = L->GetPointAtDistance(S + PDSkate::TangentProbe);
	const FVector Behind = L->GetPointAtDistance(S - PDSkate::TangentProbe);
	const FVector Tangent = Ahead - Behind;
	const FVector Fwd = Tangent.GetSafeNormal2D();
	const bool bLocked = bCommitted || bInTrick;
	SideStep = FMath::FInterpConstantTo(SideStep, ComputeSideStep(P, Fwd, bLocked), Dt, SideStepRate);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	GroundPos = P + Right * SideStep;
	CheckKnockdown(GroundPos + FVector(0.0f, 0.0f, Height), bCommitted || bInTrick);
	if (Mode != EPDSkaterMode::Ride)
	{
		return;
	}
	// Board pitch follows ramps and the rail; nose up on the pop.
	float Pitch = bGrinding || !bInTrick ? FMath::RadiansToDegrees(FMath::Atan2(Tangent.Z, FMath::Max(1.0f, Tangent.Size2D()))) : 0.0f;
	if (bInTrick && !bGrinding && PoseTimer > 0.0f && CurrentPose == TEXT("Pop"))
	{
		Pitch += 14.0f;
	}
	ApplyTransform(GroundPos, Fwd.Rotation().Yaw, Height, Dt, Pitch);
}

void UPDSkaterBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow || DeltaTime <= 0.0f)
	{
		return;
	}
	StateTime += DeltaTime;
	switch (Mode)
	{
	case EPDSkaterMode::Transfer:
		TickTransfer(DeltaTime, false);
		break;
	case EPDSkaterMode::Home:
		TickTransfer(DeltaTime, true);
		break;
	case EPDSkaterMode::Ride:
		TickRide(DeltaTime);
		break;
	case EPDSkaterMode::Scatter:
		TickScatter(DeltaTime);
		break;
	case EPDSkaterMode::Bail:
		ApplyTransform(GroundPos, LastYaw, 0.0f, DeltaTime, 0.0f);
		if (StateTime >= BailSeconds)
		{
			Abandon(false);
		}
		break;
	case EPDSkaterMode::Idle:
		Rest -= DeltaTime;
		SetPose(TEXT("Stance"));
		ApplyTransform(GroundPos, LastYaw, 0.0f, DeltaTime, 0.0f);
		// Rolling during the start screen, intro and countdown too, so the plaza is alive behind the menu (wear only counts in Active).
		if (Rest <= 0.0f && (Flow->GetMatchState() == EPDMatchState::Active || Flow->GetMatchState() == EPDMatchState::Ready))
		{
			if (AActor* Next = ChooseLine(nullptr))
			{
				BeginTransferTo(Next);
			}
			else
			{
				Rest = 1.0f;
			}
		}
		break;
	default:
		break;
	}
}

float UPDSkaterBrainComponent::GetSecondsToPoint(const FVector& Point, float Radius) const
{
	const IPDSkateLine* L = Cast<IPDSkateLine>(Line.Get());
	if (!L)
	{
		return -1.0f;
	}
	const float Speed = FMath::Max(100.0f, SkaterTrickSpeed * SpeedScale * PhaseSpeedScale());
	const float RE = L->GetRunEndDistance();
	float Lead = 0.0f;
	float From = S;
	if (Mode == EPDSkaterMode::Transfer)
	{
		float Remaining = 0.0f;
		FVector P = GroundPos;
		for (int32 i = RouteIndex + 1; i < Route.Num(); ++i)
		{
			if (FVector::Dist2D(Route[i], Point) < Radius)
			{
				return (Remaining + FVector::Dist(P, Route[i])) / Speed;
			}
			Remaining += FVector::Dist(P, Route[i]);
			P = Route[i];
		}
		Lead = Remaining / Speed;
		From = S;
	}
	else if (Mode == EPDSkaterMode::Ride)
	{
		Lead = FMath::Max(0.0f, Rest);
	}
	else
	{
		return -1.0f;
	}
	// Walk the plan up to the end of the next run.
	const float Limit = From <= RE ? RE : L->GetPathLength() + RE;
	for (float D = From; D <= Limit; D += PDSkate::SearchStep)
	{
		if (FVector::Dist2D(L->GetPointAtDistance(D), Point) < Radius && FMath::Abs(L->GetPointAtDistance(D).Z - Point.Z) < 200.0f)
		{
			return Lead + (D - From) / Speed;
		}
	}
	return -1.0f;
}
