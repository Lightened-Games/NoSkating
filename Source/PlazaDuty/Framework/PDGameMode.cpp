// Copyright Plaza Duty. All Rights Reserved.

#include "Framework/PDGameMode.h"
#include "Framework/PDMatchEvents.h"
#include "Framework/PDSkaterDirectorComponent.h"
#include "Framework/PDPlayerController.h"
#include "Framework/PDAutoplayComponent.h"
#include "Framework/PDFootQAComponent.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Interfaces/PDInterfaces.h"
#include "Player/PDGuardCharacter.h"
#include "Data/PDMatchConfig.h"
#include "World/PDArenaBounds.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "PlazaDuty.h"

#define LOCTEXT_NAMESPACE "PlazaDuty"

namespace PDModeStatics
{
	int32 RunsCompleted = 0;
	bool bShotsScheduled = false;
}

APDGameMode::APDGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = APDGuardCharacter::StaticClass();
	PlayerControllerClass = APDPlayerController::StaticClass();
	Events = CreateDefaultSubobject<UPDMatchEvents>(TEXT("Events"));
	Director = CreateDefaultSubobject<UPDSkaterDirectorComponent>(TEXT("Director"));
	MatchConfig = TSoftObjectPtr<UPDMatchConfig>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Data/DA_PD_Match.DA_PD_Match")));
}

void APDGameMode::LoadConfig()
{
	Config = MatchConfig.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogPlaza, Warning, TEXT("[Plaza] DA_PD_Match missing, using C++ defaults."));
		Config = NewObject<UPDMatchConfig>(this);
	}
	RunSeed = Config->Seed;
}

void APDGameMode::BeginPlay()
{
	Super::BeginPlay();
	LoadConfig();
	ParseCommandLine();
	if (AWorldSettings* WS = GetWorldSettings())
	{
		WS->KillZ = KillZ;
		WS->bEnableWorldBoundsChecks = true;
	}
	GatherLevel();
	if (!TActorIterator<APDArenaBounds>(GetWorld()))
	{
		GetWorld()->SpawnActor<APDArenaBounds>(APDArenaBounds::StaticClass(), FTransform(FVector(0.0f, 0.0f, 300.0f)));
	}
	ScheduleQAScreenshots();
	if (FParse::Param(FCommandLine::Get(), TEXT("PDFootQA")))
	{
		// QA: log measured sole gaps per action (floating / sunken feet) and board heights.
		UPDFootQAComponent* FootQA = NewObject<UPDFootQAComponent>(this, TEXT("FootQA"));
		FootQA->RegisterComponent();
	}
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] GameMode BeginPlay: seed %d, %d lines, %d props, %d dead zones."), RunSeed, Lines.Num(), Props.Num(), DeadZones.Num());
}

void APDGameMode::ParseCommandLine()
{
	const TCHAR* Cmd = FCommandLine::Get();
	int32 Seed = 0;
	if (FParse::Value(Cmd, TEXT("PDSeed="), Seed))
	{
		RunSeed = Seed;
	}
	bDefeatMode = FParse::Param(Cmd, TEXT("PDDefeat"));
	FParse::Value(Cmd, TEXT("PDPortrait="), PortraitIndex);
	FParse::Value(Cmd, TEXT("PDPortraitYaw="), PortraitYaw);
	bShowcase = FParse::Param(Cmd, TEXT("PDShowcase"));
	bPerf = FParse::Param(Cmd, TEXT("PDPerf"));
	// Each autoplay restart gets a different seed so chained runs cover different crowds.
	RunSeed += PDModeStatics::RunsCompleted * 1009;
}

void APDGameMode::CheckCommandLineAutoplay()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("autoplay")))
	{
		UE_LOG(LogPlaza, Warning, TEXT("Autoplay mode requested via command line."));
		bAutoplayActive = true;
		SetupAutoplayController();
	}
}

void APDGameMode::SetupAutoplayController()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return;
	}
	if (UPDAutoplayComponent* Bot = PC->FindComponentByClass<UPDAutoplayComponent>())
	{
		const bool bIdle = FParse::Param(FCommandLine::Get(), TEXT("PDIdle"));
		Bot->StartBot(bDefeatMode || bIdle, RunSeed);
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] Autoplay bot driving %s%s."), *PC->GetPawn()->GetName(), bDefeatMode ? TEXT(" (DEFEAT mode: the guard stands still)") : TEXT(""));
	}
}

