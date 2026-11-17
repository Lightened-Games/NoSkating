// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PDInterfaces.h"
#include "PDGuardCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPDInteractComponent;
class UPDBoltComponent;
class UPDDragComponent;
class UPDWhistleComponent;
class UPDGameFeelComponent;
class UPDMusicComponent;
class USkeletalMesh;
class UMaterialInterface;
class UDecalComponent;

/**
 * The plaza guard (Microsoft Rocketbox Police_Male_02, MIT). A high chase camera over the plaza, and the
 * guard's kit as components: interact focus, bolt, drag, whistle and game feel. A skater riding through
 * knocks the guard down (IPDKnockable): the bolt resets and a held prop drops.
 */
UCLASS()
class PLAZADUTY_API APDGuardCharacter : public ACharacter, public IPDKnockable
{
	GENERATED_BODY()

public:
	APDGuardCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	/** Starts the view behind the guard at CameraPitch / CameraYaw. */
	virtual void PossessedBy(AController* NewController) override;

	// IPDKnockable
	virtual bool Knockdown(AActor* By) override;
	virtual bool IsKnockedDown() const override { return KnockdownLeft > 0.0f; }
	virtual EPDGuardAction GetGuardAction() const override;

	/** Run (Shift) toggles the movement speed. Dragging overrides both. */
	void SetRunning(bool bRun) { bRunning = bRun; }
	bool IsRunning() const { return bRunning; }
	/** Plays the whistle gesture. */
	void PlayWhistleGesture();
	void ReturnToSafeLocation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDInteractComponent> Interact;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDBoltComponent> BoltKit;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDDragComponent> Drag;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDWhistleComponent> WhistleKit;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDGameFeelComponent> Feel;
	/** Music per state and ambient one-shots (v2 sound bed). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDMusicComponent> Music;
	/** Soft contact shadow on the floor under the feet (reads as grounded from the high camera at any sun angle). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UDecalComponent> ContactShadow;

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkSpeed = 340.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RunSpeed = 620.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Acceleration = 2400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BrakingDeceleration = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnRate = 720.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockdownSeconds = 1.6f;
	/** No second knock-down for this long after getting up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockdownGrace = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockbackSpeed = 520.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WhistleGestureSeconds = 0.7f;
	// Third-person camera: a spring arm behind the guard, turned by the mouse (control rotation).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraDistance = 330.0f;
	/** Starting pitch of the view (negative looks down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraPitch = -10.0f;
	/** Starting yaw of the view (0 = facing north, up the plaza toward the stairs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraYaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraFOV = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") float CameraLag = 14.0f;
	/** Pivot above the guard's capsule centre (shoulder height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") FVector CameraTargetOffset = FVector(0.0f, 0.0f, 90.0f);
	/** Over-the-shoulder offset at the camera end of the arm (Y = right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera") FVector CameraSocketOffset = FVector(0.0f, 50.0f, 25.0f);

	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<USkeletalMesh> BodyMesh;
	/** Ankle bone height above the shoe sole; the body is lowered/raised until it stands on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AnkleHeight = 9.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GroundingDelay = 0.3f;
	/** The bolting hand sits this far above the knob socket (cm): fingers on the bolt head, not inside it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnobGripRaise = 6.0f;
	/** Seconds standing still before the idle glance-around plays (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IdleGlanceAfterSeconds = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName LeftFootBone = TEXT("Bip01-L-Foot");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName RightFootBone = TEXT("Bip01-R-Foot");
	/** Material slots hidden on the guard (the police avatar's sidearm). */
	UPROPERTY(EditAnywhere, Category = "Look") TArray<FName> HiddenSlots;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> HiddenMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> ContactShadowMaterial;
	/** Contact shadow decal: projection depth, then the radius on the floor (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ContactShadowDepth = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ContactShadowRadius = 46.0f;
	/** Max ground speed at which the body is measured for foot grounding (the idle pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GroundingMaxSpeed = 10.0f;

private:
	void UpdateAnimation();
	UFUNCTION() void HandleWhistle(AActor* InGuard, int32 Scattered);

	FVector SafeLocation = FVector::ZeroVector;
	float KnockdownLeft = 0.0f;
	float GraceLeft = 0.0f;
	float WhistleLeft = 0.0f;
	float StillSeconds = 0.0f;
	bool bRunning = false;
	bool bWhistleBound = false;
	void GroundBody(float DeltaSeconds);
	float GroundingAge = 0.0f;
	bool bBodyGrounded = false;
};
