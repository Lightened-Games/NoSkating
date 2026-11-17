// Copyright Plaza Duty. All Rights Reserved.

#include "Components/PDGuardComponents.h"
#include "Interfaces/PDInterfaces.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PlazaDuty.h"

// =================================================================================================
// Interact
// =================================================================================================

UPDInteractComponent::UPDInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

EPDInteractKind UPDInteractComponent::GetFocusKind() const
{
	const IPDInteractable* I = Cast<IPDInteractable>(Focus.Get());
	return I ? I->GetInteractKind() : EPDInteractKind::None;
}

AActor* UPDInteractComponent::FindBest(EPDInteractKind Kind, const FVector& From, const FVector& Facing) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow)
	{
		return nullptr;
	}
	TArray<AActor*> Candidates = Kind == EPDInteractKind::Bolt ? Flow->GetSkateLines() : Flow->GetProps();
	const float Range = Kind == EPDInteractKind::Bolt ? BoltRange : GrabRange;
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* A : Candidates)
	{
		const IPDInteractable* I = Cast<IPDInteractable>(A);
		if (!I || I->GetInteractKind() != Kind || !I->CanInteract(GetOwner()))
		{
			continue;
		}
		FVector P = I->GetInteractPoint();
		if (const IPDDraggable* D = Cast<IPDDraggable>(A))
		{
			// Measure to the prop's edge, not its centre (benches are long).
			const FVector To = P - From;
			P -= To.GetSafeNormal2D() * FMath::Min(D->GetFootprintRadius() * 0.7f, To.Size2D());
		}
		const float D2 = FVector::Dist2D(P, From);
		if (D2 > Range || FMath::Abs(P.Z - From.Z) > 220.0f)
		{
			continue;
		}
		const float Dot = FVector::DotProduct((P - From).GetSafeNormal2D(), Facing.GetSafeNormal2D());
		const float Score = D2 - Dot * FacingBonus;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = A;
		}
	}
	return Best;
}

void UPDInteractComponent::ClearFocus()
{
	if (IPDInteractable* Old = Cast<IPDInteractable>(Focus.Get()))
	{
		Old->SetFocused(false);
	}
	Focus = nullptr;
	OnFocusChanged.Broadcast(nullptr);
}

void UPDInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Owner || !Flow)
	{
		return;
	}
	const FVector From = Owner->GetActorLocation() - FVector(0.0f, 0.0f, 80.0f);
	const FVector Facing = Owner->GetActorForwardVector();
	const IPDKnockable* Guard = Cast<IPDKnockable>(Owner);
	const EPDGuardAction Action = Guard ? Guard->GetGuardAction() : EPDGuardAction::None;

	AActor* NewFocus = nullptr;
	if (Action == EPDGuardAction::None && Flow->GetMatchState() == EPDMatchState::Active)
	{
		AActor* Bolt = FindBest(EPDInteractKind::Bolt, From, Facing);
		AActor* Prop = FindBest(EPDInteractKind::Drag, From, Facing);
		// A knob socket wins when both are in reach: bolting is the rarer, more deliberate act.
		NewFocus = Bolt ? Bolt : Prop;
	}
	else if (Action == EPDGuardAction::Bolting || Action == EPDGuardAction::Dragging)
	{
		NewFocus = Focus.Get();
	}
	if (NewFocus != Focus.Get())
	{
		if (IPDInteractable* Old = Cast<IPDInteractable>(Focus.Get()))
		{
			Old->SetFocused(false);
		}
		Focus = NewFocus;
		if (IPDInteractable* New = Cast<IPDInteractable>(NewFocus))
		{
			New->SetFocused(true);
		}
		OnFocusChanged.Broadcast(NewFocus);
	}

	// First-time prompts: shown a little before the guard reaches the thing (the GameMode dedups them).
	if (Flow->GetMatchState() == EPDMatchState::Active)
	{
		for (AActor* Line : Flow->GetSkateLines())
		{
			const IPDSkateLine* L = Cast<IPDSkateLine>(Line);
			const IPDInteractable* I = Cast<IPDInteractable>(Line);
			if (L && I && I->CanInteract(Owner) && FVector::Dist2D(L->GetKnobPoint(), From) < PromptRange)
			{
				Flow->ReportPrompt(EPDPrompt::Bolt, Line);
				break;
			}
		}
		for (AActor* Prop : Flow->GetProps())
		{
			const IPDInteractable* I = Cast<IPDInteractable>(Prop);
			if (I && I->CanInteract(Owner) && FVector::Dist2D(Prop->GetActorLocation(), From) < PromptRange * 0.6f)
			{
				Flow->ReportPrompt(EPDPrompt::Drag, Prop);
				break;
			}
		}
	}
}

