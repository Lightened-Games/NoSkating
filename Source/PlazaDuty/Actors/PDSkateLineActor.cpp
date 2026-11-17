// Copyright Plaza Duty. All Rights Reserved.

#include "Actors/PDSkateLineActor.h"
#include "Data/PDMatchConfig.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PlazaDuty.h"

#define LOCTEXT_NAMESPACE "PlazaDuty"

namespace PDLine
{
	const TCHAR* MeshDir = TEXT("/Game/PlazaDuty/Meshes/");
	const TCHAR* MatDir = TEXT("/Game/PlazaDuty/Materials/");

	FSoftObjectPath Mesh(const TCHAR* Name) { return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), MeshDir, Name, Name)); }
	FSoftObjectPath Mat(const TCHAR* Name) { return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), MatDir, Name, Name)); }

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Mat, const FName& Name)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		// Line markers (lane chalk, chevron, socket, knob cap) are non-solid by design: tagged for the collision audit (C2).
		C->ComponentTags.AddUnique(PD::NoCollideTag);
		C->SetStaticMesh(Mesh);
		if (Mat)
		{
			C->SetMaterial(0, Mat);
		}
		C->RegisterComponent();
		return C;
	}
}

APDSkateLineActor::APDSkateLineActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Tags.Add(PD::LineTag);

	LaneMesh = TSoftObjectPtr<UStaticMesh>(PDLine::Mesh(TEXT("SM_PD_Plane")));
	LaneMaterial = TSoftObjectPtr<UMaterialInterface>(PDLine::Mat(TEXT("MI_PD_Lane")));
	KnobMesh = TSoftObjectPtr<UStaticMesh>(PDLine::Mesh(TEXT("SM_PD_Knob")));
	KnobMaterial = TSoftObjectPtr<UMaterialInterface>(PDLine::Mat(TEXT("MI_PD_KnobSteel")));
	SocketMesh = TSoftObjectPtr<UStaticMesh>(PDLine::Mesh(TEXT("SM_PD_Ring")));
	ChevronMesh = TSoftObjectPtr<UStaticMesh>(PDLine::Mesh(TEXT("SM_PD_Chevron")));
	AccentMaterial = TSoftObjectPtr<UMaterialInterface>(PDLine::Mat(TEXT("MI_PD_AccentGlow")));
}

void APDSkateLineActor::BeginPlay()
{
	Super::BeginPlay();
	BuildCache();
	BuildVisuals();
	UpdateTimer = FMath::FRandRange(0.0f, UpdateInterval);
}

void APDSkateLineActor::BuildCache()
{
	Cumulative.Reset();
	TotalLength = 0.0f;
	const int32 N = Points.Num();
	if (N < 2)
	{
		return;
	}
	for (int32 i = 0; i < N; ++i)
	{
		Cumulative.Add(TotalLength);
		TotalLength += FVector::Dist(Points[i], Points[(i + 1) % N]);
	}
}

float APDSkateLineActor::GetFeatureStartDistance() const
{
	return Cumulative.IsValidIndex(FeatureStartIndex) ? Cumulative[FeatureStartIndex] : 0.0f;
}

float APDSkateLineActor::GetFeatureEndDistance() const
{
	return Cumulative.IsValidIndex(FeatureEndIndex) ? Cumulative[FeatureEndIndex] : 0.0f;
}

float APDSkateLineActor::GetRunEndDistance() const
{
	return Cumulative.IsValidIndex(RunEndIndex) ? Cumulative[RunEndIndex] : GetFeatureEndDistance();
}

