// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PDPetalFall.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Life pass (design guide 2.5.5): jacaranda petals drifting down under a tree canopy, swaying and spinning, and
 * starting again at the top. One instanced mesh per tree; no collision (tagged NoCollide for the C2 audit).
 */
UCLASS()
class PLAZADUTY_API APDPetalFall : public AActor
{
	GENERATED_BODY()

public:
	APDPetalFall();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UInstancedStaticMeshComponent> Petals;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") TSoftObjectPtr<UStaticMesh> PetalMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") TSoftObjectPtr<UMaterialInterface> PetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 Count = 28;
	/** Canopy radius and height band the petals start from (relative to the actor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Radius = 380.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D StartHeight = FVector2D(260.0f, 520.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D FallSpeed = FVector2D(35.0f, 70.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SwayAmplitude = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PetalScale = 0.05f;
	/** Drift with the breeze (cm/s, world). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector Breeze = FVector(18.0f, 10.0f, 0.0f);

private:
	struct FPetal
	{
		FVector Base;
		float Z = 0.0f;
		float Speed = 0.0f;
		float Phase = 0.0f;
		float Spin = 0.0f;
		float Age = 0.0f;
	};
	void Respawn(FPetal& P, bool bAnywhere);
	TArray<FPetal> State;
};
