// Copyright Plaza Duty. All Rights Reserved.

#include "Components/PDGameFeelComponent.h"
#include "Framework/PDMatchEvents.h"
#include "Interfaces/PDInterfaces.h"
#include "Feedback/PDCameraShakes.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"

UPDGameFeelComponent::UPDGameFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;

	ImpactShake = UPDImpactShake::StaticClass();
	MicroShake = UPDMicroShake::StaticClass();
	RumbleShake = UPDRumbleShake::StaticClass();

	CountdownSound = UPDAudioLibrary::CountdownTick();
	GoSound = UPDAudioLibrary::GoWhistle();
	GrindSound = UPDAudioLibrary::GrindScrape();
	LandSound = UPDAudioLibrary::BoardLand();
	WearSound = UPDAudioLibrary::WearTick();
	CappedSound = UPDAudioLibrary::BoltClunk();
	ConvertedSound = UPDAudioLibrary::PropConverted();
	FriendSound = UPDAudioLibrary::FriendChime();
	WhistleSound = UPDAudioLibrary::Whistle();
	KnockSound = UPDAudioLibrary::KnockThud();
	ClosingSound = UPDAudioLibrary::ClosingChime();
	VictorySound = UPDAudioLibrary::Fanfare();
	DefeatSound = UPDAudioLibrary::DefeatStinger();
	PromptSound = UPDAudioLibrary::PromptBlip();

	LandingDust.Count = 6; LandingDust.Life = 0.6f; LandingDust.StartSize = 18.0f; LandingDust.EndSize = 75.0f;
	LandingDust.Spread = 160.0f; LandingDust.Rise = 30.0f; LandingDust.Color = FLinearColor(0.72f, 0.68f, 0.6f); LandingDust.Opacity = 0.5f;

	GraniteChips.Count = 5; GraniteChips.Life = 0.45f; GraniteChips.StartSize = 7.0f; GraniteChips.EndSize = 4.0f;
	GraniteChips.Spread = 260.0f; GraniteChips.Rise = 260.0f; GraniteChips.Gravity = 980.0f; GraniteChips.Color = FLinearColor(0.55f, 0.54f, 0.52f); GraniteChips.Opacity = 0.95f;

	KnobSparks.Count = 9; KnobSparks.Life = 0.4f; KnobSparks.StartSize = 6.0f; KnobSparks.EndSize = 2.0f;
	KnobSparks.Spread = 320.0f; KnobSparks.Rise = 220.0f; KnobSparks.Gravity = 900.0f; KnobSparks.Color = FLinearColor(1.0f, 0.62f, 0.18f); KnobSparks.Opacity = 1.0f; KnobSparks.Glow = 3.0f;

	WhistleRing.bGroundRing = true; WhistleRing.Life = 0.55f; WhistleRing.StartSize = 60.0f; WhistleRing.EndSize = 900.0f;
	WhistleRing.Spread = 0.0f; WhistleRing.Rise = 0.0f; WhistleRing.Color = FLinearColor(0.95f, 0.7f, 0.24f); WhistleRing.Opacity = 0.7f; WhistleRing.Glow = 1.5f;

	KnockDust.Count = 9; KnockDust.Life = 0.8f; KnockDust.StartSize = 25.0f; KnockDust.EndSize = 95.0f;
	KnockDust.Spread = 200.0f; KnockDust.Rise = 45.0f; KnockDust.Color = FLinearColor(0.7f, 0.66f, 0.6f); KnockDust.Opacity = 0.55f;

	ConvertSparkle.Count = 8; ConvertSparkle.Life = 0.7f; ConvertSparkle.StartSize = 9.0f; ConvertSparkle.EndSize = 3.0f;
	// D34 (user, 2026-11-09: "the smoke effects and blast look bad, avoid them as much as possible"): every sprite
	// effect is off; sound, camera shake, hit-stop and the floating wear numbers carry the feedback.
	LandingDust.bEnabled = GraniteChips.bEnabled = KnobSparks.bEnabled = WhistleRing.bEnabled = KnockDust.bEnabled = ConvertSparkle.bEnabled = false;
	ConvertSparkle.Spread = 140.0f; ConvertSparkle.Rise = 220.0f; ConvertSparkle.Gravity = 300.0f; ConvertSparkle.Color = FLinearColor(0.9f, 0.25f, 0.25f); ConvertSparkle.Opacity = 1.0f; ConvertSparkle.Glow = 2.5f;
}