// =================================================================================================
// Bolt
// =================================================================================================

UPDBoltComponent::UPDBoltComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	RatchetSound = UPDAudioLibrary::BoltRatchet();
	SegmentMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Meshes/SM_PD_RingSegment.SM_PD_RingSegment")));
	SegmentMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_AccentGlow.MI_PD_AccentGlow")));
	SegmentTrackMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_TrackDim.MI_PD_TrackDim")));
}

void UPDBoltComponent::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Mesh = SegmentMesh.LoadSynchronous();
	USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!Mesh || !Root)
	{
		return;
	}
	UMaterialInterface* Track = SegmentTrackMaterial.LoadSynchronous();
	for (int32 i = 0; i < RingSegments; ++i)
	{
		UStaticMeshComponent* Seg = NewObject<UStaticMeshComponent>(GetOwner(), *FString::Printf(TEXT("BoltRing%d"), i));
		Seg->SetupAttachment(Root);
		Seg->SetStaticMesh(Mesh);
		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Seg->SetCastShadow(false);
		Seg->SetCanEverAffectNavigation(false);
		Seg->SetUsingAbsoluteRotation(true);
		if (Track)
		{
			Seg->SetMaterial(0, Track);
		}
		Seg->SetVisibility(false);
		Seg->RegisterComponent();
		Segments.Add(Seg);
	}
}

bool UPDBoltComponent::StartBolt(AActor* Line)
{
	IPDSkateLine* L = Cast<IPDSkateLine>(Line);
	const IPDInteractable* I = Cast<IPDInteractable>(Line);
	AActor* Owner = GetOwner();
	if (!L || !I || !Owner || !I->CanInteract(Owner) || IsBolting())
	{
		return false;
	}
	Target = Line;
	Progress = 0.0f;
	RatchetTimer = 0.0f;
	StepAge = 0.0f;
	StepFrom = Owner->GetActorLocation();
	// Step in to the stand point beside the feature (keeps the capsule height).
	StepTo = FVector(L->GetBoltStandPoint().X, L->GetBoltStandPoint().Y, StepFrom.Z);
	if (const ACharacter* C = Cast<ACharacter>(Owner))
	{
		C->GetCharacterMovement()->StopMovementImmediately();
	}
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Bolting %s."), *L->GetLineId().ToString());
	return true;
}

void UPDBoltComponent::CancelBolt(bool bInterrupted)
{
	if (!IsBolting())
	{
		return;
	}
	if (const IPDSkateLine* L = Cast<IPDSkateLine>(Target.Get()))
	{
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] Bolt on %s %s at %.0f%%."), *L->GetLineId().ToString(), bInterrupted ? TEXT("interrupted") : TEXT("released"), Progress * 100.0f);
	}
	Target = nullptr;
	Progress = 0.0f;
	UpdateRing();
}

