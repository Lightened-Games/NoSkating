// FactoryRecord: see header.

#include "FactoryRecordSubsystem.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

bool UFactoryRecordSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	float S = 0.0f;
	return FParse::Value(FCommandLine::Get(), TEXT("FactoryRecordAudio="), S) && S > 0.0f;
}

void UFactoryRecordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("FactoryRecordAudio="), Seconds);
	UE_LOG(LogTemp, Display, TEXT("[FactoryRecord] Armed: game audio will be recorded for %.1f s from the first game world."), Seconds);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFactoryRecordSubsystem::TickStop), 0.25f);
}

void UFactoryRecordSubsystem::Deinitialize()
{
	Stop();
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UFactoryRecordSubsystem::StartOn(UWorld* World)
{
	bStarted = true;
	bActive = true;
	CaptureWorld = World;
	StartTime = FPlatformTime::Seconds();
	UAudioMixerBlueprintLibrary::StartRecordingOutput(World, Seconds);
	const double UnixMs = (FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTotalMilliseconds();
	UE_LOG(LogTemp, Display, TEXT("[FactoryRecord] AudioCaptureStart unix_ms=%.0f seconds=%.1f"), UnixMs, Seconds);
}

bool UFactoryRecordSubsystem::TickStop(float DeltaTime)
{
	// Start on the first game world that has begun play (the start screen).
	if (!bStarted && GetGameInstance())
	{
		UWorld* World = GetGameInstance()->GetWorld();
		if (World && World->IsGameWorld() && World->HasBegunPlay())
		{
			StartOn(World);
		}
	}
	if (bActive && FPlatformTime::Seconds() - StartTime >= Seconds)
	{
		Stop();
	}
	return true;
}

void UFactoryRecordSubsystem::Stop()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	UWorld* World = CaptureWorld.Get();
	if (!World && GetGameInstance()) { World = GetGameInstance()->GetWorld(); }
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryRecord] No world left to stop the audio capture."));
		return;
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Recording"));
	UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, TEXT("FactoryRecord_Audio"), Dir);
	UE_LOG(LogTemp, Display, TEXT("[FactoryRecord] AudioCaptureSaved %s/FactoryRecord_Audio.wav"), *Dir);
}