FVector APDSkateLineActor::GetPointAtDistance(float Distance) const
{
	const int32 N = Points.Num();
	if (N == 0)
	{
		return GetActorLocation();
	}
	if (N == 1 || TotalLength <= KINDA_SMALL_NUMBER)
	{
		return Points[0];
	}
	float D = FMath::Fmod(Distance, TotalLength);
	if (D < 0.0f)
	{
		D += TotalLength;
	}
	for (int32 i = 0; i < N; ++i)
	{
		const float Start = Cumulative[i];
		const float End = (i + 1 < N) ? Cumulative[i + 1] : TotalLength;
		if (D <= End || i == N - 1)
		{
			const float Len = FMath::Max(End - Start, KINDA_SMALL_NUMBER);
			return FMath::Lerp(Points[i], Points[(i + 1) % N], FMath::Clamp((D - Start) / Len, 0.0f, 1.0f));
		}
	}
	return Points[0];
}

float APDSkateLineActor::GetDistanceClosestTo(const FVector& World) const
{
	const int32 N = Points.Num();
	float Best = TNumericLimits<float>::Max();
	float BestDistance = 0.0f;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[(i + 1) % N];
		const FVector C = FMath::ClosestPointOnSegment(World, A, B);
		const float D = FVector::DistSquared(C, World);
		if (D < Best)
		{
			Best = D;
			BestDistance = Cumulative[i] + FVector::Dist(A, C);
		}
	}
	return BestDistance;
}

float APDSkateLineActor::DistanceToRun(const FVector& World, float& OutPathDistance) const
{
	// The run: from the loop start through the landing. The ride back doesn't count (a prop there blocks nothing).
	float Best = TNumericLimits<float>::Max();
	OutPathDistance = 0.0f;
	const int32 Last = FMath::Clamp(RunEndIndex, 1, Points.Num() - 1);
	for (int32 i = 0; i < Last; ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		const FVector C = FMath::ClosestPointOnSegment(World, A, B);
		const float D = FVector::Dist2D(C, World) + FMath::Max(0.0f, FMath::Abs(C.Z - World.Z) - 120.0f);
		if (D < Best)
		{
			Best = D;
			OutPathDistance = Cumulative[i] + FVector::Dist(A, C);
		}
	}
	return Best;
}

bool APDSkateLineActor::IsOpen() const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	return !Flow || Flow->GetElapsed() >= OpensAt || Flow->GetMatchState() != EPDMatchState::Active;
}

bool APDSkateLineActor::IsBlocked() const
{
	for (const FTracked& T : Tracked)
	{
		if (T.Prop.IsValid() && !T.bConverted)
		{
			return true;
		}
	}
	return false;
}

float APDSkateLineActor::GetAppeal() const
{
	if (!IsOpen() || IsBlocked())
	{
		return 0.0f;
	}
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const float PropScale = Flow ? Flow->GetPhase().PropAppealScale : 1.0f;
	float Appeal = bCapped ? 0.0f : BaseAppeal;
	for (const FPDPropTrick& T : PropTricks)
	{
		const IPDDraggable* Prop = Cast<IPDDraggable>(T.Prop.Get());
		const UPDMatchConfig* Config = Flow ? Flow->GetMatchConfig() : nullptr;
		if (Prop && Config)
		{
			Appeal += Config->GetPropTuning(Prop->GetPropKind()).AppealBonus * PropScale;
		}
	}
	return bGuardNear ? Appeal * GuardDeterFactor : Appeal;
}

float APDSkateLineActor::GetWearPerRun() const
{
	float Wear = bCapped ? 0.0f : BaseWear;
	for (const FPDPropTrick& T : PropTricks)
	{
		Wear += T.Wear;
	}
	return Wear;
}

void APDSkateLineActor::NotifyRun(AActor* Skater)
{
}

void APDSkateLineActor::NotifyTrickLanded(float Wear)
{
	Heat = FMath::Min(1.0f, Heat + HeatPerTrick);
}

bool APDSkateLineActor::CanInteract(const AActor* Guard) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	return bCappable && !bCapped && Flow && Flow->GetKnobsLeft() > 0 && Flow->GetMatchState() == EPDMatchState::Active;
}

FText APDSkateLineActor::GetInteractLabel() const
{
	return LOCTEXT("BoltLabel", "HOLD E  BOLT");
}