void APDGameMode::ScheduleQAScreenshots()
{
	FString ShotList;
	if (PDModeStatics::bShotsScheduled || !FParse::Value(FCommandLine::Get(), TEXT("PDShots="), ShotList, false))
	{
		return;
	}
	PDModeStatics::bShotsScheduled = true;
	FString Tag;
	FParse::Value(FCommandLine::Get(), TEXT("PDShotTag="), Tag);
	TArray<FString> Times;
	ShotList.ParseIntoArray(Times, TEXT(","), true);
	for (int32 i = 0; i < Times.Num(); ++i)
	{
		const float Delay = FCString::Atof(*Times[i]);
		if (Delay <= 0.0f)
		{
			continue;
		}
		// No '.' in the name: it would be taken as the file extension.
		const FString Name = FString::Printf(TEXT("PDShot%s_%02d_%04dds"), *Tag, i, FMath::RoundToInt(Delay * 10.0f));
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(Name, true, false);
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] Screenshot %s"), *Name);
		}), Delay, false);
	}
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] QA screenshots scheduled: %s"), *ShotList);
}

void APDGameMode::GatherLevel()
{
	Lines.Reset();
	Props.Reset();
	DeadZones.Reset();
	ClosingLamps.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A->ActorHasTag(PD::LineTag) && Cast<IPDSkateLine>(A))
		{
			Lines.Add(A);
		}
		else if (A->ActorHasTag(PD::PropTag) && Cast<IPDDraggable>(A))
		{
			Props.Add(A);
		}
		else if (A->ActorHasTag(PD::DeadZoneTag))
		{
			DeadZones.Add(A->GetActorLocation());
		}
		else if (A->ActorHasTag(PD::ClosingLampTag))
		{
			ClosingLamps.Add(A);
		}
	}
	Lines.Sort([](const AActor& A, const AActor& B) { return Cast<IPDSkateLine>(&A)->GetLineId().LexicalLess(Cast<IPDSkateLine>(&B)->GetLineId()); });
	SetClosingLamps(0.0f);
}

TArray<AActor*> APDGameMode::GetSkaters() const
{
	return Director ? Director->GetSkaters() : TArray<AActor*>();
}

TArray<AActor*> APDGameMode::GetSkateLines() const
{
	TArray<AActor*> Out;
	for (AActor* A : Lines)
	{
		if (IsValid(A))
		{
			Out.Add(A);
		}
	}
	return Out;
}

TArray<AActor*> APDGameMode::GetProps() const
{
	TArray<AActor*> Out;
	for (AActor* A : Props)
	{
		if (IsValid(A))
		{
			Out.Add(A);
		}
	}
	return Out;
}

float APDGameMode::GetTimeRemaining() const
{
	return Config ? FMath::Max(0.0f, Config->RoundSeconds - Elapsed) : 0.0f;
}

float APDGameMode::GetWearLimit() const
{
	return Config ? Config->WearLimit : 100.0f;
}

const FPDPhaseRow& APDGameMode::GetPhase() const
{
	static const FPDPhaseRow Fallback;
	return (Config && Config->Phases.IsValidIndex(PhaseIndex)) ? Config->Phases[PhaseIndex] : Fallback;
}

void APDGameMode::SetPlayerInputLocked(bool bLocked)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetIgnoreMoveInput(bLocked);
	}
}

void APDGameMode::SetupMatch()
{
	bSetupDone = true;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	Guard = PC ? PC->GetPawn() : nullptr;
	if (!Guard)
	{
		return;
	}
	KnobsLeft = Config->SkatestopperCount;
	if (bShowcase)
	{
		ApplyShowcaseTiming();
	}
	Director->Init(Config, RunSeed);
	Director->SpawnOpeningCrew(StartSkaters);

	State = EPDMatchState::Ready;
	CountdownLeft = static_cast<float>(Config->CountdownSeconds);
	LastCountdownSecond = Config->CountdownSeconds;
	SetPlayerInputLocked(true);
	Events->OnStateChanged.Broadcast(State);
	Events->OnCountdown.Broadcast(LastCountdownSecond);
	Events->OnWearChanged.Broadcast(Wear, GetWearLimit());
	CheckCommandLineAutoplay();
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Ready: %d knobs, wear limit %.0f, %.0f s to closing."), KnobsLeft, GetWearLimit(), Config->RoundSeconds);
	// Start screen and intro over the live plaza (FactoryKit, design guide 2.5): hold the countdown until PLAY.
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		bFlowHold = true;
		Events->OnHUDVisible.Broadcast(false);
		Flow->OnStartGame.AddUniqueDynamic(this, &APDGameMode::HandleFlowStart);
		Flow->Begin(bAutoplayActive);
	}
}

