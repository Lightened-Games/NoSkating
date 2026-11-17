// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "PDCameraShakes.generated.h"

/** Heavy shake: knocked down by a skater. */
UCLASS()
class PLAZADUTY_API UPDImpactShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UPDImpactShake(const FObjectInitializer& ObjectInitializer);
};

/** Micro shake: knob seated, prop dropped, nearby landing. */
UCLASS()
class PLAZADUTY_API UPDMicroShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UPDMicroShake(const FObjectInitializer& ObjectInitializer);
};

/** Low rumble: whistle blast, closing time. */
UCLASS()
class PLAZADUTY_API UPDRumbleShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UPDRumbleShake(const FObjectInitializer& ObjectInitializer);
};
