// Copyright Plaza Duty. All Rights Reserved.

#include "Framework/PDPlayerController.h"
#include "Framework/PDAutoplayComponent.h"
#include "Components/PDGuardComponents.h"
#include "UI/PDHUDWidget.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "PlazaDuty.h"

APDPlayerController::APDPlayerController()
{
	Autoplay = CreateDefaultSubobject<UPDAutoplayComponent>(TEXT("Autoplay"));
	HUDWidgetClass = UPDHUDWidget::StaticClass();
}

void APDPlayerController::BuildInput()
{
	if (Mapping)
	{
		return;
	}
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_PlazaDuty"));
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};

	// WASD -> 2D axis (X = right, Y = forward).
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	{
		FEnhancedActionKeyMapping& W = Mapping->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* SwizzleW = NewObject<UInputModifierSwizzleAxis>(this);
		SwizzleW->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(SwizzleW);
		FEnhancedActionKeyMapping& S = Mapping->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* SwizzleS = NewObject<UInputModifierSwizzleAxis>(this);
		SwizzleS->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(SwizzleS);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		Mapping->MapKey(MoveAction, EKeys::D);
		FEnhancedActionKeyMapping& A = Mapping->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	RunAction = MakeAction(TEXT("IA_Run"), EInputActionValueType::Boolean);
	Mapping->MapKey(RunAction, EKeys::LeftShift);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	Mapping->MapKey(InteractAction, EKeys::E);
	WhistleAction = MakeAction(TEXT("IA_Whistle"), EInputActionValueType::Boolean);
	Mapping->MapKey(WhistleAction, EKeys::Q);
	RestartAction = MakeAction(TEXT("IA_Restart"), EInputActionValueType::Boolean);
	Mapping->MapKey(RestartAction, EKeys::R);
	MenuAction = MakeAction(TEXT("IA_Menu"), EInputActionValueType::Boolean);
	Mapping->MapKey(MenuAction, EKeys::Escape);
}

void APDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->AddMappingContext(Mapping, 0);
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APDPlayerController::OnMove);
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APDPlayerController::OnLook);
		EIC->BindAction(RunAction, ETriggerEvent::Started, this, &APDPlayerController::OnRunStarted);
		EIC->BindAction(RunAction, ETriggerEvent::Completed, this, &APDPlayerController::OnRunCompleted);
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APDPlayerController::OnInteractStarted);
		EIC->BindAction(InteractAction, ETriggerEvent::Completed, this, &APDPlayerController::OnInteractCompleted);
		EIC->BindAction(WhistleAction, ETriggerEvent::Started, this, &APDPlayerController::OnWhistle);
		EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &APDPlayerController::OnRestart);
		EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &APDPlayerController::OnMenu);
	}
}

void APDPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = MinViewPitch;
		PlayerCameraManager->ViewPitchMax = MaxViewPitch;
	}

	if (!HUDWidget && IsLocalController() && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UPDHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] HUD created and added to the viewport."));
		}
	}
}

bool APDPlayerController::IsBotDriving() const
{
	// Take-over (FactoryKit): any real input hands the guard to the player until they go idle again.
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	return Autoplay && Autoplay->IsBotActive() && (!Flow || Flow->IsBotDriving());
}

void APDPlayerController::OnMenu(const FInputActionValue& Value)
{
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		if (Flow->GetState() == EFactoryFlowState::Playing || Flow->GetState() == EFactoryFlowState::Ending)
		{
			Flow->ReturnToMenu();
		}
	}
}

bool APDPlayerController::IsActive() const
{
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	return Flow && Flow->GetMatchState() == EPDMatchState::Active;
}

void APDPlayerController::Flash(FName Key, bool bSimulated)
{
	OnInputVisualized.Broadcast(Key, bSimulated);
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->PulseLegend(Key);  // the bot's inputs light the legend too: the video's visible input
	}
}

void APDPlayerController::OnMove(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	// Camera-relative: W walks where the camera looks.
	const FRotator Yaw(0.0f, PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Yaw : 0.0f, 0.0f);
	const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Axis.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Axis.X;
	InputMove(Dir, FMath::Min(1.0f, Axis.Size()), false);
}

void APDPlayerController::OnLook(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	FVector2D Delta = Value.Get<FVector2D>() * LookSensitivity * UFactoryFlowSubsystem::GetMouseSensitivity();
	if (UFactoryFlowSubsystem::GetInvertY())
	{
		Delta.Y = -Delta.Y;
	}
	FRotator R = GetControlRotation();
	R.Yaw += Delta.X;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + Delta.Y), MinViewPitch, MaxViewPitch);
	R.Roll = 0.0f;
	SetControlRotation(R);
	LookFlashTimer -= GetWorld()->GetDeltaSeconds();
	if (Delta.SizeSquared() > 0.25f && LookFlashTimer <= 0.0f)
	{
		LookFlashTimer = MoveFlashInterval;
		Flash(PD::KeyLook, false);
	}
}

