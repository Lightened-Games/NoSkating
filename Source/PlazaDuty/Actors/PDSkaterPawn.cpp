// Copyright Plaza Duty. All Rights Reserved.

#include "Actors/PDSkaterPawn.h"
#include "Components/PDSkaterBrainComponent.h"
#include "Player/PDAnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "PlazaDuty.h"

APDSkaterPawn::APDSkaterPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(PD::SkaterTag);
	// FactoryQA measures tagged non-Character pawns too (gait numbers and leg shots for the runners).
	Tags.Add(TEXT("FactoryQAProbe"));
	AutoPossessAI = EAutoPossessAI::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(Pivot);
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Board->SetCanEverAffectNavigation(false);

	Rider = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rider"));
	Rider->SetupAttachment(Pivot);
	Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rider->SetCanEverAffectNavigation(false);
	Rider->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Rider->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	Rider->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Rider->bEnableUpdateRateOptimizations = false;
	Rider->SetReceivesDecals(false);
	Rider->bCastContactShadow = true;
	Board->SetReceivesDecals(false);

	// On the root, not the pivot: it stays flat on the ground while the board tips and ollies.
	ContactShadow = CreateDefaultSubobject<UDecalComponent>(TEXT("ContactShadow"));
	ContactShadow->SetupAttachment(Root);
	ContactShadow->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->InitCapsuleSize(34.0f, 78.0f);
	Blocker->SetRelativeLocation(FVector(0.0f, 0.0f, 98.0f));  // bottom 20 cm up: steps over kerbs
	Blocker->SetCollisionProfileName(TEXT("Pawn"));
	Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Blocker->SetCanEverAffectNavigation(false);

	Brain = CreateDefaultSubobject<UPDSkaterBrainComponent>(TEXT("Brain"));

	// No default board: the skaters are runners (D24). Set BoardMesh with bRideBoard to bring boards back.
	ContactShadowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_Decal_Contact.MI_PD_Decal_Contact")));
	DefaultRider = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Characters/Rocketbox/Male_Adult_01/SK_Male_Adult_01.SK_Male_Adult_01")));
}

void APDSkaterPawn::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = BoardMesh.LoadSynchronous())
	{
		Board->SetStaticMesh(M);
		Board->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, BoardLift), FRotator(0.0f, BoardYaw, 0.0f));
		Board->SetRelativeScale3D(FVector(BoardScale));
		DeckCentre = Board->GetRelativeTransform().TransformPosition(M->GetBounds().Origin);
		if (bMeasureDeck)
		{
			// The deck is the board's top surface: stand the rider on the real mesh, not an assumed height.
			DeckHeight = BoardLift + M->GetBoundingBox().Max.Z * BoardScale;
		}
	}
	if (!Rider->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* M = DefaultRider.LoadSynchronous())
		{
			Rider->SetSkeletalMesh(M);
		}
	}
	// The Rocketbox mesh faces +Y; -90 faces the board's travel direction, StanceYaw turns the rider sideways.
	Rider->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, DeckHeight), FRotator(0.0f, -90.0f + StanceYaw + 90.0f, 0.0f));
	Rider->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	SwayPhase = FMath::FRand() * 2.0f * PI;
	if (!bRideBoard)
	{
		// On foot: no board; the native clips plant the feet at the root's height (ankles 10.6 cm = sole on the ground).
		Board->SetVisibility(false);
		Board->SetHiddenInGame(true);
		DeckHeight = 0.0f;
		Rider->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.0f, OnFootYaw, 0.0f));
		ContactShadowSize = OnFootShadowSize;
		bGrounded = true;
		SetActorTickEnabled(false);
	}
	if (UMaterialInterface* M = ContactShadowMaterial.LoadSynchronous())
	{
		ContactShadow->SetDecalMaterial(M);
		ContactShadow->DecalSize = ContactShadowSize;
	}
}

void APDSkaterPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bGrounded)
	{
		if (bContinuousGrounding && Rider->GetBoneIndex(LeftFootBone) != INDEX_NONE)
		{
			// Foot heights in the rider's own component space do not change when the rider moves, so this is no feedback loop.
			const FVector LCS = Rider->GetSocketTransform(LeftFootBone, RTS_Component).GetLocation();
			const FVector RCS = Rider->GetSocketTransform(RightFootBone, RTS_Component).GetLocation();
			const float FootCS = FMath::Min(LCS.Z, RCS.Z);
			FVector Rel = Rider->GetRelativeLocation();
			Rel.Z = FMath::FInterpTo(Rel.Z, DeckHeight - (FootCS - AnkleHeight), DeltaSeconds, GroundingFollowRate);
			if (bCentreFeetOnDeck)
			{
				// Feet midpoint in the pivot's axes (the rider is only yawed relative to it), placed over the deck centre.
				const FVector Mid = Rider->GetRelativeRotation().RotateVector(0.5f * (LCS + RCS));
				Rel.X = FMath::FInterpTo(Rel.X, DeckCentre.X - Mid.X, DeltaSeconds, CentreFollowRate);
				Rel.Y = FMath::FInterpTo(Rel.Y, DeckCentre.Y - Mid.Y, DeltaSeconds, CentreFollowRate);
			}
			Rider->SetRelativeLocation(Rel);
			// Body sway: turn and tilt a little around the stance, like balancing on a rolling board.
			SwayTime += DeltaSeconds;
			const float S = FMath::Sin(2.0f * PI * SwayRate * SwayTime + SwayPhase);
			Rider->SetRelativeRotation(FRotator(0.0f, StanceYaw + SwayYaw * S, SwayRoll * FMath::Sin(2.0f * PI * SwayRate * 1.7f * SwayTime + SwayPhase)));
		}
		return;
	}
	GroundingAge += DeltaSeconds;
	// Measure only once the riding stance has fully blended in (never mid-pop, in the air or bailing).
	const UPDAnimInstance* Anim = Cast<UPDAnimInstance>(Rider->GetAnimInstance());
	PoseHeld = (Anim && Anim->GetAction() == GroundingAction) ? PoseHeld + DeltaSeconds : 0.0f;
	if (GroundingAge < GroundingDelay || PoseHeld < GroundingSettleTime || Rider->GetBoneIndex(LeftFootBone) == INDEX_NONE)
	{
		return;
	}
	// Measured foot grounding: the Rocketbox bodies imported onto the shared skeleton sit high in the
	// retargeted pose, so lower the rider until the ankles are AnkleHeight above the deck top.
	const float RiderZ = Rider->GetComponentLocation().Z;
	const float Feet = FMath::Min(Rider->GetBoneLocation(LeftFootBone).Z, Rider->GetBoneLocation(RightFootBone).Z);
	const float Correction = (Feet - RiderZ) - AnkleHeight;
	FVector Rel = Rider->GetRelativeLocation();
	Rel.Z = DeckHeight - Correction;
	Rider->SetRelativeLocation(Rel);
	bGrounded = true;
	SetActorTickEnabled(bContinuousGrounding);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] %s rider grounded: deck top %.1f, ankles were %.1f above the mesh origin, rider offset now %.1f."), *GetName(), DeckHeight, Feet - RiderZ, Rel.Z);
}

FVector APDSkaterPawn::GetVelocity() const
{
	return CurrentVelocity;
}

void APDSkaterPawn::Scatter(const FVector& From, float Seconds)
{
	Brain->Scatter(From, Seconds);
}

AActor* APDSkaterPawn::GetTargetLine() const
{
	return Brain->GetLine();
}

bool APDSkaterPawn::IsCommitted() const
{
	return Brain->IsCommitted();
}

float APDSkaterPawn::GetSecondsToPoint(const FVector& Point, float Radius) const
{
	return Brain->GetSecondsToPoint(Point, Radius);
}

void APDSkaterPawn::PreferLine(AActor* InLine)
{
	Brain->PreferLine(InLine);
}

void APDSkaterPawn::GoHome()
{
	Brain->GoHome();
}