void UPDBoltComponent::UpdateRing()
{
	const bool bShow = IsBolting();
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	UMaterialInterface* Lit = SegmentMaterial.LoadSynchronous();
	UMaterialInterface* Track = SegmentTrackMaterial.LoadSynchronous();
	const FVector Feet = Owner->GetActorLocation() - FVector(0.0f, 0.0f, 86.0f);
	const int32 LitCount = FMath::FloorToInt(Progress * Segments.Num() + 0.001f);
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		UStaticMeshComponent* Seg = Segments[i];
		Seg->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		// Clockwise from the top of the screen (+X).
		const float Yaw = 360.0f * i / Segments.Num();
		Seg->SetWorldLocation(Feet);
		Seg->SetWorldRotation(FRotator(0.0f, Yaw, 0.0f));
		Seg->SetWorldScale3D(FVector(RingRadius / 100.0f));
		UMaterialInterface* Want = i < LitCount ? Lit : Track;
		if (Want && Seg->GetMaterial(0) != Want)
		{
			Seg->SetMaterial(0, Want);
		}
	}
}

void UPDBoltComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsBolting())
	{
		return;
	}
	AActor* Owner = GetOwner();
	IPDSkateLine* L = Cast<IPDSkateLine>(Target.Get());
	const IPDInteractable* I = Cast<IPDInteractable>(Target.Get());
	IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Owner || !L || !I || !Flow || !I->CanInteract(Owner))
	{
		CancelBolt(false);
		return;
	}
	// Step in and face the knob, then the clock runs.
	StepAge += DeltaTime;
	const float StepAlpha = StepInSeconds > 0.0f ? FMath::Clamp(StepAge / StepInSeconds, 0.0f, 1.0f) : 1.0f;
	Owner->SetActorLocation(FMath::Lerp(StepFrom, StepTo, FMath::InterpEaseOut(0.0f, 1.0f, StepAlpha, 2.0f)), true);
	const FVector ToKnob = L->GetKnobPoint() - Owner->GetActorLocation();
	Owner->SetActorRotation(FMath::RInterpTo(Owner->GetActorRotation(), FRotator(0.0f, ToKnob.Rotation().Yaw, 0.0f), DeltaTime, 14.0f));
	if (StepAlpha < 1.0f)
	{
		UpdateRing();
		return;
	}
	Progress = FMath::Min(1.0f, Progress + DeltaTime / FMath::Max(0.2f, BoltHoldSeconds));
	RatchetTimer -= DeltaTime;
	if (RatchetTimer <= 0.0f)
	{
		RatchetTimer = RatchetInterval;
		UPDAudioLibrary::PlaySynthAtLocation(this, RatchetSound, L->GetKnobPoint(), 1.0f, 0.9f + 0.3f * Progress);
	}
	UpdateRing();
	if (Progress >= 1.0f)
	{
		Target = nullptr;
		Progress = 0.0f;
		UpdateRing();
		if (Flow->ConsumeKnob())
		{
			L->Bolt();
		}
	}
}

// =================================================================================================
// Drag
// =================================================================================================

UPDDragComponent::UPDDragComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	GrabSound = UPDAudioLibrary::GrabClick();
	DropSound = UPDAudioLibrary::DropThump();
}

bool UPDDragComponent::Grab(AActor* Prop)
{
	IPDDraggable* D = Cast<IPDDraggable>(Prop);
	const IPDInteractable* I = Cast<IPDInteractable>(Prop);
	if (!D || !I || IsDragging() || !I->CanInteract(GetOwner()))
	{
		return false;
	}
	Held = Prop;
	FPDPropBox Box;
	bCarrying = PropBox(Prop, Box) && Box.Center.Z + Box.Extent.Z - SoleZ() < CarryBelowTopHeight;
	CarryLift = 0.0f;
	Prop->SetOwner(GetOwner());
	D->BeginDrag(GetOwner());
	if (ACharacter* C = Cast<ACharacter>(GetOwner()))
	{
		// Turn to face the prop.
		const FVector To = Prop->GetActorLocation() - C->GetActorLocation();
		C->SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
	}
	UPDAudioLibrary::PlaySynthAtLocation(this, GrabSound, Prop->GetActorLocation());
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Grabbed %s."), *Prop->GetName());
	return true;
}

