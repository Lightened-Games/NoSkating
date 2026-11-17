// Copyright Plaza Duty. All Rights Reserved.

#include "Feedback/PDPuffFX.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

APDPuffFX::APDPuffFX()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetCanBeDamaged(false);
	QuadMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Meshes/SM_PD_Quad.SM_PD_Quad")));
	PuffMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_Puff.MI_PD_Puff")));
	RingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Meshes/SM_PD_Ring.SM_PD_Ring")));
	RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/PlazaDuty/Materials/MI_PD_PuffRing.MI_PD_PuffRing")));
}

APDPuffFX* APDPuffFX::Spawn(UWorld* World, const FVector& Location, const FPDPuffStyle& Style, int32 Seed)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APDPuffFX* Fx = World->SpawnActor<APDPuffFX>(APDPuffFX::StaticClass(), FTransform(Location), Params);
	if (Fx)
	{
		Fx->Init(Style, Seed);
	}
	return Fx;
}

void APDPuffFX::Init(const FPDPuffStyle& InStyle, int32 Seed)
{
	Style = InStyle;
	FRandomStream Random(Seed);
	UStaticMesh* Mesh = Style.bGroundRing ? RingMesh.LoadSynchronous() : QuadMesh.LoadSynchronous();
	UMaterialInterface* Mat = Style.bGroundRing ? RingMaterial.LoadSynchronous() : PuffMaterial.LoadSynchronous();
	if (!Mesh)
	{
		Destroy();
		return;
	}
	MeshSize = FMath::Max(1.0f, Mesh->GetBounds().BoxExtent.X * 2.0f);
	const int32 Count = Style.bGroundRing ? 1 : Style.Count;
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(this);
		Quad->SetupAttachment(Root);
		Quad->SetStaticMesh(Mesh);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetCanEverAffectNavigation(false);
		Quad->SetUsingAbsoluteRotation(true);
		Quad->RegisterComponent();
		UMaterialInstanceDynamic* Mid = Mat ? Quad->CreateDynamicMaterialInstance(0, Mat) : nullptr;
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Style.Color);
			Mid->SetScalarParameterValue(TEXT("Glow"), Style.Glow);
			Mid->SetScalarParameterValue(TEXT("Opacity"), Style.Opacity);
		}
		FParticle& P = Particles.AddDefaulted_GetRef();
		P.Quad = Quad;
		P.Mid = Mid;
		const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
		const float Speed = Style.Spread * Random.FRandRange(0.5f, 1.0f);
		P.Velocity = FVector(FMath::Cos(Angle) * Speed, FMath::Sin(Angle) * Speed, Style.Rise * Random.FRandRange(0.6f, 1.2f));
		P.Spin = Random.FRandRange(-90.0f, 90.0f);
		P.SizeScale = Random.FRandRange(0.7f, 1.2f);
		Quad->SetRelativeLocation(FVector(Random.FRandRange(-6.0f, 6.0f), Random.FRandRange(-6.0f, 6.0f), Random.FRandRange(0.0f, 8.0f)));
		Quads.Add(Quad);
		Mids.Add(Mid);
	}
	SetLifeSpan(Style.Life + 0.1f);
}

void APDPuffFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / FMath::Max(0.05f, Style.Life), 0.0f, 1.0f);
	const float Ease = 1.0f - FMath::Square(1.0f - T);
	const float Size = FMath::Lerp(Style.StartSize, Style.EndSize, Ease);
	const float Fade = FMath::Clamp((1.0f - T) * 2.5f, 0.0f, 1.0f) * FMath::Clamp(Age / 0.05f, 0.0f, 1.0f);

	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FRotator Facing = Cam ? (Cam->GetCameraLocation() - GetActorLocation()).Rotation() : FRotator::ZeroRotator;

	for (FParticle& P : Particles)
	{
		if (!P.Quad)
		{
			continue;
		}
		P.Velocity.Z -= Style.Gravity * DeltaSeconds;
		P.Velocity *= FMath::Max(0.0f, 1.0f - 2.2f * DeltaSeconds);
		P.Quad->AddRelativeLocation(P.Velocity * DeltaSeconds);
		if (Style.bGroundRing)
		{
			P.Quad->SetWorldRotation(FRotator::ZeroRotator);
			P.Quad->SetWorldScale3D(FVector(Size / MeshSize * 2.0f, Size / MeshSize * 2.0f, 1.0f));
		}
		else
		{
			// The quad mesh faces +X: turn it toward the camera, spin about the view axis.
			P.Quad->SetWorldRotation(FRotator(Facing.Pitch, Facing.Yaw, P.Spin * Age));
			P.Quad->SetWorldScale3D(FVector(1.0f, Size * P.SizeScale / MeshSize, Size * P.SizeScale / MeshSize));
		}
		if (P.Mid)
		{
			P.Mid->SetScalarParameterValue(TEXT("Opacity"), Style.Opacity * Fade);
		}
	}
}
