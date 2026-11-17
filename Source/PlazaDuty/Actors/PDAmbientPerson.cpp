// Copyright Plaza Duty. All Rights Reserved.

#include "Actors/PDAmbientPerson.h"
#include "Player/PDAnimInstance.h"
#include "Framework/PDMatchEvents.h"
#include "Interfaces/PDInterfaces.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PlazaDuty.h"

APDAmbientPerson::APDAmbientPerson()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	Tags.Add(TEXT("PD.Ambient"));
	Tags.Add(TEXT("FactoryQAProbe"));  // FactoryQA measures their gait too

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCanEverAffectNavigation(false);
	RootComponent = Capsule;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -CapsuleHalfHeight), FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetReceivesDecals(false);

	ContactShadow = CreateDefaultSubobject<UDecalComponent>(TEXT("ContactShadow"));
	ContactShadow->SetupAttachment(Capsule);
	ContactShadow->SetUsingAbsoluteRotation(true);
	ContactShadowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_Decal_Contact.MI_PD_Decal_Contact")));
}

void APDAmbientPerson::BeginPlay()
{
	Super::BeginPlay();
	const bool bSeated = Waypoints.Num() == 0 && StationaryAction.ToString().StartsWith(TEXT("Sit"));
	const float Half = (bSeated ? SeatedCapsuleHalfHeight : CapsuleHalfHeight) + FloorClearance;
	// Keep the feet where they were placed: the capsule is centred Half above the floor point the layout gave.
	const FVector Floor = GetActorLocation();
	Capsule->SetCapsuleSize(CapsuleRadius, Half - FloorClearance);
	SetActorLocation(Floor + FVector(0.0f, 0.0f, Half));
	if (USkeletalMesh* M = BodyMesh.LoadSynchronous())
	{
		Body->SetSkeletalMesh(M);
	}
	// The native clips plant the soles at the root (ankles 10.6 cm = the measured idle; no per-avatar offset).
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -Half));
	Body->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	if (UMaterialInterface* Mat = ContactShadowMaterial.LoadSynchronous())
	{
		ContactShadow->SetDecalMaterial(Mat);
		ContactShadow->DecalSize = ContactShadowSize;
		ContactShadow->SetRelativeLocation(FVector(0.0f, 0.0f, -Half));
		ContactShadow->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}
	if (Waypoints.Num() > 0)
	{
		// Start at a random point of the loop so the edges are busy from the first frame.
		Target = FMath::RandRange(0, Waypoints.Num() - 1);
		SetActorLocation(FVector(Waypoints[Target].X, Waypoints[Target].Y, Waypoints[Target].Z + Half));
		Target = (Target + 1) % Waypoints.Num();
	}
	SetAction(StationaryAction);
}

void APDAmbientPerson::SetAction(FName Action)
{
	if (Action == CurrentAction)
	{
		return;
	}
	CurrentAction = Action;
	if (UPDAnimInstance* Anim = Cast<UPDAnimInstance>(Body->GetAnimInstance()))
	{
		if (Action.IsNone())
		{
			Anim->ClearAction(0.25f);
		}
		else
		{
			Anim->SetAction(Action, false, 0.25f);
		}
	}
}

void APDAmbientPerson::HandleWhistle(AActor* Guard, int32 Scattered)
{
	if (Guard && Waypoints.Num() > 0 && FVector::Dist2D(Guard->GetActorLocation(), GetActorLocation()) < ReactRadius)
	{
		ReactLeft = ReactSeconds;
		ReactFocus = Guard->GetActorLocation();
	}
}

void APDAmbientPerson::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBound)
	{
		if (IPDMatchFlow* Flow = PD::GetFlow(this))
		{
			if (UPDMatchEvents* Events = Flow->GetMatchEvents())
			{
				Events->OnWhistle.AddDynamic(this, &APDAmbientPerson::HandleWhistle);
				bBound = true;
			}
		}
	}
	CurrentVelocity = FVector::ZeroVector;
	if (Waypoints.Num() == 0)
	{
		return;
	}
	auto TurnTo = [this, DeltaSeconds](const FVector& Dir)
	{
		const float Yaw = Dir.Rotation().Yaw;
		SetActorRotation(FRotator(0.0f, FMath::FixedTurn(GetActorRotation().Yaw, Yaw, TurnRateDegrees * DeltaSeconds), 0.0f));
	};
	if (ReactLeft > 0.0f)
	{
		// The whistle: stop and look at the guard.
		ReactLeft -= DeltaSeconds;
		SetAction(NAME_None);
		TurnTo((ReactFocus - GetActorLocation()).GetSafeNormal2D());
		return;
	}
	if (PauseLeft > 0.0f)
	{
		PauseLeft -= DeltaSeconds;
		SetAction(PauseAction);
		return;
	}
	SetAction(NAME_None);
	const FVector Here = GetActorLocation();
	const FVector Goal(Waypoints[Target].X, Waypoints[Target].Y, Here.Z);
	const FVector To = Goal - Here;
	const float Dist = To.Size2D();
	if (Dist < 20.0f)
	{
		Target = (Target + 1) % Waypoints.Num();
		PauseLeft = FMath::FRandRange(PauseSeconds.X, PauseSeconds.Y);
		return;
	}
	const FVector Dir = To.GetSafeNormal2D();
	TurnTo(Dir);
	// Walk where the body faces (no sideways slide while turning), slower in tight turns.
	const FVector Fwd = GetActorForwardVector();
	const float Align = FMath::Clamp(FVector::DotProduct(Fwd, Dir), 0.2f, 1.0f);
	const float Step = FMath::Min(Dist, WalkSpeed * Align * DeltaSeconds);
	FHitResult Hit;
	SetActorLocation(Here + Fwd * Step, true, &Hit);
	CurrentVelocity = DeltaSeconds > 0.0f ? (GetActorLocation() - Here) / DeltaSeconds : FVector::ZeroVector;
	// Blocked (the guard in the way, or a prop): wait, then turn back rather than freeze against it.
	BlockedFor = (Hit.bBlockingHit && CurrentVelocity.Size2D() < WalkSpeed * 0.2f) ? BlockedFor + DeltaSeconds : 0.0f;
	if (BlockedFor > 2.0f)
	{
		if (!bLoggedBlock)
		{
			bLoggedBlock = true;
			UE_LOG(LogPlaza, Warning, TEXT("[Plaza] %s blocked by %s at %s: turning back."), *GetName(), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"), *GetActorLocation().ToCompactString());
		}
		BlockedFor = 0.0f;
		Target = (Target + Waypoints.Num() - 1) % Waypoints.Num();
		PauseLeft = 1.0f;
	}
}
