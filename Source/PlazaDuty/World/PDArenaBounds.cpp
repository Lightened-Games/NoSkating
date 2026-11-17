// Copyright Plaza Duty. All Rights Reserved.

#include "World/PDArenaBounds.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Data/PDTypes.h"

APDArenaBounds::APDArenaBounds()
{
	PrimaryActorTick.bCanEverTick = true;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->SetBoxExtent(FVector(2450.0f, 3250.0f, 900.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APDArenaBounds::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Timer -= DeltaSeconds;
	if (Timer > 0.0f)
	{
		return;
	}
	Timer = CheckInterval;
	const FTransform Xf = Bounds->GetComponentTransform();
	const FVector Extent = Bounds->GetScaledBoxExtent();
	const float FloorZ = Xf.GetLocation().Z - Extent.Z;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (!Pawn->ActorHasTag(PD::GuardTag))
		{
			continue; // skaters ride kinematic routes inside the plaza
		}
		const FVector Local = Xf.InverseTransformPosition(Pawn->GetActorLocation());
		const bool bOutside = FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || Pawn->GetActorLocation().Z < FloorZ - FloorTolerance;
		if (bOutside)
		{
			Pawn->FellOutOfWorld(*GetDefault<UDamageType>());
		}
	}
}