void UPDGameFeelComponent::BeginPlay()
{
	Super::BeginPlay();
	Bind();
	Ambience = NewObject<UPDLoopSynth>(this);
	Ambience->InitLoop(17);
	Ambience->SetLoopVolume(AmbienceVolume);
	AmbienceComp = UGameplayStatics::SpawnSound2D(this, Ambience, 1.0f, 1.0f, 0.0f, nullptr, true, false);
}

void UPDGameFeelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bHitStopActive)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}
	if (AmbienceComp)
	{
		AmbienceComp->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void UPDGameFeelComponent::Bind()
{
	if (bBound)
	{
		return;
	}
	IPDMatchFlow* Flow = PD::GetFlow(this);
	UPDMatchEvents* Events = Flow ? Flow->GetMatchEvents() : nullptr;
	if (!Events)
	{
		return;
	}
	Events->OnStateChanged.AddDynamic(this, &UPDGameFeelComponent::HandleState);
	Events->OnCountdown.AddDynamic(this, &UPDGameFeelComponent::HandleCountdown);
	Events->OnTrickLanded.AddDynamic(this, &UPDGameFeelComponent::HandleTrick);
	Events->OnLineCapped.AddDynamic(this, &UPDGameFeelComponent::HandleCapped);
	Events->OnPropConverted.AddDynamic(this, &UPDGameFeelComponent::HandleConverted);
	Events->OnWhistle.AddDynamic(this, &UPDGameFeelComponent::HandleWhistle);
	Events->OnKnockdown.AddDynamic(this, &UPDGameFeelComponent::HandleKnockdown);
	Events->OnFriendArrived.AddDynamic(this, &UPDGameFeelComponent::HandleFriend);
	Events->OnPrompt.AddDynamic(this, &UPDGameFeelComponent::HandlePrompt);
	Events->OnClosingSoon.AddDynamic(this, &UPDGameFeelComponent::HandleClosing);
	bBound = true;
}

void UPDGameFeelComponent::HitStop(float RealSeconds, float Dilation)
{
	UGameplayStatics::SetGlobalTimeDilation(this, Dilation);
	HitStopEnd = FPlatformTime::Seconds() + RealSeconds;
	bHitStopActive = true;
}

void UPDGameFeelComponent::Shake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && ShakeClass && Scale > 0.0f)
	{
		PC->ClientStartCameraShake(ShakeClass, Scale);
	}
}

void UPDGameFeelComponent::Puff(const FVector& Location, const FPDPuffStyle& Style)
{
	if (Style.bEnabled)
	{
		APDPuffFX::Spawn(GetWorld(), Location, Style, PuffSeed++);
	}
}

void UPDGameFeelComponent::Play(const FPDSynthPreset& Preset, const FVector& Location, float Volume, float Pitch, bool b2D)
{
	if (b2D)
	{
		UPDAudioLibrary::PlaySynth2D(this, Preset, Volume, Pitch);
	}
	else
	{
		UPDAudioLibrary::PlaySynthAtLocation(this, Preset, Location, Volume, Pitch);
	}
}

void UPDGameFeelComponent::HandleState(EPDMatchState NewState)
{
	const FVector Here = GetOwner()->GetActorLocation();
	switch (NewState)
	{
	case EPDMatchState::Active:
		Play(GoSound, Here, 1.0f, 1.0f, true);
		break;
	case EPDMatchState::Victory:
		Play(VictorySound, Here, 1.0f, 1.0f, true);
		Shake(RumbleShake, 0.5f);
		break;
	case EPDMatchState::Defeat:
		Play(DefeatSound, Here, 1.0f, 1.0f, true);
		Shake(ImpactShake, 0.6f);
		break;
	default:
		break;
	}
}

void UPDGameFeelComponent::HandleCountdown(int32 SecondsLeft)
{
	if (SecondsLeft > 0)
	{
		Play(CountdownSound, GetOwner()->GetActorLocation(), 1.0f, 1.0f, true);
	}
}