void APDGameMode::HandleFlowStart()
{
	if (!bFlowHold)
	{
		return;
	}
	bFlowHold = false;
	CountdownLeft = static_cast<float>(Config->CountdownSeconds);
	LastCountdownSecond = Config->CountdownSeconds;
	Events->OnHUDVisible.Broadcast(true);
	Events->OnStateChanged.Broadcast(State);
	Events->OnCountdown.Broadcast(LastCountdownSecond);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Start screen done: countdown."));
}

void APDGameMode::ApplyShowcaseTiming()
{
	// Compress the shift: every authored time (phase starts, line openings) scales with the round.
	const float Scale = FMath::Clamp(ShowcaseShiftSeconds / FMath::Max(1.0f, Config->RoundSeconds), 0.1f, 1.0f);
	Config->RoundSeconds = ShowcaseShiftSeconds;
	ClockScale = FMath::Max(1.0f, ShowcaseClockSeconds) / FMath::Max(1.0f, ShowcaseShiftSeconds);
	for (FPDPhaseRow& P : Config->Phases)
	{
		P.StartTime *= Scale;
	}
	for (AActor* L : Lines)
	{
		if (IPDSkateLine* Line = Cast<IPDSkateLine>(L))
		{
			Line->ScaleOpeningTime(Scale);
		}
	}
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Showcase: %.0f s shift (time scale %.2f), clock shows %.0f s."), ShowcaseShiftSeconds, Scale, ShowcaseClockSeconds);
}

AActor* APDGameMode::MostWornLine() const
{
	AActor* Best = nullptr;
	float BestWear = -1.0f;
	for (AActor* L : Lines)
	{
		const IPDSkateLine* Line = Cast<IPDSkateLine>(L);
		const float* W = Line ? LineWear.Find(Line->GetLineId()) : nullptr;
		if (W && *W > BestWear)
		{
			BestWear = *W;
			Best = L;
		}
	}
	return Best;
}

void APDGameMode::BeginActive()
{
	State = EPDMatchState::Active;
	Elapsed = 0.0f;
	PhaseIndex = 0;
	SetPlayerInputLocked(false);
	Events->OnCountdown.Broadcast(0);
	Events->OnStateChanged.Broadcast(State);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] GO: shift started."));
}

void APDGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPerf)
	{
		const float Ms = FApp::GetDeltaTime() * 1000.0f;
		PerfFrames.Add(Ms);
		PerfClock += FApp::GetDeltaTime();
		if (PerfClock >= 10.0f && PerfFrames.Num() > 0)
		{
			PerfFrames.Sort();
			float Sum = 0.0f;
			for (float F : PerfFrames) { Sum += F; }
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] Perf: %.1f fps avg, p95 %.1f ms, worst %.1f ms over %d frames."), 1000.0f * PerfFrames.Num() / Sum,
				PerfFrames[FMath::Min(PerfFrames.Num() - 1, PerfFrames.Num() * 95 / 100)], PerfFrames.Last(), PerfFrames.Num());
			PerfFrames.Reset();
			PerfClock = 0.0f;
		}
	}
	if (!bSetupDone)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->GetPawn())
		{
			SetupMatch();
		}
		return;
	}
	UpdatePortraitCamera();
	if (State == EPDMatchState::Ready)
	{
		if (bFlowHold)
		{
			return;  // start screen / intro (FactoryKit)
		}
		CountdownLeft -= DeltaSeconds;
		const int32 Second = FMath::CeilToInt(CountdownLeft);
		if (Second != LastCountdownSecond && Second > 0)
		{
			LastCountdownSecond = Second;
			Events->OnCountdown.Broadcast(Second);
		}
		if (CountdownLeft <= 0.0f)
		{
			BeginActive();
		}
		return;
	}
	if (State != EPDMatchState::Active)
	{
		SetClosingLamps(FMath::Min(1.0f, LampAlpha + DeltaSeconds));
		return;
	}

	Elapsed += DeltaSeconds;
	Stats.Elapsed = Elapsed;
	const int32 NewPhase = Config->GetPhaseIndex(Elapsed);
	if (NewPhase != PhaseIndex)
	{
		PhaseIndex = NewPhase;
		Events->OnPhaseChanged.Broadcast(PhaseIndex, GetPhase());
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f phase %s (target %d skaters)."), Elapsed, *GetPhase().Name.ToString(), GetPhase().SkaterTarget);
	}
	Director->TickDirector(DeltaSeconds, Elapsed, GetPhase());
	Stats.PeakSkaters = FMath::Max(Stats.PeakSkaters, Director->GetActiveCount());

	// Wear trend over the last few seconds.
	WearHistory.Add({ Elapsed, Wear });
	while (WearHistory.Num() > 1 && Elapsed - WearHistory[0].Key > WearRateWindow)
	{
		WearHistory.RemoveAt(0);
	}
	const float Span = WearHistory.Num() > 1 ? Elapsed - WearHistory[0].Key : 0.0f;
	WearRate = Span > 0.5f ? (Wear - WearHistory[0].Value) / Span : 0.0f;
	Stats.PeakWearRate = FMath::Max(Stats.PeakWearRate, WearRate);

	const float Remaining = GetTimeRemaining();
	// The closing warning follows the clock players see.
	const float ClockLeft = GetClockRemaining();
	if (!bClosingWarned && ClockLeft <= Config->ClosingWarningSeconds)
	{
		bClosingWarned = true;
		Events->OnClosingSoon.Broadcast();
	}
	if (ClockLeft <= Config->ClosingWarningSeconds)
	{
		SetClosingLamps(1.0f - ClockLeft / FMath::Max(1.0f, Config->ClosingWarningSeconds));
	}
	if (Remaining <= 0.0f)
	{
		EndMatch(true, FText::Format(LOCTEXT("VictoryReason", "Closing time. The plaza held at {0} wear."), FText::AsNumber(FMath::FloorToInt(Wear))));
	}
}

void APDGameMode::ReportTrick(const FPDTrickEvent& Trick)
{
	if (State != EPDMatchState::Active)
	{
		return;
	}
	Wear = FMath::Min(GetWearLimit(), Wear + Trick.Wear);
	LineWear.FindOrAdd(Trick.LineId) += Trick.Wear;
	++Stats.Tricks;
	if (Trick.bOnProp)
	{
		++Stats.PropTricks;
	}
	Events->OnTrickLanded.Broadcast(Trick);
	Events->OnWearChanged.Broadcast(Wear, GetWearLimit());
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f wear=%.0f line=%s trick=%s +%.0f%s"), Elapsed, Wear, *Trick.LineId.ToString(), *Trick.TrickName.ToString(), Trick.Wear, Trick.bOnProp ? TEXT(" (prop)") : TEXT(""));
	if (AActor* Friend = Director->HandleTrick(Trick, Elapsed, GetPhase()))
	{
		++Stats.Friends;
		Events->OnFriendArrived.Broadcast(Friend);
		ReportPrompt(EPDPrompt::Friend, Friend);
	}
	if (Wear >= GetWearLimit())
	{
		EndMatch(false, FText::Format(LOCTEXT("DefeatReason", "The plaza wore out with {0} to closing."), FText::FromString(FormatClock(GetClockRemaining()))));
	}
}

bool APDGameMode::ConsumeKnob()
{
	if (KnobsLeft <= 0 || State != EPDMatchState::Active)
	{
		return false;
	}
	--KnobsLeft;
	return true;
}

void APDGameMode::ReportLineCapped(AActor* Line)
{
	++Stats.KnobsBolted;
	Events->OnLineCapped.Broadcast(Line, KnobsLeft);
	ReportPrompt(EPDPrompt::Capped, Line);
}