void APDSkateLineActor::Bolt()
{
	if (bCapped)
	{
		return;
	}
	bCapped = true;
	KnobPop = 1.0f;
	for (UStaticMeshComponent* K : Knobs)
	{
		K->SetVisibility(true);
	}
	if (Socket) { Socket->SetVisibility(false); }
	if (Chevron) { Chevron->SetVisibility(false); }
	if (IPDMatchFlow* Flow = PD::GetFlow(this))
	{
		Flow->ReportLineCapped(this);
	}
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Line %s capped."), *LineId.ToString());
}

void APDSkateLineActor::BuildVisuals()
{
	UStaticMesh* Plane = LaneMesh.LoadSynchronous();
	UMaterialInterface* LaneMat = LaneMaterial.LoadSynchronous();
	if (Plane && Points.Num() >= 2)
	{
		// Chalk lane: the approach and the feature (what the skaters actually skate), not the ride back.
		const int32 From = FMath::Max(0, FeatureStartIndex - 1);
		const int32 To = FMath::Min(FeatureEndIndex, Points.Num() - 1);
		const FBoxSphereBounds B = Plane->GetBounds();
		const float PlaneSize = FMath::Max(1.0f, B.BoxExtent.X * 2.0f);
		for (int32 i = From; i < To; ++i)
		{
			const FVector A = Points[i];
			const FVector C = Points[i + 1];
			const FVector Dir = (C - A);
			const float Len = Dir.Size2D();
			if (Len < 10.0f || FMath::Abs(C.Z - A.Z) > 60.0f)
			{
				continue; // drops (stair gaps) get no lane: the skater is in the air there
			}
			UStaticMeshComponent* Part = PDLine::MakePart(this, Root, Plane, LaneMat, *FString::Printf(TEXT("Lane%d"), i));
			Part->SetWorldLocation((A + C) * 0.5f + FVector(0.0f, 0.0f, 1.2f));
			Part->SetWorldRotation(FRotator(0.0f, Dir.Rotation().Yaw, 0.0f));
			Part->SetWorldScale3D(FVector(Len / PlaneSize, LaneWidth / PlaneSize, 1.0f));
			Part->SetCastShadow(false);
			Part->SetTranslucentSortPriority(1);
			LaneParts.Add(Part);
			LaneMids.Add(Part->CreateDynamicMaterialInstance(0));
		}
	}

	UStaticMesh* KnobM = KnobMesh.LoadSynchronous();
	UMaterialInterface* KnobMat = KnobMaterial.LoadSynchronous();
	if (KnobM && bCappable && Points.IsValidIndex(FeatureStartIndex) && Points.IsValidIndex(FeatureEndIndex))
	{
		const FVector A = Points[FeatureStartIndex];
		const FVector C = Points[FeatureEndIndex];
		for (int32 k = 0; k < KnobCount; ++k)
		{
			const float T = (k + 1.0f) / (KnobCount + 1.0f);
			UStaticMeshComponent* Knob = PDLine::MakePart(this, Root, KnobM, KnobMat, *FString::Printf(TEXT("Knob%d"), k));
			Knob->SetWorldLocation(FMath::Lerp(A, C, T) + FVector(0.0f, 0.0f, FeatureHeight));
			Knob->SetWorldRotation(FRotator(0.0f, (C - A).Rotation().Yaw, 0.0f));
			Knob->SetVisibility(false);
			Knob->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Knob->SetCollisionResponseToAllChannels(ECR_Block);
			Knob->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			Knobs.Add(Knob);
		}
	}

	UMaterialInterface* Accent = AccentMaterial.LoadSynchronous();
	if (bCappable)
	{
		if (UStaticMesh* Ring = SocketMesh.LoadSynchronous())
		{
			Socket = PDLine::MakePart(this, Root, Ring, Accent, TEXT("Socket"));
			Socket->SetWorldLocation(KnobPoint + FVector(0.0f, 0.0f, FeatureHeight + 2.0f));
			Socket->SetWorldScale3D(FVector(0.32f));
			Socket->SetCastShadow(false);
		}
		if (UStaticMesh* Chev = ChevronMesh.LoadSynchronous())
		{
			Chevron = PDLine::MakePart(this, Root, Chev, Accent, TEXT("Chevron"));
			Chevron->SetWorldLocation(KnobPoint + FVector(0.0f, 0.0f, FeatureHeight + 90.0f));
			Chevron->SetWorldScale3D(FVector(0.5f));
			Chevron->SetCastShadow(false);
		}
	}
}

