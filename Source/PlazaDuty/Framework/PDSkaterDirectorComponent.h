// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PDTypes.h"
#include "PDSkaterDirectorComponent.generated.h"

class UPDMatchConfig;
class USkeletalMesh;

/**
 * Spawns and retires skaters (lives on the GameMode): the opening crew, phase top-ups, friends drawn in by a
 * big trick on one of the guard's props, and everyone rolling home at closing.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDSkaterDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDSkaterDirectorComponent();

	void Init(const UPDMatchConfig* InConfig, int32 InSeed);
	/** Opening crew, already rolling during the countdown. */
	void SpawnOpeningCrew(int32 Count);
	/** Phase top-ups (call while Active). */
	void TickDirector(float DeltaSeconds, float Elapsed, const FPDPhaseRow& Phase);
	/** A trick landed: friends come for big tricks on props. Returns the friend spawned, if any. */
	AActor* HandleTrick(const FPDTrickEvent& Trick, float Elapsed, const FPDPhaseRow& Phase);
	void SendEveryoneHome();
	TArray<AActor*> GetSkaters() const;
	int32 GetActiveCount() const;

	UPROPERTY(EditAnywhere, Category = "Config") TSubclassOf<APawn> SkaterClass;
	/** Seconds between phase top-up arrivals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArrivalInterval = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpawnHeight = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EntrySpread = 140.0f;

private:
	AActor* SpawnSkater(bool bFriend);

	UPROPERTY() TArray<TObjectPtr<AActor>> Skaters;
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> Bodies;
	UPROPERTY() TObjectPtr<const UPDMatchConfig> Config;
	TArray<FTransform> Entries;
	FRandomStream Random;
	float ArrivalTimer = 0.0f;
	float LastFriendTime = -100.0f;
	int32 Spawned = 0;
	int32 Seed = 7;
};
