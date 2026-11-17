// Copyright Plaza Duty. All Rights Reserved.

#include "World/PDPetalFall.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Data/PDTypes.h"

APDPetalFall::APDPetalFall()
{
	PrimaryActorTick.bCanEverTick = true;
	Petals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Petals"));
	RootComponent = Petals;
	Petals->SetMobility(EComponentMobility::Movable);
	Petals->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Petals->SetCanEverAffectNavigation(false);
	Petals->SetCastShadow(false);
	Petals->ComponentTags.Add(PD::NoCollideTag);
	Tags.Add(PD::NoCollideTag);
	PetalMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Meshes/SM_PD_Quad.SM_PD_Quad")));
	PetalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_Petal.MI_PD_Petal")));
}

void APDPetalFall::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = PetalMesh.LoadSynchronous())
	{
		Petals->SetStaticMesh(M);
	}
	if (UMaterialInterface* Mat = PetalMaterial.LoadSynchronous())
	{
		Petals->SetMaterial(0, Mat);
	}
	State.SetNum(Count);
	for (FPetal& P : State)
	{
		Respawn(P, true);
		Petals->AddInstance(FTransform::Identity, true);
	}
}

void APDPetalFall::Respawn(FPetal& P, bool bAnywhere)
{
	const float A = FMath::FRandRange(0.0f, 2.0f * PI);
	const float R = Radius * FMath::Sqrt(FMath::FRand());
	P.Base = GetActorLocation() + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0f);
	P.Z = bAnywhere ? FMath::FRandRange(0.0f, StartHeight.Y) : FMath::FRandRange(StartHeight.X, StartHeight.Y);
	P.Speed = FMath::FRandRange(FallSpeed.X, FallSpeed.Y);
	P.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
	P.Spin = FMath::FRandRange(-200.0f, 200.0f);
	P.Age = 0.0f;
}

void APDPetalFall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TArray<FTransform> Xf;
	Xf.Reserve(State.Num());
	for (FPetal& P : State)
	{
		P.Age += DeltaSeconds;
		P.Z -= P.Speed * DeltaSeconds;
		if (P.Z <= 1.0f)
		{
			Respawn(P, false);
		}
		const float Sway = FMath::Sin(P.Age * 1.7f + P.Phase) * SwayAmplitude;
		const FVector Loc = P.Base + Breeze * P.Age + FVector(Sway, Sway * 0.6f, P.Z);
		const FRotator Rot(FMath::Sin(P.Age * 2.3f + P.Phase) * 60.0f, P.Spin * P.Age, FMath::Cos(P.Age * 1.9f) * 50.0f);
		Xf.Add(FTransform(Rot, Loc - GetActorLocation(), FVector(PetalScale)));
	}
	Petals->BatchUpdateInstancesTransforms(0, Xf, false, true, true);
}