void APDSkateLineActor::UpdateProps()
{
	IPDMatchFlow* Flow = PD::GetFlow(this);
	const UPDMatchConfig* Config = Flow ? Flow->GetMatchConfig() : nullptr;
	if (!Flow || !Config)
	{
		return;
	}
	const bool bActive = Flow->GetMatchState() == EPDMatchState::Active;
	const TArray<AActor*> Props = Flow->GetProps();

	// Drop anything gone or moved off the run.
	for (int32 i = Tracked.Num() - 1; i >= 0; --i)
	{
		FTracked& T = Tracked[i];
		AActor* PropActor = T.Prop.Get();
		IPDDraggable* Prop = Cast<IPDDraggable>(PropActor);
		float PathD = 0.0f;
		const bool bStillOn = Prop && !Prop->IsHeld() && DistanceToRun(PropActor->GetActorLocation(), PathD) < LineBlockRadius + Prop->GetFootprintRadius();
		if (!bStillOn)
		{
			if (Prop && T.bConverted)
			{
				Prop->SetConverted(false, FText::GetEmpty());
				Flow->ReportPropMoved(PropActor, false);
			}
			Tracked.RemoveAt(i);
		}
	}

	// Pick up props that came to rest on the run.
	for (AActor* PropActor : Props)
	{
		IPDDraggable* Prop = Cast<IPDDraggable>(PropActor);
		if (!Prop || Prop->IsHeld() || Prop->GetRestSeconds() < 0.3f)
		{
			continue;
		}
		if (Tracked.ContainsByPredicate([PropActor](const FTracked& T) { return T.Prop.Get() == PropActor; }))
		{
			continue;
		}
		float PathD = 0.0f;
		if (DistanceToRun(PropActor->GetActorLocation(), PathD) < LineBlockRadius + Prop->GetFootprintRadius())
		{
			FTracked& T = Tracked.AddDefaulted_GetRef();
			T.Prop = PropActor;
			T.Since = Flow->GetElapsed();
			T.PathDistance = PathD;
			if (bActive)
			{
				Flow->ReportPropMoved(PropActor, true);
			}
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f %s blocks line %s."), Flow->GetElapsed(), *PropActor->GetName(), *LineId.ToString());
		}
	}

	// Convert props that sat long enough: the skaters work them out.
	PropTricks.Reset();
	for (FTracked& T : Tracked)
	{
		AActor* PropActor = T.Prop.Get();
		IPDDraggable* Prop = Cast<IPDDraggable>(PropActor);
		if (!Prop)
		{
			continue;
		}
		const FPDPropTuning& Tune = Config->GetPropTuning(Prop->GetPropKind());
		float PathD = T.PathDistance;
		DistanceToRun(PropActor->GetActorLocation(), PathD);
		T.PathDistance = PathD;
		const bool bLaunch = TrickStyle == EPDTrickStyle::Gap && FMath::Abs(PathD - GetFeatureStartDistance()) < LaunchRadius && Tune.LaunchWear > 0.0f;
		if (!T.bConverted && bActive && Flow->GetElapsed() - T.Since >= Tune.BlockSeconds)
		{
			T.bConverted = true;
			const FText Trick = bLaunch ? Tune.LaunchTrickName : Tune.TrickName;
			Prop->SetConverted(true, Trick);
			Flow->ReportPropConverted(this, PropActor, Trick);
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f skaters learned %s on %s: %s."), Flow->GetElapsed(), *PropActor->GetName(), *LineId.ToString(), *Trick.ToString());
		}
		if (T.bConverted)
		{
			FPDPropTrick& Trick = PropTricks.AddDefaulted_GetRef();
			Trick.Prop = PropActor;
			Trick.PathDistance = PathD;
			Trick.TrickName = bLaunch ? Tune.LaunchTrickName : Tune.TrickName;
			Trick.Wear = bLaunch ? Tune.LaunchWear : Tune.TrickWear;
			Trick.ClearHeight = Tune.ClearHeight;
			Trick.RideLength = Tune.RideLength;
			Trick.bLaunch = bLaunch;
		}
	}
	PropTricks.Sort([](const FPDPropTrick& A, const FPDPropTrick& B) { return A.PathDistance < B.PathDistance; });

	// The guard standing on the feature puts skaters off (they pick another line unless already committed).
	bGuardNear = false;
	if (const AActor* Guard = Flow->GetGuard())
	{
		if (Points.IsValidIndex(FeatureStartIndex) && Points.IsValidIndex(FeatureEndIndex))
		{
			const FVector C = FMath::ClosestPointOnSegment(Guard->GetActorLocation(), Points[FeatureStartIndex], Points[FeatureEndIndex]);
			bGuardNear = FVector::Dist2D(C, Guard->GetActorLocation()) < GuardDeterRadius && FMath::Abs(C.Z - Guard->GetActorLocation().Z) < 200.0f;
		}
	}
}

