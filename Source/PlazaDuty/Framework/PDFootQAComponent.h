// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PDFootQAComponent.generated.h"

/**
 * QA only (-PDFootQA): measures how far every character's soles are from what they stand on, per animation action,
 * and how far each pawn's static meshes (the skateboards) hover above the ground. Prints a summary every
 * ReportInterval seconds so floating or sunken feet show up as numbers, not impressions.
 *   sole gap = lowest foot bone - AnkleHeight - support top (the deck under a skater, the floor under the guard).
 */
UCLASS(ClassGroup = (PlazaDuty))
class PLAZADUTY_API UPDFootQAComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDFootQAComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QA") FName LeftFootBone = TEXT("Bip01-L-Foot");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QA") FName RightFootBone = TEXT("Bip01-R-Foot");
	/** Ankle bone height above the sole in the clips (measured: 9.4 cm in the idle pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QA") float AnkleHeight = 9.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QA") float SampleInterval = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QA") float ReportInterval = 10.0f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Sample();
	void Report();
	TMap<FString, TArray<float>> Gaps;
	float SampleClock = 0.0f;
	float ReportClock = 0.0f;
};
