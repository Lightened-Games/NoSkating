// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PDPuffFX.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UMaterialInterface;

/** What a puff looks like (dust on landings, chips off the granite, sparks on knobs, the whistle ring). */
USTRUCT(BlueprintType)
struct FPDPuffStyle
{
	GENERATED_BODY()

	/** Off: nothing spawns (the user found the smoke puffs and the whistle blast ring cheap-looking, 2026-11-09). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "1")) int32 Count = 7;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "0.05")) float Life = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "1")) float StartSize = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "1")) float EndSize = 70.0f;
	/** Outward speed (uu/s) and upward speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") float Spread = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") float Rise = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") float Gravity = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") FLinearColor Color = FLinearColor(0.75f, 0.7f, 0.62f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "0", ClampMax = "1")) float Opacity = 0.55f;
	/** Emissive multiplier for glowing particles (sparks). Capped at 3.5 by the material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMin = "0", ClampMax = "3.5")) float Glow = 0.0f;
	/** Flat ring on the ground that expands (whistle shockwave) instead of billboards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff") bool bGroundRing = false;
};

/**
 * Short-lived camera-facing quads (P37-style flipbook FX without Niagara assets): soft dust, granite chips,
 * knob sparks and the whistle ring. Spawned by UPDGameFeelComponent, destroys itself.
 */
UCLASS()
class PLAZADUTY_API APDPuffFX : public AActor
{
	GENERATED_BODY()

public:
	APDPuffFX();

	static APDPuffFX* Spawn(UWorld* World, const FVector& Location, const FPDPuffStyle& Style, int32 Seed);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> QuadMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> PuffMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UStaticMesh> RingMesh;
	UPROPERTY(EditAnywhere, Category = "Look") TSoftObjectPtr<UMaterialInterface> RingMaterial;

private:
	void Init(const FPDPuffStyle& InStyle, int32 Seed);

	struct FParticle
	{
		TObjectPtr<UStaticMeshComponent> Quad;
		TObjectPtr<UMaterialInstanceDynamic> Mid;
		FVector Velocity = FVector::ZeroVector;
		float Spin = 0.0f;
		float SizeScale = 1.0f;
	};

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Quads;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	TArray<FParticle> Particles;
	FPDPuffStyle Style;
	float Age = 0.0f;
	float MeshSize = 100.0f;
};