void APDGameMode::ReportPropConverted(AActor* Line, AActor* Prop, const FText& TrickName)
{
	++Stats.PropsConverted;
	Events->OnPropConverted.Broadcast(Line, Prop, TrickName);
	ReportPrompt(EPDPrompt::Converted, Prop);
	if (bShowcase && Line)
	{
		// Showcase: the nearest free skaters try the new trick at once, so the video shows the launch and the friend it calls.
		TArray<AActor*> Near = GetSkaters();
		const FVector At = Prop ? Prop->GetActorLocation() : Line->GetActorLocation();
		Near.Sort([At](const AActor& A, const AActor& B) { return FVector::DistSquared(A.GetActorLocation(), At) < FVector::DistSquared(B.GetActorLocation(), At); });
		int32 Sent = 0;
		for (AActor* S : Near)
		{
			IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(S);
			if (Agent && !Agent->IsCommitted() && Sent < ShowcaseNudgedSkaters)
			{
				Agent->PreferLine(Line);
				++Sent;
			}
		}
	}
}

void APDGameMode::ReportPropMoved(AActor* Prop, bool bOnLine)
{
	if (bOnLine)
	{
		++Stats.PropsPlaced;
	}
	else
	{
		++Stats.PropsCleared;
	}
	Events->OnPropMoved.Broadcast(Prop, bOnLine);
}

void APDGameMode::ReportWhistle(AActor* InGuard, int32 Scattered)
{
	++Stats.Whistles;
	Events->OnWhistle.Broadcast(InGuard, Scattered);
}

void APDGameMode::ReportKnockdown(AActor* InGuard, AActor* Skater)
{
	++Stats.Knockdowns;
	Events->OnKnockdown.Broadcast(InGuard, Skater);
	ReportPrompt(EPDPrompt::Knockdown, InGuard);
}

void APDGameMode::ReportPrompt(EPDPrompt Prompt, AActor* Anchor)
{
	if (State != EPDMatchState::Active || PromptsShown.Contains(Prompt))
	{
		return;
	}
	PromptsShown.Add(Prompt);
	Events->OnPrompt.Broadcast(Prompt, Anchor);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f first-time prompt %d at %s."), Elapsed, static_cast<int32>(Prompt), *GetNameSafe(Anchor));
}

void APDGameMode::SetClosingLamps(float Alpha)
{
	LampAlpha = Alpha;
	for (AActor* A : ClosingLamps)
	{
		if (ALight* L = Cast<ALight>(A))
		{
			L->GetLightComponent()->SetIntensity(FMath::Lerp(ClosingLampBaseIntensity, ClosingLampIntensity, Alpha));
		}
	}
}

FString APDGameMode::FormatClock(float Seconds) const
{
	const int32 Whole = FMath::CeilToInt(Seconds);
	return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
}

void APDGameMode::EndMatch(bool bVictory, const FText& Reason)
{
	if (State == EPDMatchState::Victory || State == EPDMatchState::Defeat)
	{
		return;
	}
	State = bVictory ? EPDMatchState::Victory : EPDMatchState::Defeat;
	EndReason = Reason;
	Stats.FinalWear = Wear;
	SetPlayerInputLocked(true);
	Director->SendEveryoneHome();
	Events->OnStateChanged.Broadcast(State);
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Match ended: %s - %s (%.1fs, wear %.0f, tricks %d, knobs %d, whistles %d, props placed %d / converted %d / cleared %d, friends %d, knockdowns %d)"),
		bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"), *Reason.ToString(), Elapsed, Wear, Stats.Tricks, Stats.KnobsBolted, Stats.Whistles,
		Stats.PropsPlaced, Stats.PropsConverted, Stats.PropsCleared, Stats.Friends, Stats.Knockdowns);
	WriteTelemetry(bVictory);
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		// Victory: orbit on the guard. Defeat: push in on the line that wore the plaza out.
		AActor* Focus = bVictory ? Guard.Get() : MostWornLine();
		Flow->PlayEnding(bVictory, Focus ? Focus : Guard.Get());
	}
	if (bAutoplayActive)
	{
		GetWorldTimerManager().SetTimer(AfterMatchTimer, this, &APDGameMode::AfterMatch, FMath::Max(1.0f, AutoplayRestartDelay), false);
	}
}

