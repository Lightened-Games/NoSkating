// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/PDInterfaces.h"
#include "PDPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UPDHUDWidget;
class UPDAutoplayComponent;
struct FInputActionValue;

/**
 * Guard controls (Enhanced Input, actions built at runtime, no template Input assets):
 * WASD move, Shift run, E bolt / drag, Q whistle, R restart. Creates the HUD on possess and owns the bot.
 * Every control goes through IPDGuardInput so the bot drives exactly the same entry points.
 */
UCLASS()
class PLAZADUTY_API APDPlayerController : public APlayerController, public IPDGuardInput
{
	GENERATED_BODY()

public:
	APDPlayerController();

	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// IPDGuardInput
	virtual void InputMove(const FVector& WorldDirection, float Scale, bool bSimulated) override;
	virtual void InputRun(bool bRun, bool bSimulated) override;
	virtual void InputInteractPressed(bool bSimulated) override;
	virtual void InputInteractReleased(bool bSimulated) override;
	virtual void InputWhistle(bool bSimulated) override;
	virtual bool IsRunHeld() const override { return bRunHeld; }
	virtual FPDInputVisualized& GetInputVisualized() override { return OnInputVisualized; }

	UPROPERTY(BlueprintAssignable, Category = "PD|Input") FPDInputVisualized OnInputVisualized;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDAutoplayComponent> Autoplay;
	UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UPDHUDWidget> HUDWidgetClass;

	/** Seconds between legend flashes while a movement key is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MoveFlashInterval = 0.2f;
	/** Moving harder than this while kneeling cancels the bolt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CancelBoltInput = 0.5f;
	/** Mouse look: degrees per mouse count, and the view pitch range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float LookSensitivity = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float MinViewPitch = -60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float MaxViewPitch = 15.0f;
	/** Autoplay: the view swings in behind the walking guard at this rate (and eases back to this pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float BotCameraFollowRate = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float BotCameraPitch = -10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float BotCameraMinSpeed = 120.0f;

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	void BuildInput();
	void OnMove(const FInputActionValue& Value);
	void OnRunStarted(const FInputActionValue& Value);
	void OnRunCompleted(const FInputActionValue& Value);
	void OnInteractStarted(const FInputActionValue& Value);
	void OnInteractCompleted(const FInputActionValue& Value);
	void OnWhistle(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	bool IsBotDriving() const;
	bool IsActive() const;
	void Flash(FName Key, bool bSimulated);

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	void OnLook(const FInputActionValue& Value);
	UPROPERTY() TObjectPtr<UInputAction> MenuAction;
	void OnMenu(const FInputActionValue& Value);
	UPROPERTY() TObjectPtr<UInputAction> RunAction;
	UPROPERTY() TObjectPtr<UInputAction> InteractAction;
	UPROPERTY() TObjectPtr<UInputAction> WhistleAction;
	UPROPERTY() TObjectPtr<UInputAction> RestartAction;
	UPROPERTY() TObjectPtr<UPDHUDWidget> HUDWidget;

	float MoveFlashTimer = 0.0f;
	float LookFlashTimer = 0.0f;
	bool bRunHeld = false;
};
