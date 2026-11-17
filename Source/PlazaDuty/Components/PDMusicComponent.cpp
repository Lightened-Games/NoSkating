// Copyright Plaza Duty. All Rights Reserved.

#include "Components/PDMusicComponent.h"
#include "Framework/PDMatchEvents.h"
#include "Interfaces/PDInterfaces.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "PlazaDuty.h"

namespace PDMusic
{
	TSoftObjectPtr<USoundBase> Track(const TCHAR* Folder, const TCHAR* Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/PlazaDuty/Audio/%s/%s.%s"), Folder, Name, Name)));
	}
}

UPDMusicComponent::UPDMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	MenuMusic = PDMusic::Track(TEXT("Music"), TEXT("MUS_Menu"));
	PlayMusic = PDMusic::Track(TEXT("Music"), TEXT("MUS_Play"));
	TensionMusic = PDMusic::Track(TEXT("Music"), TEXT("MUS_Tension"));
	VictorySting = PDMusic::Track(TEXT("Music"), TEXT("MUS_Victory"));
	DefeatSting = PDMusic::Track(TEXT("Music"), TEXT("MUS_Defeat"));
	AmbientCues = { PDMusic::Track(TEXT("Ambience"), TEXT("AMB_BusBrake")), PDMusic::Track(TEXT("Ambience"), TEXT("AMB_CupClink")),
		PDMusic::Track(TEXT("Ambience"), TEXT("AMB_Wings")) };
}

void UPDMusicComponent::BeginPlay()
{
	Super::BeginPlay();
	for (const TSoftObjectPtr<USoundBase>* S : { &MenuMusic, &PlayMusic, &TensionMusic, &VictorySting, &DefeatSting })
	{
		Preloaded.Add(S->LoadSynchronous());
	}
	for (const TSoftObjectPtr<USoundBase>& S : AmbientCues)
	{
		Preloaded.Add(S.LoadSynchronous());
	}
	Bind();
	SwitchTo(MenuMusic);  // under the start screen; a restart (menu skipped) switches to the play loop at once
}

void UPDMusicComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Current)
	{
		Current->Stop();
	}
	Super::EndPlay(Reason);
}

void UPDMusicComponent::Bind()
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
	Events->OnHUDVisible.AddDynamic(this, &UPDMusicComponent::HandleHUDVisible);
	Events->OnStateChanged.AddDynamic(this, &UPDMusicComponent::HandleState);
	Events->OnWearChanged.AddDynamic(this, &UPDMusicComponent::HandleWear);
	bBound = true;
}

void UPDMusicComponent::SwitchTo(const TSoftObjectPtr<USoundBase>& Track)
{
	if (Track == CurrentTrack && Current && Current->IsPlaying())
	{
		return;
	}
	if (Current)
	{
		Current->FadeOut(FadeSeconds, 0.0f);
		Current = nullptr;
	}
	CurrentTrack = Track;
	if (USoundBase* S = Track.LoadSynchronous())
	{
		Current = UGameplayStatics::CreateSound2D(this, S, MusicVolume, 1.0f, 0.0f, nullptr, true, false);
		if (Current)
		{
			Current->FadeIn(FadeSeconds, MusicVolume);
		}
	}
}

void UPDMusicComponent::HandleHUDVisible(bool bVisible)
{
	if (bVisible && !bEnded)
	{
		SwitchTo(bTension ? TensionMusic : PlayMusic);
	}
}

void UPDMusicComponent::HandleState(EPDMatchState NewState)
{
	if (NewState == EPDMatchState::Active && !bEnded)
	{
		SwitchTo(bTension ? TensionMusic : PlayMusic);
	}
	if ((NewState == EPDMatchState::Victory || NewState == EPDMatchState::Defeat) && !bEnded)
	{
		bEnded = true;
		if (Current)
		{
			Current->FadeOut(0.6f, 0.0f);
			Current = nullptr;
		}
		CurrentTrack.Reset();
		if (USoundBase* S = (NewState == EPDMatchState::Victory ? VictorySting : DefeatSting).LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, S, StingVolume);
		}
	}
}

void UPDMusicComponent::HandleWear(float Wear, float Limit)
{
	if (bEnded || Limit <= 0.0f)
	{
		return;
	}
	const float F = Wear / Limit;
	if (!bTension && F >= TensionOn)
	{
		bTension = true;
		SwitchTo(TensionMusic);
	}
	else if (bTension && F <= TensionOff)
	{
		bTension = false;
		SwitchTo(PlayMusic);
	}
}

void UPDMusicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Bind();
	AmbientLeft -= DeltaTime;
	if (AmbientLeft > 0.0f || AmbientCues.Num() == 0 || !GetOwner())
	{
		return;
	}
	AmbientLeft = FMath::FRandRange(AmbientGap.X, AmbientGap.Y);
	if (USoundBase* S = AmbientCues[FMath::RandRange(0, AmbientCues.Num() - 1)].LoadSynchronous())
	{
		const FVector Dir = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
		const FVector At = GetOwner()->GetActorLocation() + Dir * FMath::FRandRange(AmbientDistance.X, AmbientDistance.Y);
		UGameplayStatics::PlaySoundAtLocation(this, S, At, AmbientVolume);
	}
}
