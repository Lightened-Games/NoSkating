// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PDTypes.h"
#include "PDMatchEvents.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDStateChanged, EPDMatchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDCountdown, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDPhaseChanged, int32, PhaseIndex, const FPDPhaseRow&, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDTrickLanded, const FPDTrickEvent&, Trick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDWearChanged, float, Wear, float, Limit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDLineCapped, AActor*, Line, int32, KnobsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPDPropConverted, AActor*, Line, AActor*, Prop, const FText&, TrickName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDPropMoved, AActor*, Prop, bool, bOnLine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDWhistle, AActor*, Guard, int32, Scattered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDKnockdown, AActor*, Guard, AActor*, Skater);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDFriendArrived, AActor*, Skater);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPDPrompt, EPDPrompt, Prompt, AActor*, Anchor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPDClosingSoon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPDHUDVisible, bool, bVisible);

/**
 * The match event bus (lives on the GameMode). Gameplay reports through IPDMatchFlow, the GameMode broadcasts
 * here, and the HUD, audio and juice only listen. Nothing that listens ever changes gameplay.
 */
UCLASS(ClassGroup = (PlazaDuty), meta = (BlueprintSpawnableComponent))
class PLAZADUTY_API UPDMatchEvents : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDStateChanged OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDCountdown OnCountdown;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDPhaseChanged OnPhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDTrickLanded OnTrickLanded;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDWearChanged OnWearChanged;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDLineCapped OnLineCapped;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDPropConverted OnPropConverted;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDPropMoved OnPropMoved;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDWhistle OnWhistle;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDKnockdown OnKnockdown;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDFriendArrived OnFriendArrived;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDPrompt OnPrompt;
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDClosingSoon OnClosingSoon;
	/** The game HUD hides under the start screen and the intro (FactoryKit), and shows when play starts. */
	UPROPERTY(BlueprintAssignable, Category = "PD|Events") FPDHUDVisible OnHUDVisible;
};
