// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PDInterfaces.h"
#include "PDSkateLineActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * One skate line (new system N1): a looping route through a plaza feature. Skaters ride it, land the feature
 * trick and add wear. Props dragged onto the route block it for a few seconds, then become extra tricks.
 * A bolted skatestopper kills the feature for good. Placed and configured by the level builder script.
 */
UCLASS()
class PLAZADUTY_API APDSkateLineActor : public AActor, public IPDSkateLine, public IPDInteractable
{
	GENERATED_BODY()

public:
	APDSkateLineActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IPDSkateLine
	virtual FName GetLineId() const override { return LineId; }
	virtual FText GetLineName() const override { return LineName; }
	virtual FText GetTrickName() const override { return TrickName; }
	virtual EPDTrickStyle GetTrickStyle() const override { return TrickStyle; }
	virtual const TArray<FVector>& GetPath() const override { return Points; }
	virtual float GetPathLength() const override { return TotalLength; }
	virtual float GetFeatureStartDistance() const override;
	virtual float GetFeatureEndDistance() const override;
	virtual float GetRunEndDistance() const override;
	virtual FVector GetPointAtDistance(float Distance) const override;
	virtual float GetDistanceClosestTo(const FVector& World) const override;
	virtual bool IsOpen() const override;
	virtual bool IsCapped() const override { return bCapped; }
	virtual bool IsCappable() const override { return bCappable; }
	virtual bool IsBlocked() const override;
	virtual float GetAppeal() const override;
	virtual float GetBaseWear() const override { return BaseWear; }
	virtual float GetWearPerRun() const override;
	virtual const TArray<FPDPropTrick>& GetPropTricks() const override { return PropTricks; }
	virtual float GetHeat() const override { return Heat; }
	virtual void NotifyRun(AActor* Skater) override;
	virtual void NotifyTrickLanded(float Wear) override;
	virtual FVector GetKnobPoint() const override { return KnobPoint; }
	virtual FVector GetBoltStandPoint() const override { return BoltStandPoint; }
	virtual void Bolt() override;
	virtual float GetFeatureHeight() const override { return FeatureHeight; }
	virtual bool StartsOnDeck() const override { return bStartsOnDeck; }
	virtual void ScaleOpeningTime(float Scale) override { OpensAt *= Scale; }

	// IPDInteractable (the knob socket)
	virtual EPDInteractKind GetInteractKind() const override { return EPDInteractKind::Bolt; }
	virtual bool CanInteract(const AActor* Guard) const override;
	virtual FVector GetInteractPoint() const override { return KnobPoint; }
	virtual FText GetInteractLabel() const override;
	virtual void SetFocused(bool bFocused) override { bIsFocused = bFocused; }

	// ---- Line data (set by Tools/Editor/build_level.py) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") FName LineId = TEXT("L1");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") FText LineName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") FText TrickName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") EPDTrickStyle TrickStyle = EPDTrickStyle::Grind;
	/** World-space loop points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") TArray<FVector> Points;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") int32 FeatureStartIndex = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") int32 FeatureEndIndex = 2;
	/** The run ends here (after the landing); the rest of the loop is the ride back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") int32 RunEndIndex = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") FVector KnobPoint = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") FVector BoltStandPoint = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line") bool bStartsOnDeck = false;

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float BaseWear = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float BaseAppeal = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") bool bCappable = true;
	/** Match second at which skaters may use the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float OpensAt = 0.0f;
	/** Grind height above the path (ledge / rail top). 0 for gaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float FeatureHeight = 45.0f;
	/** A prop closer than this to the run blocks the line (uu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "10")) float LineBlockRadius = 160.0f;
	/** A planter (any prop) this close to the take-off of a gap line becomes its launch version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float LaunchRadius = 320.0f;
	/** Skaters avoid the line while the guard stands this close to the feature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float GuardDeterRadius = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0", ClampMax = "1")) float GuardDeterFactor = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float HeatPerTrick = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float HeatDecay = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0.02")) float UpdateInterval = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "1")) int32 KnobCount = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "10")) float LaneWidth = 46.0f;
	/** Lane glow (emissive, capped by the material at 3.5) at rest and at full heat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float LaneGlowIdle = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float LaneGlowHot = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0")) float SocketBobHeight = 12.0f;

	// ---- Look ----
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> LaneMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> LaneMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> KnobMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> KnobMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> SocketMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> ChevronMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> AccentMaterial;

private:
	void BuildCache();
	void BuildVisuals();
	void UpdateProps();
	void UpdateVisuals(float DeltaSeconds);
	/** Closest distance from World to the run part of the path; outputs the path distance there. */
	float DistanceToRun(const FVector& World, float& OutPathDistance) const;

	struct FTracked
	{
		TWeakObjectPtr<AActor> Prop;
		float Since = 0.0f;
		float PathDistance = 0.0f;
		bool bConverted = false;
	};

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LaneParts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LaneMids;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Knobs;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Socket;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Chevron;

	TArray<float> Cumulative;
	float TotalLength = 0.0f;
	TArray<FTracked> Tracked;
	TArray<FPDPropTrick> PropTricks;
	float Heat = 0.0f;
	float UpdateTimer = 0.0f;
	float KnobPop = 0.0f;
	float Age = 0.0f;
	bool bCapped = false;
	bool bGuardNear = false;
	bool bIsFocused = false;
};