void UPDGameFeelComponent::HandleTrick(const FPDTrickEvent& Trick)
{
	Puff(Trick.Location, LandingDust);
	Puff(Trick.Location + FVector(0.0f, 0.0f, 10.0f), GraniteChips);
	Play(LandSound, Trick.Location, 1.0f, FMath::FRandRange(0.92f, 1.08f));
	// The chip tick rises in pitch as the plaza wears out.
	const IPDMatchFlow* Flow = PD::GetFlow(this);
	const float WearFrac = Flow ? Flow->GetWear() / FMath::Max(1.0f, Flow->GetWearLimit()) : 0.0f;
	Play(WearSound, Trick.Location, 0.9f, 0.85f + 0.6f * WearFrac);
	if (FVector::Dist(Trick.Location, GetOwner()->GetActorLocation()) < NearLandingRadius)
	{
		Shake(MicroShake, NearLandingShakeScale);
	}
}

void UPDGameFeelComponent::HandleCapped(AActor* Line, int32 KnobsLeft)
{
	const IPDSkateLine* L = Cast<IPDSkateLine>(Line);
	const FVector Where = L ? L->GetKnobPoint() + FVector(0.0f, 0.0f, L->GetFeatureHeight()) : GetOwner()->GetActorLocation();
	Puff(Where, KnobSparks);
	Play(CappedSound, Where);
	Shake(MicroShake, 1.0f);
}

void UPDGameFeelComponent::HandleConverted(AActor* Line, AActor* Prop, const FText& TrickName)
{
	if (Prop)
	{
		Puff(Prop->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), ConvertSparkle);
		Play(ConvertedSound, Prop->GetActorLocation(), 1.0f, 1.0f, true);
	}
}

void UPDGameFeelComponent::HandleWhistle(AActor* Guard, int32 Scattered)
{
	const FVector Here = Guard ? Guard->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f) : GetOwner()->GetActorLocation();
	Puff(Here + FVector(0.0f, 0.0f, 3.0f), WhistleRing);
	Play(WhistleSound, Here, 1.0f, 1.0f, true);
	Shake(RumbleShake, 0.6f);
}

void UPDGameFeelComponent::HandleKnockdown(AActor* Guard, AActor* Skater)
{
	const FVector Here = Guard ? Guard->GetActorLocation() - FVector(0.0f, 0.0f, 80.0f) : GetOwner()->GetActorLocation();
	Puff(Here, KnockDust);
	Play(KnockSound, Here, 1.0f, 1.0f, true);
	Shake(ImpactShake, 1.0f);
	HitStop(HitStopSeconds, HitStopDilation);
}

void UPDGameFeelComponent::HandleFriend(AActor* Skater)
{
	if (Skater)
	{
		Play(FriendSound, Skater->GetActorLocation(), 0.8f, 1.0f, true);
		Play(UPDAudioLibrary::RollIn(), Skater->GetActorLocation());
	}
}

void UPDGameFeelComponent::HandlePrompt(EPDPrompt Prompt, AActor* Anchor)
{
	Play(PromptSound, GetOwner()->GetActorLocation(), 1.0f, 1.0f, true);
}

void UPDGameFeelComponent::HandleClosing()
{
	Play(ClosingSound, GetOwner()->GetActorLocation(), 1.0f, 1.0f, true);
}

void UPDGameFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBound)
	{
		Bind();
	}
	if (bHitStopActive && FPlatformTime::Seconds() >= HitStopEnd)
	{
		bHitStopActive = false;
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}

	const IPDMatchFlow* Flow = PD::GetFlow(this);
	if (Ambience && Flow)
	{
		const FVector Here = GetOwner()->GetActorLocation();
		int32 Near = 0;
		for (const AActor* S : Flow->GetSkaters())
		{
			if (S && FVector::Dist(S->GetActorLocation(), Here) < RollingRadius)
			{
				++Near;
			}
		}
		const bool bPlaying = Flow->GetMatchState() == EPDMatchState::Active;
		Ambience->SetRolling(bPlaying ? FMath::Clamp(Near / FMath::Max(1.0f, RollingFullCount), 0.0f, 1.0f) : 0.0f);
		Ambience->SetDrive(bPlaying ? Flow->GetWear() / FMath::Max(1.0f, Flow->GetWearLimit()) : 0.0f);
		const float Round = Flow->GetElapsed() + Flow->GetTimeRemaining();
		Ambience->SetLightsOn(Round > 0.0f ? Flow->GetElapsed() / Round : 0.0f);
	}
}
