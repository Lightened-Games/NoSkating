// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PDArenaBounds.generated.h"

class UBoxComponent;

/**
 * Out-of-bounds safety net (P19): the guard, if it leaves the plaza box or drops below the floor is sent back
 * through its FellOutOfWorld override (teleport to its safe spot with zero velocity). KillZ -2000 backs it up.
 */
UCLASS()
class PLAZADUTY_API APDArenaBounds : public AActor
{
	GENERATED_BODY()

public:
	APDArenaBounds();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CheckInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FloorTolerance = 300.0f;

private:
	float Timer = 0.0f;
};
