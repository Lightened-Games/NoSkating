// Copyright Plaza Duty. All Rights Reserved.

#include "Player/PDGuardCharacter.h"
#include "Player/PDAnimInstance.h"
#include "Components/PDGuardComponents.h"
#include "Components/PDGameFeelComponent.h"
#include "Components/PDMusicComponent.h"
#include "Audio/PDSynth.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Framework/PDMatchEvents.h"
#include "PlazaDuty.h"

APDGuardCharacter::APDGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(PD::GuardTag);

	GetCapsuleComponent()->InitCapsuleSize(36.0f, 90.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	// Refresh bones even when not rendered, so foot grounding and the foot QA measure the real pose.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bEnableUpdateRateOptimizations = false;
	GetMesh()->SetReceivesDecals(false);
	GetMesh()->bCastContactShadow = true;

	ContactShadow = CreateDefaultSubobject<UDecalComponent>(TEXT("ContactShadow"));
	ContactShadow->SetupAttachment(GetCapsuleComponent());
	ContactShadow->SetUsingAbsoluteRotation(true);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, TurnRate, 0.0f);
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = Acceleration;
	Move->BrakingDecelerationWalking = BrakingDeceleration;
	Move->bUseRVOAvoidance = false;
	Move->bEnablePhysicsInteraction = true;
	Move->PushForceFactor = 120000.0f;
	Move->NavAgentProps.AgentRadius = 36.0f;
	Move->NavAgentProps.AgentHeight = 180.0f;
	Move->SetWalkableFloorAngle(50.0f);
	Move->MaxStepHeight = 32.0f;

	// Third-person camera: the arm follows the control rotation (mouse look); W moves where the camera looks.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 14.0f;
	CameraBoom->ProbeChannel = ECC_Camera;
	CameraBoom->bEnableCameraLag = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Interact = CreateDefaultSubobject<UPDInteractComponent>(TEXT("Interact"));
	BoltKit = CreateDefaultSubobject<UPDBoltComponent>(TEXT("Bolt"));
	Drag = CreateDefaultSubobject<UPDDragComponent>(TEXT("Drag"));
	WhistleKit = CreateDefaultSubobject<UPDWhistleComponent>(TEXT("Whistle"));
	Feel = CreateDefaultSubobject<UPDGameFeelComponent>(TEXT("GameFeel"));
	Music = CreateDefaultSubobject<UPDMusicComponent>(TEXT("Music"));

	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Characters/Rocketbox/Security_Male_01/SK_Security_Male_01.SK_Security_Male_01")));
	HiddenSlots = {};
	HiddenMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/M_PD_Hidden.M_PD_Hidden")));
	ContactShadowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_Decal_Contact.MI_PD_Decal_Contact")));
}

void APDGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
	SafeLocation = GetActorLocation();
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* M = BodyMesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(M);
		}
	}
	GetMesh()->SetAnimInstanceClass(UPDAnimInstance::StaticClass());
	if (UMaterialInterface* Hidden = HiddenMaterial.LoadSynchronous())
	{
		const TArray<FName> Slots = GetMesh()->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			for (const FName& H : HiddenSlots)
			{
				if (Slots[i].ToString().Contains(H.ToString()))
				{
					GetMesh()->SetMaterial(i, Hidden);
				}
			}
		}
	}
	if (UMaterialInterface* M = ContactShadowMaterial.LoadSynchronous())
	{
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		ContactShadow->SetDecalMaterial(M);
		ContactShadow->DecalSize = FVector(ContactShadowDepth, ContactShadowRadius, ContactShadowRadius);
		ContactShadow->SetRelativeLocation(FVector(0.0f, 0.0f, -Half));
		ContactShadow->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->TargetOffset = CameraTargetOffset;
	CameraBoom->SocketOffset = CameraSocketOffset;
	CameraBoom->CameraLagSpeed = CameraLag;
	Camera->SetFieldOfView(CameraFOV);
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnRate, 0.0f);
}