void UPDDragComponent::Release()
{
	AActor* Prop = Held.Get();
	IPDDraggable* D = Cast<IPDDraggable>(Prop);
	const AActor* Owner = GetOwner();
	if (D && Owner && bCarrying)
	{
		// A carried prop is held inside the capsule radius (bent elbows): set it down at the normal drag distance on the
		// floor first, so its collision never comes back on inside the guard (FactoryQA penetration, round 3).
		float CapsuleRadius = 34.0f;
		if (const ACharacter* C = Cast<ACharacter>(Owner))
		{
			CapsuleRadius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
		}
		const FVector Fwd = Owner->GetActorForwardVector().GetSafeNormal2D();
		D->SetCarryLift(0.0f);
		D->DragTo(Owner->GetActorLocation() + Fwd * (CapsuleRadius + HoldGap + D->GetFootprintRadius()), Owner->GetActorRotation().Yaw);
	}
	CarryLift = 0.0f;
	bCarrying = false;
	Held = nullptr;
	if (D)
	{
		D->EndDrag();
		UPDAudioLibrary::PlaySynthAtLocation(this, DropSound, Prop->GetActorLocation());
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] Dropped %s at (%.0f, %.0f)."), *Prop->GetName(), Prop->GetActorLocation().X, Prop->GetActorLocation().Y);
	}
}

void UPDDragComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Prop = Held.Get();
	IPDDraggable* D = Cast<IPDDraggable>(Prop);
	const AActor* Owner = GetOwner();
	if (!D || !Owner)
	{
		if (Prop == nullptr && Held.IsStale())
		{
			Held = nullptr;
		}
		return;
	}
	float CapsuleRadius = 34.0f;
	if (const ACharacter* C = Cast<ACharacter>(Owner))
	{
		CapsuleRadius = C->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}
	const FVector Fwd = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float Gap = bCarrying ? CarryGap : HoldGap;
	const FVector Target = Owner->GetActorLocation() + Fwd * (CapsuleRadius + Gap + D->GetFootprintRadius());
	// A carried prop comes up so its middle sits at the hands; a dragged one stays on the floor.
	FPDPropBox Box;
	CarryLift = bCarrying && PropBox(Prop, Box) ? FMath::Max(0.0f, CarryHandHeight - (Box.Center.Z - CarryLift - SoleZ())) : 0.0f;
	D->SetCarryLift(CarryLift);
	D->DragTo(Target, Owner->GetActorRotation().Yaw);
}