void APDPlayerController::InputMove(const FVector& WorldDirection, float Scale, bool bSimulated)
{
	APawn* P = GetPawn();
	if (!P || Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const IPDKnockable* Guard = Cast<IPDKnockable>(P);
	const EPDGuardAction Action = Guard ? Guard->GetGuardAction() : EPDGuardAction::None;
	if (Action == EPDGuardAction::KnockedDown)
	{
		return;
	}
	if (Action == EPDGuardAction::Bolting)
	{
		// Walking away from the knob cancels the bolt.
		if (Scale >= CancelBoltInput)
		{
			if (UPDBoltComponent* Bolt = P->FindComponentByClass<UPDBoltComponent>())
			{
				Bolt->CancelBolt(false);
			}
		}
		return;
	}
	if (IsMoveInputIgnored())
	{
		return;
	}
	P->AddMovementInput(FVector(WorldDirection.X, WorldDirection.Y, 0.0f).GetSafeNormal(), Scale);
	MoveFlashTimer -= GetWorld()->GetDeltaSeconds();
	if (MoveFlashTimer <= 0.0f)
	{
		MoveFlashTimer = MoveFlashInterval;
		Flash(PD::KeyMove, bSimulated);
	}
}

void APDPlayerController::OnRunStarted(const FInputActionValue& Value)
{
	if (!IsBotDriving())
	{
		InputRun(true, false);
	}
}

void APDPlayerController::OnRunCompleted(const FInputActionValue& Value)
{
	if (!IsBotDriving())
	{
		InputRun(false, false);
	}
}

void APDPlayerController::InputRun(bool bRun, bool bSimulated)
{
	if (bRun && !bRunHeld)
	{
		Flash(PD::KeyRun, bSimulated);
	}
	bRunHeld = bRun;
}

void APDPlayerController::OnInteractStarted(const FInputActionValue& Value)
{
	if (!IsBotDriving())
	{
		InputInteractPressed(false);
	}
}

void APDPlayerController::OnInteractCompleted(const FInputActionValue& Value)
{
	if (!IsBotDriving())
	{
		InputInteractReleased(false);
	}
}

void APDPlayerController::InputInteractPressed(bool bSimulated)
{
	APawn* P = GetPawn();
	if (!P || !IsActive())
	{
		return;
	}
	const IPDKnockable* Guard = Cast<IPDKnockable>(P);
	if (Guard && Guard->IsKnockedDown())
	{
		return;
	}
	UPDDragComponent* Drag = P->FindComponentByClass<UPDDragComponent>();
	UPDBoltComponent* Bolt = P->FindComponentByClass<UPDBoltComponent>();
	UPDInteractComponent* Interact = P->FindComponentByClass<UPDInteractComponent>();
	Flash(PD::KeyInteract, bSimulated);
	if (Drag && Drag->IsDragging())
	{
		Drag->Release();
		return;
	}
	AActor* Focus = Interact ? Interact->GetFocus() : nullptr;
	const IPDInteractable* I = Cast<IPDInteractable>(Focus);
	if (!I)
	{
		return;
	}
	if (I->GetInteractKind() == EPDInteractKind::Drag && Drag)
	{
		Drag->Grab(Focus);
	}
	else if (I->GetInteractKind() == EPDInteractKind::Bolt && Bolt)
	{
		Bolt->StartBolt(Focus);
	}
}

void APDPlayerController::InputInteractReleased(bool bSimulated)
{
	APawn* P = GetPawn();
	if (UPDBoltComponent* Bolt = P ? P->FindComponentByClass<UPDBoltComponent>() : nullptr)
	{
		Bolt->CancelBolt(false);
	}
}

void APDPlayerController::OnWhistle(const FInputActionValue& Value)
{
	if (!IsBotDriving())
	{
		InputWhistle(false);
	}
}

void APDPlayerController::InputWhistle(bool bSimulated)
{
	APawn* P = GetPawn();
	const IPDKnockable* Guard = Cast<IPDKnockable>(P);
	if (!P || !IsActive() || (Guard && Guard->IsKnockedDown()))
	{
		return;
	}
	Flash(PD::KeyWhistle, bSimulated);
	if (UPDWhistleComponent* Whistle = P->FindComponentByClass<UPDWhistleComponent>())
	{
		Whistle->Blow();
	}
}

void APDPlayerController::OnRestart(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		// Unattended runs ignore stray key presses; the bot restarts on its own.
		return;
	}
	Flash(PD::KeyRestart, false);
	if (IPDMatchFlow* Flow = PD::GetFlow(this))
	{
		Flow->RequestRestart();
	}
}

void APDPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// Autoplay has no mouse: swing the view in behind the guard as it walks, like a player would.
	const APawn* P = GetPawn();
	if (IsBotDriving() && P)
	{
		FRotator R = GetControlRotation();
		const FVector V = P->GetVelocity();
		if (V.Size2D() > BotCameraMinSpeed)
		{
			R.Yaw = FMath::FInterpTo(R.Yaw, R.Yaw + FRotator::NormalizeAxis(V.Rotation().Yaw - R.Yaw), DeltaTime, BotCameraFollowRate);
		}
		else
		{
			// Standing to bolt a knob or pick up a prop: settle in behind him so his body hides the hand work
			// (user 2026-11-17: "hide the hands of the character when doing something").
			const float Facing = P->GetActorRotation().Yaw;
			R.Yaw = FMath::FInterpTo(R.Yaw, R.Yaw + FRotator::NormalizeAxis(Facing - R.Yaw), DeltaTime, BotCameraFollowRate);
		}
		R.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(R.Pitch), BotCameraPitch, DeltaTime, BotCameraFollowRate);
		R.Roll = 0.0f;
		SetControlRotation(R);
	}
}
