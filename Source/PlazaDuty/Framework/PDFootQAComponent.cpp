// Copyright Plaza Duty. All Rights Reserved.

#include "Framework/PDFootQAComponent.h"
#include "Player/PDAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Data/PDTypes.h"
#include "PlazaDuty.h"

UPDFootQAComponent::UPDFootQAComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPDFootQAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SampleClock += DeltaTime;
	ReportClock += DeltaTime;
	if (SampleClock >= SampleInterval)
	{
		SampleClock = 0.0f;
		Sample();
	}
	if (ReportClock >= ReportInterval)
	{
		ReportClock = 0.0f;
		Report();
	}
}

void UPDFootQAComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Report();
	Super::EndPlay(Reason);
}

void UPDFootQAComponent::Sample()
{
	UWorld* World = GetWorld();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		const FString Kind = Pawn->ActorHasTag(PD::SkaterTag) ? TEXT("Skater") : TEXT("Guard");
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PDFootQA), false, Pawn);
		auto GroundZ = [&](const FVector& At, float& OutZ)
		{
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 60), At - FVector(0, 0, 400), ECC_Visibility, Params))
			{
				OutZ = Hit.ImpactPoint.Z;
				return true;
			}
			return false;
		};

		TInlineComponentArray<UStaticMeshComponent*> Statics(Pawn);
		TInlineComponentArray<USkeletalMeshComponent*> Skels(Pawn);
		for (USkeletalMeshComponent* Mesh : Skels)
		{
			if (!Mesh->IsVisible() || Mesh->GetBoneIndex(LeftFootBone) == INDEX_NONE)
			{
				continue;
			}
			const FVector L = Mesh->GetBoneLocation(LeftFootBone);
			const FVector R = Mesh->GetBoneLocation(RightFootBone);
			const FVector Low = L.Z <= R.Z ? L : R;
			// Support: a visible static mesh of the pawn under the foot (the deck), else the world floor.
			float Support = 0.0f;
			bool bHasSupport = false;
			for (UStaticMeshComponent* SM : Statics)
			{
				const FBox Box = SM->Bounds.GetBox();
				if (SM->IsVisible() && SM->GetStaticMesh() && Box.IsInsideXY(FVector(Low.X, Low.Y, 0.0)))
				{
					Support = bHasSupport ? FMath::Max(Support, Box.Max.Z) : Box.Max.Z;
					bHasSupport = true;
				}
			}
			if (!bHasSupport && !GroundZ(Low, Support))
			{
				continue;
			}
			const UPDAnimInstance* Anim = Cast<UPDAnimInstance>(Mesh->GetAnimInstance());
			const FName Action = Anim ? Anim->GetAction() : NAME_None;
			const float Speed = Pawn->GetVelocity().Size2D();
			const FString Key = FString::Printf(TEXT("%s sole gap, %s%s"), *Kind, *(Action.IsNone() ? FString(TEXT("Locomotion")) : Action.ToString()),
				Action.IsNone() ? (Speed < 10.0f ? TEXT(" idle") : TEXT(" moving")) : TEXT(""));
			Gaps.FindOrAdd(Key).Add(Low.Z - AnkleHeight - Support);
			// Horizontal fit: where the feet sit on the pawn's largest static mesh (the board), in that mesh's own axes.
			UStaticMeshComponent* Deck = nullptr;
			for (UStaticMeshComponent* SM : Statics)
			{
				if (SM->IsVisible() && SM->GetStaticMesh() && (!Deck || SM->Bounds.SphereRadius > Deck->Bounds.SphereRadius))
				{
					Deck = SM;
				}
			}
			if (Deck)
			{
				const FVector Centre = Deck->GetComponentTransform().TransformPosition(Deck->GetStaticMesh()->GetBounds().Origin);
				const FVector LocalL = Deck->GetComponentTransform().InverseTransformPositionNoScale(L) - Deck->GetComponentTransform().InverseTransformPositionNoScale(Centre);
				const FVector LocalR = Deck->GetComponentTransform().InverseTransformPositionNoScale(R) - Deck->GetComponentTransform().InverseTransformPositionNoScale(Centre);
				Gaps.FindOrAdd(FString::Printf(TEXT("%s feet on board, mesh X (L/R)"), *Kind)).Add(LocalL.X);
				Gaps.FindOrAdd(FString::Printf(TEXT("%s feet on board, mesh X (L/R)"), *Kind)).Add(LocalR.X);
				Gaps.FindOrAdd(FString::Printf(TEXT("%s feet on board, mesh Y (L/R)"), *Kind)).Add(LocalL.Y);
				Gaps.FindOrAdd(FString::Printf(TEXT("%s feet on board, mesh Y (L/R)"), *Kind)).Add(LocalR.Y);
				const FVector Ext = Deck->GetStaticMesh()->GetBounds().BoxExtent * Deck->GetComponentScale();
				Gaps.FindOrAdd(FString::Printf(TEXT("%s board half extents X / Y"), *Kind)).Add(Ext.X);
				Gaps.FindOrAdd(FString::Printf(TEXT("%s board half extents X / Y"), *Kind)).Add(Ext.Y);
			}
		}
		for (UStaticMeshComponent* SM : Statics)
		{
			if (!SM->IsVisible() || !SM->GetStaticMesh())
			{
				continue;
			}
			const FBox Box = SM->Bounds.GetBox();
			float Ground = 0.0f;
			if (GroundZ(Box.GetCenter(), Ground))
			{
				Gaps.FindOrAdd(FString::Printf(TEXT("%s board/mesh bottom above ground"), *Kind)).Add(Box.Min.Z - Ground);
			}
		}
	}
}

void UPDFootQAComponent::Report()
{
	for (TPair<FString, TArray<float>>& P : Gaps)
	{
		TArray<float>& V = P.Value;
		if (V.Num() == 0)
		{
			continue;
		}
		V.Sort();
		auto Pct = [&V](float F) { return V[FMath::Clamp(FMath::FloorToInt(F * (V.Num() - 1)), 0, V.Num() - 1)]; };
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] FootQA %-40s n=%4d min=%6.1f p10=%6.1f median=%6.1f p90=%6.1f max=%6.1f"),
			*P.Key, V.Num(), V[0], Pct(0.1f), Pct(0.5f), Pct(0.9f), V.Last());
	}
	Gaps.Reset();
}
