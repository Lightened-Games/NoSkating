// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PDAmbientPerson.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UDecalComponent;
class UMaterialInterface;

/**
 * Life pass (design guide 2.5.5): a passer-by on the edge pavements or someone sitting on a bench. Not part of the
 * game rules. Walkers follow a loop of points at a speed matched to the native fast-walk clip, pause and look around,
 * and stop to face the guard when the whistle blows nearby (a reactive world). Sitters play a seated loop.
 * Solid (C2): the capsule blocks the guard. Movement uses the same native-clip AnimInstance as the guard (C3).
 */
UCLASS()
class PLAZADUTY_API APDAmbientPerson : public APawn
{
	GENERATED_BODY()

public:
	APDAmbientPerson();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override { return CurrentVelocity; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCapsuleComponent> Capsule;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UDecalComponent> ContactShadow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") TSoftObjectPtr<USkeletalMesh> BodyMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") TSoftObjectPtr<UMaterialInterface> ContactShadowMaterial;

	/** World points walked in a loop. Empty = stand or sit in place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TArray<FVector> Waypoints;
	/** Walking speed (cm/s). 140 plays the native fast walk (measured 149 cm/s) at 0.94x. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkSpeed = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnRateDegrees = 240.0f;
	/** Seconds paused at each waypoint (random in range), playing PauseAction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D PauseSeconds = FVector2D(3.0f, 6.0f);  // a real stop at each end: looking around, waiting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName PauseAction = TEXT("LookAround");
	/** Seated or standing loop played while not walking (None = plain idle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName StationaryAction = NAME_None;
	/** The whistle within this radius makes a walker stop and face the guard for ReactSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReactRadius = 2600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReactSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleRadius = 34.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleHalfHeight = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector ContactShadowSize = FVector(60.0f, 44.0f, 44.0f);
	/** Seated people: the capsule shrinks so it does not hang through the bench (the bench itself is solid). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SeatedCapsuleHalfHeight = 50.0f;
	/** The capsule floats this far above the floor (the mesh is lowered by the same amount): a capsule resting exactly on
	 *  the floor makes every horizontal sweep report the floor as a blocking hit, and the walker never moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FloorClearance = 3.0f;

private:
	UFUNCTION() void HandleWhistle(AActor* Guard, int32 Scattered);
	void SetAction(FName Action);

	FVector CurrentVelocity = FVector::ZeroVector;
	int32 Target = 0;
	float PauseLeft = 0.0f;
	float ReactLeft = 0.0f;
	FVector ReactFocus = FVector::ZeroVector;
	FName CurrentAction = NAME_None;
	bool bBound = false;
	float BlockedFor = 0.0f;
	bool bLoggedBlock = false;
};
