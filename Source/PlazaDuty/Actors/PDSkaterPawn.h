// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PDInterfaces.h"
#include "PDSkaterPawn.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UDecalComponent;
class UMaterialInterface;
class UPDSkaterBrainComponent;

/**
 * A skater: a Rocketbox rider (MIT) standing sideways on a generated skateboard. The board is the root and
 * moves kinematically; the rider's feet stay on the deck. All decisions live in UPDSkaterBrainComponent.
 */
UCLASS()
class PLAZADUTY_API APDSkaterPawn : public APawn, public IPDSkaterAgent
{
	GENERATED_BODY()

public:
	APDSkaterPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override;

	// IPDSkaterAgent
	virtual void Scatter(const FVector& From, float Seconds) override;
	virtual AActor* GetTargetLine() const override;
	virtual bool IsCommitted() const override;
	virtual float GetSecondsToPoint(const FVector& Point, float Radius) const override;
	virtual void GoHome() override;
	virtual void PreferLine(AActor* Line) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> Root;
	/** Leans into turns and tips on ollies; the board and rider hang off it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Board;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USkeletalMeshComponent> Rider;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPDSkaterBrainComponent> Brain;
	/** Soft contact shadow under the board (stays on the ground; the board and rider ollie off it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UDecalComponent> ContactShadow;
	/** Solid body (user 2026-11-12: "characters going through player and other things"): the guard and walkers collide with it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<class UCapsuleComponent> Blocker;

	/** false = the skaters are on foot (runners): no board, the body faces travel and runs on the locomotion clips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bRideBoard = false;
	/** Body yaw relative to travel when on foot (the Rocketbox mesh faces +Y, so -90 faces +X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OnFootYaw = -90.0f;
	/** Contact shadow size when on foot (projection depth, radius, radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector OnFootShadowSize = FVector(60.0f, 42.0f, 42.0f);
	/** Deck top above the ground (the rider stands here). Measured from the board mesh at BeginPlay when bMeasureDeck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DeckHeight = 13.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bMeasureDeck = true;
	/** The CC-BY board model: scale to an 82 cm deck, turn its length onto +X (the direction of travel), lift the wheels onto the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoardScale = 0.66f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoardYaw = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BoardLift = 6.7f;
	/** Rider yaw relative to the board (sideways stance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StanceYaw = 0.0f;
	/** Ankle bone height above the shoe sole (the rider is lowered until the soles sit on the deck). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AnkleHeight = 9.4f;
	/** Seconds after spawn when the stance pose is measured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GroundingDelay = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName LeftFootBone = TEXT("Bip01-L-Foot");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName RightFootBone = TEXT("Bip01-R-Foot");
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> BoardMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<USkeletalMesh> DefaultRider;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> ContactShadowMaterial;
	/** Contact shadow decal: projection depth, half-length along the board, half-width (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector ContactShadowSize = FVector(60.0f, 30.0f, 56.0f);
	/** Pose the rider is measured in for grounding (the riding stance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName GroundingAction = TEXT("Stance");
	/** Seconds the grounding pose must have been held before it is measured (its blend-in is over). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GroundingSettleTime = 0.3f;
	/** Keep the lower foot on the deck every frame (pop, air, land, grind, bail), not only in the stance it was measured in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bContinuousGrounding = true;
	/** How fast the rider follows the measured foot height (1/s); high enough to track a pop, low enough to hide jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GroundingFollowRate = 22.0f;
	/** Also keep the midpoint of the feet over the middle of the deck (horizontal fit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bCentreFeetOnDeck = true;
	/** Horizontal follow rate (1/s): slower than the height so pose changes do not make the rider skid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CentreFollowRate = 10.0f;
	/** Riding life: the rider's body sways around its stance yaw (degrees, Hz) and leans with speed changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SwayYaw = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SwayRate = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SwayRoll = 3.0f;

	/** Set by the brain each tick (real displacement / dt). */
	FVector CurrentVelocity = FVector::ZeroVector;

private:
	float GroundingAge = 0.0f;
	float PoseHeld = 0.0f;
	/** Deck centre in the pivot's space (board mesh bounds origin). */
	FVector DeckCentre = FVector::ZeroVector;
	float SwayTime = 0.0f;
	float SwayPhase = 0.0f;
	bool bGrounded = false;
};
