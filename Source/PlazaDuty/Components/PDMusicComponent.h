// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PDTypes.h"
#include "PDMusicComponent.generated.h"

class USoundBase;
class UAudioComponent;

/**
 * Sound bed (design guide 2.5.5): music per state plus distant ambient one-shots. Listens to the match event bus only.
 * Menu loop under the start screen -> play loop when the HUD appears -> tension loop at high wear -> victory / defeat
 * sting. Loops crossfade. All tracks are original (Tools/generate_music.py, CC0).
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPDMusicComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TSoftObjectPtr<USoundBase> MenuMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TSoftObjectPtr<USoundBase> PlayMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TSoftObjectPtr<USoundBase> TensionMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TSoftObjectPtr<USoundBase> VictorySting;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TSoftObjectPtr<USoundBase> DefeatSting;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TArray<TSoftObjectPtr<USoundBase>> AmbientCues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MusicVolume = 0.42f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StingVolume = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeSeconds = 1.6f;
	/** Wear fraction that switches to the tension loop, and the fraction that switches back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TensionOn = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TensionOff = 0.7f;
	/** Seconds between ambient cues (random in range), their distance from the guard and volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D AmbientGap = FVector2D(7.0f, 15.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D AmbientDistance = FVector2D(900.0f, 2200.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AmbientVolume = 0.5f;

private:
	UFUNCTION() void HandleHUDVisible(bool bVisible);
	UFUNCTION() void HandleState(EPDMatchState NewState);
	UFUNCTION() void HandleWear(float Wear, float Limit);
	void SwitchTo(const TSoftObjectPtr<USoundBase>& Track);
	void Bind();

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Current;
	/** Everything loaded up front: a synchronous load at a state change hitched the frame (FactoryQA glitch log). */
	UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> Preloaded;
	TSoftObjectPtr<USoundBase> CurrentTrack;
	float AmbientLeft = 5.0f;
	bool bBound = false;
	bool bTension = false;
	bool bEnded = false;
};