EPDGuardAction APDGuardCharacter::GetGuardAction() const
{
	if (KnockdownLeft > 0.0f)
	{
		return EPDGuardAction::KnockedDown;
	}
	if (BoltKit && BoltKit->IsBolting())
	{
		return EPDGuardAction::Bolting;
	}
	if (Drag && Drag->IsDragging())
	{
		return EPDGuardAction::Dragging;
	}
	if (WhistleLeft > 0.0f)
	{
		return EPDGuardAction::Whistling;
	}
	return EPDGuardAction::None;
}

bool APDGuardCharacter::Knockdown(AActor* By)
{
	if (KnockdownLeft > 0.0f || GraceLeft > 0.0f)
	{
		return false;
	}
	IPDMatchFlow* Flow = PD::GetFlow(this);
	if (!Flow || Flow->GetMatchState() != EPDMatchState::Active)
	{
		return false;
	}
	KnockdownLeft = KnockdownSeconds;
	BoltKit->CancelBolt(true);
	Drag->Release();
	// Shoved along the skater's direction of travel.
	const FVector Push = By ? By->GetVelocity().GetSafeNormal2D() : -GetActorForwardVector();
	GetCharacterMovement()->StopMovementImmediately();
	LaunchCharacter(Push * KnockbackSpeed + FVector(0.0f, 0.0f, 120.0f), true, true);
	if (!Push.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, (-Push).Rotation().Yaw, 0.0f));
	}
	if (UPDAnimInstance* Anim = Cast<UPDAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Anim->SetAction(TEXT("Knocked"), true, 0.06f);
	}
	Flow->ReportKnockdown(this, By);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f Guard knocked down by %s."), Flow->GetElapsed(), *GetNameSafe(By));
	return true;
}

void APDGuardCharacter::PlayWhistleGesture()
{
	WhistleLeft = WhistleGestureSeconds;
	if (UPDAnimInstance* Anim = Cast<UPDAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		Anim->SetAction(TEXT("Whistle"), true, 0.08f);
	}
}

void APDGuardCharacter::UpdateAnimation()
{
	UPDAnimInstance* Anim = Cast<UPDAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Anim)
	{
		return;
	}
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const EPDMatchState State = Flow ? Flow->GetMatchState() : EPDMatchState::Active;
	// A held prop is gripped with both hands (arm IK over the walk); released at the end of the shift.
	FVector GripL = FVector::ZeroVector, GripR = FVector::ZeroVector;
	float Lean = 0.0f;
	const bool bGrip = State != EPDMatchState::Victory && State != EPDMatchState::Defeat
		&& GetGuardAction() == EPDGuardAction::Dragging && Drag->GetGrip(GripL, GripR, Lean);
	Anim->SetHandGrip(bGrip, GripL, GripR, Lean);
	if (!bGrip && State != EPDMatchState::Victory && State != EPDMatchState::Defeat)
	{
		// Round 3 hand work: the right hand works the knob while kneeling, two fingers of the left go to the lips for
		// the whistle while the right arm points the runners off (the clip alone pointed with no whistle).
		const EPDGuardAction Act = GetGuardAction();
		const IPDSkateLine* BoltLine = (Act == EPDGuardAction::Bolting && BoltKit) ? Cast<IPDSkateLine>(BoltKit->GetTarget()) : nullptr;
		Anim->SetArmGoal(1, BoltLine != nullptr, BoltLine ? BoltLine->GetKnobPoint() + FVector(0.0f, 0.0f, BoltLine->GetFeatureHeight() + KnobGripRaise) : FVector::ZeroVector);
		Anim->SetArmGoal(0, Act == EPDGuardAction::Whistling, FVector::ZeroVector, true);
	}
	if (State == EPDMatchState::Victory)
	{
		Anim->SetAction(TEXT("Victory"));
		return;
	}
	if (State == EPDMatchState::Defeat)
	{
		Anim->SetAction(TEXT("Defeat"));
		return;
	}
	switch (GetGuardAction())
	{
	case EPDGuardAction::KnockedDown:
		Anim->SetAction(TEXT("Knocked"));
		break;
	case EPDGuardAction::Bolting:
		Anim->SetAction(TEXT("Bolt"));
		break;
	case EPDGuardAction::Dragging:
	{
		// Both hands on the prop (arm IK, below); the legs keep the speed-matched walk.
		Anim->ClearAction();
		break;
	}
	case EPDGuardAction::Whistling:
		Anim->SetAction(TEXT("Whistle"));
		break;
	default:
		// Standing still for a while: he glances around the plaza (native look-around) instead of freezing.
		if (StillSeconds >= IdleGlanceAfterSeconds && IdleGlanceAfterSeconds > 0.0f)
		{
			Anim->SetAction(TEXT("LookAround"), false, 0.4f);
		}
		else
		{
			Anim->ClearAction(0.3f);
		}
		break;
	}
}

void APDGuardCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (KnockdownLeft > 0.0f)
	{
		KnockdownLeft -= DeltaSeconds;
		if (KnockdownLeft <= 0.0f)
		{
			GraceLeft = KnockdownGrace;
		}
	}
	else if (GraceLeft > 0.0f)
	{
		GraceLeft -= DeltaSeconds;
	}
	StillSeconds = GetVelocity().Size2D() < 5.0f ? StillSeconds + DeltaSeconds : 0.0f;
	if (WhistleLeft > 0.0f)
	{
		WhistleLeft -= DeltaSeconds;
	}
	if (const IPDGuardInput* Input = Cast<IPDGuardInput>(GetController()))
	{
		bRunning = Input->IsRunHeld();
	}
	if (!bWhistleBound)
	{
		if (IPDMatchFlow* Flow = PD::GetFlow(this))
		{
			if (UPDMatchEvents* Events = Flow->GetMatchEvents())
			{
				Events->OnWhistle.AddDynamic(this, &APDGuardCharacter::HandleWhistle);
				bWhistleBound = true;
			}
		}
	}
	GroundBody(DeltaSeconds);
	float Speed = bRunning ? RunSpeed : WalkSpeed;
	if (Drag->IsDragging())
	{
		Speed = WalkSpeed * Drag->DragSpeedFactor;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	UpdateAnimation();
}

void APDGuardCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (NewController)
	{
		NewController->SetControlRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
	}
}

void APDGuardCharacter::GroundBody(float DeltaSeconds)
{
	if (bBodyGrounded)
	{
		return;
	}
	GroundingAge += DeltaSeconds;
	USkeletalMeshComponent* Body = GetMesh();
	const UPDAnimInstance* Anim = Body ? Cast<UPDAnimInstance>(Body->GetAnimInstance()) : nullptr;
	// Measure only in the plain idle pose (no action layered, standing still) so the reference is the same every run.
	if (GroundingAge < GroundingDelay || !Body || !Anim || Body->GetBoneIndex(LeftFootBone) == INDEX_NONE || GetGuardAction() != EPDGuardAction::None
		|| !Anim->GetAction().IsNone() || GetVelocity().Size2D() > GroundingMaxSpeed)
	{
		return;
	}
	// Measured foot grounding (same as the skaters): put the ankles AnkleHeight above the capsule's floor.
	const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Feet = FMath::Min(Body->GetBoneLocation(LeftFootBone).Z, Body->GetBoneLocation(RightFootBone).Z);
	FVector Rel = Body->GetRelativeLocation();
	Rel.Z -= (Feet - Floor) - AnkleHeight;
	Body->SetRelativeLocation(Rel);
	bBodyGrounded = true;
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Guard body grounded: ankles were %.1f above the floor, mesh offset now %.1f."), Feet - Floor, Rel.Z);
}

void APDGuardCharacter::ReturnToSafeLocation()
{
	GetCharacterMovement()->StopMovementImmediately();
	TeleportTo(SafeLocation, GetActorRotation(), false, true);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	UPDAudioLibrary::PlaySynth2D(this, UPDAudioLibrary::Respawn());
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Guard left the plaza bounds: returned to the start."));
}

void APDGuardCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	// Never-fall-through: KillZ teleports back instead of destroying the pawn.
	Drag->Release();
	BoltKit->CancelBolt(false);
	ReturnToSafeLocation();
}

void APDGuardCharacter::HandleWhistle(AActor* InGuard, int32 Scattered)
{
	if (InGuard == this)
	{
		PlayWhistleGesture();
	}
}