float UPDDragComponent::SoleZ() const
{
	const AActor* Owner = GetOwner();
	float HalfHeight = 90.0f;
	if (const ACharacter* C = Cast<ACharacter>(Owner))
	{
		HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return Owner ? Owner->GetActorLocation().Z - HalfHeight : 0.0f;
}

bool UPDDragComponent::PropBox(const AActor* Prop, FPDPropBox& Out)
{
	// The prop's own mesh box (oriented, pivot-independent); actor bounds would include its floor ring.
	const UStaticMeshComponent* M = Prop ? Cast<UStaticMeshComponent>(Prop->GetRootComponent()) : nullptr;
	if (!M || !M->GetStaticMesh())
	{
		return false;
	}
	const FBoxSphereBounds B = M->GetStaticMesh()->GetBounds();
	const FTransform& T = M->GetComponentTransform();
	Out.Center = T.TransformPosition(B.Origin);
	Out.AxisX = T.GetUnitAxis(EAxis::X);
	Out.AxisY = T.GetUnitAxis(EAxis::Y);
	Out.Extent = B.BoxExtent * T.GetScale3D().GetAbs();
	return true;
}

bool UPDDragComponent::GetGrip(FVector& OutLeft, FVector& OutRight, float& OutLeanDegrees) const
{
	const AActor* Prop = Held.Get();
	const AActor* Owner = GetOwner();
	FPDPropBox Box;
	if (!Prop || !Owner || !PropBox(Prop, Box))
	{
		return false;
	}
	const FVector Fwd = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Owner->GetActorRightVector().GetSafeNormal2D();
	auto Half = [&Box](const FVector& Dir) { return FMath::Abs(FVector::DotProduct(Dir, Box.AxisX)) * Box.Extent.X + FMath::Abs(FVector::DotProduct(Dir, Box.AxisY)) * Box.Extent.Y; };
	const FVector ToBox = Box.Center - Owner->GetActorLocation();
	const float NearFace = FVector::DotProduct(Fwd, ToBox) - Half(Fwd);
	const float Lateral = FVector::DotProduct(Right, ToBox);
	const float HalfWidth = Half(Right);
	const FVector Ground(Owner->GetActorLocation().X, Owner->GetActorLocation().Y, 0.0f);
	if (bCarrying)
	{
		// Both hands on the side faces, a little in from the near face, at the box's middle height.
		const FVector Mid = Ground + FVector(0.0f, 0.0f, Box.Center.Z) + Fwd * (NearFace + FMath::Min(CarryHandDepth, Half(Fwd))) + Right * Lateral;
		OutLeft = Mid - Right * (HalfWidth + CarryHandSideOffset);
		OutRight = Mid + Right * (HalfWidth + CarryHandSideOffset);
		OutLeanDegrees = CarryLeanDegrees;
	}
	else
	{
		// A tall prop pushed along the floor: both hands on its near top edge.
		const FVector Edge = Ground + FVector(0.0f, 0.0f, Box.Center.Z + Box.Extent.Z - 1.0f) + Fwd * (NearFace + GripInset) + Right * Lateral;
		const float Spread = FMath::Clamp(HalfWidth - 4.0f, 8.0f, GripHalfSpread);
		OutLeft = Edge - Right * Spread;
		OutRight = Edge + Right * Spread;
		OutLeanDegrees = DragLeanDegrees;
	}
	return true;
}

// =================================================================================================
// Whistle
// =================================================================================================

UPDWhistleComponent::UPDWhistleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	ReadySound = UPDAudioLibrary::WhistleReady();
}

int32 UPDWhistleComponent::CountInRange() const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const AActor* Owner = GetOwner();
	int32 N = 0;
	if (Flow && Owner)
	{
		for (const AActor* S : Flow->GetSkaters())
		{
			if (S && FVector::Dist2D(S->GetActorLocation(), Owner->GetActorLocation()) < WhistleRadius)
			{
				++N;
			}
		}
	}
	return N;
}

int32 UPDWhistleComponent::Blow()
{
	IPDMatchFlow* Flow = PD::GetFlow(this);
	AActor* Owner = GetOwner();
	if (!IsReady() || !Flow || !Owner || Flow->GetMatchState() != EPDMatchState::Active)
	{
		return -1;
	}
	Cooldown = WhistleCooldown;
	int32 Scattered = 0;
	for (AActor* S : Flow->GetSkaters())
	{
		IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(S);
		if (Agent && FVector::Dist2D(S->GetActorLocation(), Owner->GetActorLocation()) < WhistleRadius)
		{
			Agent->Scatter(Owner->GetActorLocation(), WhistleScatterSeconds);
			++Scattered;
		}
	}
	Flow->ReportWhistle(Owner, Scattered);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f Whistle scattered %d skater(s)."), Flow->GetElapsed(), Scattered);
	return Scattered;
}

void UPDWhistleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Cooldown > 0.0f)
	{
		Cooldown -= DeltaTime;
		if (Cooldown <= 0.0f)
		{
			UPDAudioLibrary::PlaySynth2D(this, ReadySound);
		}
	}
	PromptTimer -= DeltaTime;
	if (PromptTimer <= 0.0f)
	{
		PromptTimer = 0.25f;
		IPDMatchFlow* Flow = PD::GetFlow(this);
		if (Flow && IsReady() && Flow->GetMatchState() == EPDMatchState::Active && CountInRange() >= PromptCrowd)
		{
			Flow->ReportPrompt(EPDPrompt::Whistle, GetOwner());
		}
	}
}
