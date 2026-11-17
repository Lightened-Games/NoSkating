// Copyright Plaza Duty. All Rights Reserved.

#include "Interfaces/PDInterfaces.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"

IPDMatchFlow* PD::GetFlow(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? Cast<IPDMatchFlow>(World->GetAuthGameMode()) : nullptr;
}
