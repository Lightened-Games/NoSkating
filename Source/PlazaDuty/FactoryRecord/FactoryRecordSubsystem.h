// FactoryRecord: drop-in game-audio capture for the auto-recorded submission video (design guide 2.5).
// Copy this folder into Source/<Module>/ and add "AudioMixer" to the module's PrivateDependencyModuleNames.
// Only active with -FactoryRecordAudio=<seconds>. It starts on the first game world (the start screen, which is where the
// video must begin), logs "[FactoryRecord] AudioCaptureStart unix_ms=<ms>" for reference_tools/record_showcase.ps1 to sync
// the screen capture, and writes Saved/Recording/FactoryRecord_Audio.wav when the time is up or the game exits.
// Audio keeps recording across level loads and restarts (the capture lives on the audio device, not the world).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "FactoryRecordSubsystem.generated.h"

UCLASS()
class UFactoryRecordSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void StartOn(UWorld* World);
	bool TickStop(float DeltaTime);
	void Stop();

	float Seconds = 0.0f;
	double StartTime = 0.0;
	bool bStarted = false;
	bool bActive = false;
	TWeakObjectPtr<UWorld> CaptureWorld;
	FTSTicker::FDelegateHandle TickHandle;
};