void APDGameMode::AfterMatch()
{
	++PDModeStatics::RunsCompleted;
	int32 MaxRuns = 0;
	const TCHAR* Cmd = FCommandLine::Get();
	const bool bHasRuns = FParse::Value(Cmd, TEXT("PDRuns="), MaxRuns);
	const bool bExit = FParse::Param(Cmd, TEXT("autoplayexit")) || FParse::Param(Cmd, TEXT("PDExit"));
	if ((bExit && !bHasRuns) || (bHasRuns && PDModeStatics::RunsCompleted >= MaxRuns))
	{
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] Autoplay finished %d run(s); quitting."), PDModeStatics::RunsCompleted);
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		return;
	}
	RequestRestart();
}

void APDGameMode::UpdatePortraitCamera()
{
	// QA: -PDCam=X,Y,Z,Pitch,Yaw holds a fixed camera on a set piece (sitters, cafe, stencil) for review shots.
	FString CamSpec;
	if (PortraitIndex < -1 && FParse::Value(FCommandLine::Get(), TEXT("PDCam="), CamSpec, false))
	{
		TArray<FString> P;
		CamSpec.ParseIntoArray(P, TEXT(","));
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (P.Num() == 5 && PC)
		{
			if (!PortraitCamera)
			{
				PortraitCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
				PortraitCamera->GetCameraComponent()->SetFieldOfView(50.0f);
			}
			PortraitCamera->SetActorLocationAndRotation(FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])),
				FRotator(FCString::Atof(*P[3]), FCString::Atof(*P[4]), 0.0f));
			if (PC->GetViewTarget() != PortraitCamera)
			{
				PC->SetViewTarget(PortraitCamera);
			}
		}
		return;
	}
	if (PortraitIndex < -1)
	{
		return;
	}
	AActor* Subject = Guard;
	if (PortraitIndex >= 0)
	{
		const TArray<AActor*> Skaters = GetSkaters();
		Subject = Skaters.IsValidIndex(PortraitIndex) ? Skaters[PortraitIndex] : nullptr;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Subject || !PC)
	{
		return;
	}
	if (!PortraitCamera)
	{
		PortraitCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		PortraitCamera->GetCameraComponent()->SetFieldOfView(45.0f);
		PC->SetViewTarget(PortraitCamera);
	}
	const FRotator Facing(0.0f, Subject->GetActorRotation().Yaw + 90.0f + PortraitYaw, 0.0f);
	const FVector Focus = Subject->GetActorLocation() + FVector(0.0f, 0.0f, PortraitIndex >= 0 ? 80.0f : 0.0f);
	const FVector Eye = Focus - Facing.Vector() * 520.0f + FVector(0.0f, 0.0f, 110.0f);
	PortraitCamera->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
}

void APDGameMode::RequestRestart()
{
	UE_LOG(LogPlaza, Display, TEXT("[Plaza] Restarting the shift."));
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->MarkRestart();  // R restarts at the countdown: no menu, no intro replay
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void APDGameMode::WriteTelemetry(bool bVictory) const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("result"), bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"));
	J->SetStringField(TEXT("reason"), EndReason.ToString());
	J->SetNumberField(TEXT("seed"), RunSeed);
	J->SetBoolField(TEXT("autoplay"), bAutoplayActive);
	J->SetBoolField(TEXT("defeat_mode"), bDefeatMode);
	J->SetNumberField(TEXT("duration"), Elapsed);
	J->SetNumberField(TEXT("wear"), Wear);
	J->SetNumberField(TEXT("tricks"), Stats.Tricks);
	J->SetNumberField(TEXT("prop_tricks"), Stats.PropTricks);
	J->SetNumberField(TEXT("knobs"), Stats.KnobsBolted);
	J->SetNumberField(TEXT("whistles"), Stats.Whistles);
	J->SetNumberField(TEXT("props_placed"), Stats.PropsPlaced);
	J->SetNumberField(TEXT("props_converted"), Stats.PropsConverted);
	J->SetNumberField(TEXT("props_cleared"), Stats.PropsCleared);
	J->SetNumberField(TEXT("friends"), Stats.Friends);
	J->SetNumberField(TEXT("knockdowns"), Stats.Knockdowns);
	J->SetNumberField(TEXT("peak_skaters"), Stats.PeakSkaters);
	J->SetNumberField(TEXT("peak_wear_rate"), Stats.PeakWearRate);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(J, Writer);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("match_%s_s%d_r%d.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), RunSeed, PDModeStatics::RunsCompleted);
	FFileHelper::SaveStringToFile(Out, *Path);
}

#undef LOCTEXT_NAMESPACE