void APDSkateLineActor::UpdateVisuals(float DeltaSeconds)
{
	Heat = FMath::Max(0.0f, Heat - HeatDecay * DeltaSeconds);
	Age += DeltaSeconds;
	const bool bOpen = IsOpen();
	const bool bFeatureLive = bOpen && !bCapped;
	const float Glow = !bOpen ? 0.0f : (bCapped && PropTricks.Num() == 0 ? 0.08f : FMath::Lerp(LaneGlowIdle, LaneGlowHot, Heat));
	const bool bBlocked = IsBlocked();
	for (UMaterialInstanceDynamic* Mid : LaneMids)
	{
		if (Mid)
		{
			Mid->SetScalarParameterValue(TEXT("Glow"), bBlocked ? Glow * 0.4f : Glow);
		}
	}
	for (UStaticMeshComponent* Part : LaneParts)
	{
		Part->SetVisibility(bOpen);
	}

	// Knobs pop in with a short scale tween.
	if (KnobPop > 0.0f)
	{
		KnobPop = FMath::Max(0.0f, KnobPop - DeltaSeconds * 3.0f);
		const float S = 1.0f + 0.6f * FMath::Sin(KnobPop * PI);
		for (UStaticMeshComponent* K : Knobs)
		{
			K->SetRelativeScale3D(FVector(S));
		}
	}

	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const bool bShowSocket = !bCapped && bCappable && Flow && Flow->GetKnobsLeft() > 0 && Flow->GetMatchState() != EPDMatchState::Victory && Flow->GetMatchState() != EPDMatchState::Defeat;
	if (Socket)
	{
		Socket->SetVisibility(bShowSocket);
	}
	if (Chevron)
	{
		Chevron->SetVisibility(bShowSocket);
		const float Bob = FMath::Sin(Age * 3.0f) * SocketBobHeight * (bIsFocused ? 0.4f : 1.0f);
		Chevron->SetWorldLocation(KnobPoint + FVector(0.0f, 0.0f, FeatureHeight + 95.0f + Bob));
		Chevron->SetWorldRotation(FRotator(0.0f, Age * 60.0f, 0.0f));
	}
}

void APDSkateLineActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTimer -= DeltaSeconds;
	if (UpdateTimer <= 0.0f)
	{
		UpdateTimer = UpdateInterval;
		UpdateProps();
	}
	UpdateVisuals(DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE
