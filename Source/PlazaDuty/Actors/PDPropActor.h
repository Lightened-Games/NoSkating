// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PDInterfaces.h"
#include "PDPropActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Loose plaza furniture (cone, bench, planter, trash can). Physics-simulated at rest; the guard drags it
 * (kinematic, swept so it stops at walls). Skate lines track it and may turn it into a trick.
 */
UCLASS()
class PLAZADUTY_API APDPropActor : public AActor, public IPDDraggable, public IPDInteractable
{
	GENERATED_BODY()

public:
	APDPropActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IPDDraggable
	virtual EPDPropKind GetPropKind() const override { return Kind; }
	virtual bool IsHeld() const override { return bHeld; }
	virtual void BeginDrag(AActor* Guard) override;
	virtual void SetCarryLift(float Cm) override { CarryLift = FMath::Max(0.0f, Cm); }
	virtual void DragTo(const FVector& Target, float Yaw) override;
	virtual void EndDrag() override;
	virtual void SetConverted(bool bInConverted, const FText& InTrickName) override;
	virtual bool IsConverted() const override { return bConverted; }
	virtual float GetFootprintRadius() const override;
	virtual float GetRestSeconds() const override { return RestSeconds; }

	// IPDInteractable
	virtual EPDInteractKind GetInteractKind() const override { return EPDInteractKind::Drag; }
	virtual bool CanInteract(const AActor* Guard) const override;
	virtual FVector GetInteractPoint() const override;
	virtual FText GetInteractLabel() const override;
	virtual void SetFocused(bool bFocused) override { bIsFocused = bFocused; }

	UFUNCTION(BlueprintPure, Category = "PD|Prop") FText GetTrickName() const { return TrickName; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> FocusRing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") EPDPropKind Kind = EPDPropKind::Cone;

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.1")) float MassKg = 6.0f;
	/** Lift while dragged so it slides over joints and the bank lip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float DragLift = 3.0f;
	/** Ground trace height when looking for the floor under a dragged prop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "10")) float GroundProbe = 220.0f;
	/** Moving less than this per tick counts as resting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float RestTolerance = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float RingPadding = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> RingMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> FocusMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> ConvertedMaterial;

private:
	void UpdateRing();

	FVector LastLocation = FVector::ZeroVector;
	FText TrickName;
	float RestSeconds = 0.0f;
	float BottomOffset = 0.0f;
	float CarryLift = 0.0f;
	bool bHeld = false;
	bool bConverted = false;
	bool bIsFocused = false;
};
