// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/PDTypes.h"
#include "PDMatchConfig.generated.h"

class USkeletalMesh;

/**
 * Every shift number lives here (DA_PD_Match). Difficulty and pacing are edited in this asset, never in code.
 * The GameMode falls back to these C++ defaults when the asset is missing.
 */
UCLASS(BlueprintType)
class PLAZADUTY_API UPDMatchConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPDMatchConfig();

	/** Tuning for a prop kind (falls back to the first row). */
	const FPDPropTuning& GetPropTuning(EPDPropKind Kind) const;
	/** Phase active at a match time. */
	int32 GetPhaseIndex(float Elapsed) const;

	// ---- Round ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "1")) int32 CountdownSeconds = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "10.0")) float RoundSeconds = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "1.0")) float WearLimit = 100.0f;
	/** Wear at or above this fraction of the limit is shown in the danger colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "0.1", ClampMax = "1.0")) float WearDangerFraction = 0.8f;
	/** "CLOSING SOON" warning this many seconds before the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "0.0")) float ClosingWarningSeconds = 10.0f;
	/** Skaters stop starting new runs this many seconds before closing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "0.0")) float LastRunSeconds = 2.0f;

	// ---- Guard kit ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard", meta = (ClampMin = "0")) int32 SkatestopperCount = 3;

	// ---- Skaters ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skaters", meta = (ClampMin = "1")) int32 MaxSkaters = 6;
	/** A single trick on a converted prop worth at least this much wear calls a friend. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skaters", meta = (ClampMin = "0")) float FriendTriggerWear = 7.0f;
	/** Rider bodies (Rocketbox humans on the shared skeleton), assigned in turn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skaters") TArray<TSoftObjectPtr<USkeletalMesh>> SkaterMeshes;

	// ---- Content ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Props") TArray<FPDPropTuning> Props;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phases") TArray<FPDPhaseRow> Phases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Random") int32 Seed = 7;
};
