// Copyright Plaza Duty. All Rights Reserved.

#include "Actors/PDPropActor.h"
#include "Data/PDMatchConfig.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "CollisionQueryParams.h"

#define LOCTEXT_NAMESPACE "PlazaDuty"

APDPropActor::APDPropActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(PD::PropTag);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionProfileName(TEXT("PhysicsActor"));
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetSimulatePhysics(true);
	Body->SetLinearDamping(0.6f);
	Body->SetAngularDamping(1.2f);
	// Solid props carve the navmesh so NPCs and the bot path around them (C2); off only while held.
	Body->SetCanEverAffectNavigation(true);
	Body->BodyInstance.bUseCCD = true;

	FocusRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FocusRing"));
	FocusRing->SetupAttachment(Body);
	FocusRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FocusRing->SetCastShadow(false);
	FocusRing->SetCanEverAffectNavigation(false);
	FocusRing->SetUsingAbsoluteRotation(true);
	FocusRing->SetVisibility(false);

	RingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Meshes/SM_PD_Ring.SM_PD_Ring")));
	FocusMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_AccentGlow.MI_PD_AccentGlow")));
	ConvertedMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_DangerGlow.MI_PD_DangerGlow")));
}

void APDPropActor::BeginPlay()
{
	Super::BeginPlay();
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	if (UStaticMesh* Ring = RingMesh.LoadSynchronous())
	{
		FocusRing->SetStaticMesh(Ring);
	}
	const FBoxSphereBounds B = Body->GetStaticMesh() ? Body->GetStaticMesh()->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(30.0f), 30.0f);
	BottomOffset = -(B.Origin.Z - B.BoxExtent.Z) * GetActorScale3D().Z;
	const float RingRadius = GetFootprintRadius() + RingPadding;
	const float RingMeshRadius = RingMesh.Get() ? FMath::Max(1.0f, RingMesh.Get()->GetBounds().BoxExtent.X) : 50.0f;
	FocusRing->SetWorldScale3D(FVector(RingRadius / RingMeshRadius, RingRadius / RingMeshRadius, 1.0f));
	LastLocation = GetActorLocation();
	UpdateRing();
}

float APDPropActor::GetFootprintRadius() const
{
	if (!Body || !Body->GetStaticMesh())
	{
		return 30.0f;
	}
	const FVector E = Body->GetStaticMesh()->GetBounds().BoxExtent * GetActorScale3D();
	return FMath::Max(E.X, E.Y);
}

bool APDPropActor::CanInteract(const AActor* Guard) const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	return !bHeld && Flow && Flow->GetMatchState() == EPDMatchState::Active;
}

FVector APDPropActor::GetInteractPoint() const
{
	return GetActorLocation();
}

FText APDPropActor::GetInteractLabel() const
{
	return LOCTEXT("DragLabel", "E  DRAG");
}

void APDPropActor::BeginDrag(AActor* Guard)
{
	bHeld = true;
	RestSeconds = 0.0f;
	Body->SetSimulatePhysics(false);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCanEverAffectNavigation(false);
	// Stand it upright while it is dragged.
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
	UpdateRing();
}

void APDPropActor::DragTo(const FVector& Target, float Yaw)
{
	if (!bHeld)
	{
		return;
	}
	// Find the floor under the target so the prop follows steps, the bank and the deck.
	FVector Goal = Target;
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PDPropFloor), false, this);
	TArray<AActor*> Ignore;
	GetAttachedActors(Ignore);
	Params.AddIgnoredActors(Ignore);
	if (const AActor* Holder = GetOwner())
	{
		Params.AddIgnoredActor(Holder);
	}
	if (GetWorld()->LineTraceSingleByChannel(Floor, Target + FVector(0.0f, 0.0f, GroundProbe), Target - FVector(0.0f, 0.0f, GroundProbe), ECC_WorldStatic, Params))
	{
		Goal.Z = Floor.ImpactPoint.Z + BottomOffset + DragLift + CarryLift;
	}
	else
	{
		Goal.Z = GetActorLocation().Z;
	}
	const FRotator NewRot(0.0f, FMath::FixedTurn(GetActorRotation().Yaw, Yaw, 6.0f), 0.0f);
	// Lift first (steps), then slide swept so the prop stops at walls and ledges.
	FVector Here = GetActorLocation();
	if (Goal.Z > Here.Z)
	{
		SetActorLocation(FVector(Here.X, Here.Y, Goal.Z), true);
		Here = GetActorLocation();
	}
	SetActorLocationAndRotation(FVector(Goal.X, Goal.Y, Here.Z), NewRot, true);
	if (Goal.Z < GetActorLocation().Z)
	{
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Goal.Z), true);
	}
}

void APDPropActor::EndDrag()
{
	if (!bHeld)
	{
		return;
	}
	bHeld = false;
	CarryLift = 0.0f;
	SetOwner(nullptr);
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Body->SetCanEverAffectNavigation(true);
	Body->SetSimulatePhysics(true);
	Body->SetPhysicsLinearVelocity(FVector(0.0f, 0.0f, -50.0f));
	Body->WakeAllRigidBodies();
	UpdateRing();
}

void APDPropActor::SetConverted(bool bInConverted, const FText& InTrickName)
{
	bConverted = bInConverted;
	TrickName = InTrickName;
	UpdateRing();
}

void APDPropActor::UpdateRing()
{
	const bool bShow = bIsFocused || bHeld || bConverted;
	FocusRing->SetVisibility(bShow);
	UMaterialInterface* Mat = (bConverted && !bHeld) ? ConvertedMaterial.LoadSynchronous() : FocusMaterial.LoadSynchronous();
	if (Mat && FocusRing->GetMaterial(0) != Mat)
	{
		FocusRing->SetMaterial(0, Mat);
	}
}

void APDPropActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector Here = GetActorLocation();
	if (bHeld || FVector::Dist(Here, LastLocation) > RestTolerance)
	{
		RestSeconds = 0.0f;
	}
	else
	{
		RestSeconds += DeltaSeconds;
	}
	LastLocation = Here;
	// The ring lies flat on the ground under the prop whatever the prop's tilt.
	FocusRing->SetWorldLocation(Here - FVector(0.0f, 0.0f, BottomOffset + CarryLift - 2.0f));
	FocusRing->SetWorldRotation(FRotator::ZeroRotator);
	UpdateRing();
}

#undef LOCTEXT_NAMESPACE
