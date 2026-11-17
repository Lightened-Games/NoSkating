// Copyright Plaza Duty. All Rights Reserved.

#include "Framework/PDSkaterDirectorComponent.h"
#include "Data/PDMatchConfig.h"
#include "Actors/PDSkaterPawn.h"
#include "Components/PDSkaterBrainComponent.h"
#include "Interfaces/PDInterfaces.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PlazaDuty.h"

UPDSkaterDirectorComponent::UPDSkaterDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SkaterClass = APDSkaterPawn::StaticClass();
}

void UPDSkaterDirectorComponent::Init(const UPDMatchConfig* InConfig, int32 InSeed)
{
	Config = InConfig;
	Seed = InSeed;
	Random.Initialize(InSeed * 31 + 5);
	Entries.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(PD::SkaterEntryTag))
		{
			Entries.Add(It->GetActorTransform());
		}
	}
	Entries.Sort([](const FTransform& A, const FTransform& B) { return A.GetLocation().Y < B.GetLocation().Y; });
	if (Entries.Num() == 0)
	{
		Entries.Add(FTransform(FVector(0.0f, -3200.0f, 0.0f)));
	}
	Bodies.Reset();
	if (Config)
	{
		for (const TSoftObjectPtr<USkeletalMesh>& M : Config->SkaterMeshes)
		{
			if (USkeletalMesh* L = M.LoadSynchronous())
			{
				Bodies.Add(L);
			}
		}
	}
}

AActor* UPDSkaterDirectorComponent::SpawnSkater(bool bFriend)
{
	if (!SkaterClass || (Config && GetActiveCount() >= Config->MaxSkaters))
	{
		return nullptr;
	}
	const FTransform& Entry = Entries[Spawned % Entries.Num()];
	const FVector Loc = Entry.GetLocation() + FVector(Random.FRandRange(-EntrySpread, EntrySpread), Random.FRandRange(-EntrySpread, EntrySpread), SpawnHeight);
	const FTransform Xf(Entry.Rotator(), Loc);
	APawn* Skater = GetWorld()->SpawnActorDeferred<APawn>(SkaterClass, Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Skater)
	{
		return nullptr;
	}
	if (Bodies.Num() > 0)
	{
		if (USkeletalMeshComponent* Body = Skater->FindComponentByClass<USkeletalMeshComponent>())
		{
			Body->SetSkeletalMesh(Bodies[Spawned % Bodies.Num()]);
		}
	}
	Skater->FinishSpawning(Xf);
	++Spawned;
	Skaters.Add(Skater);
	if (UPDSkaterBrainComponent* Brain = Skater->FindComponentByClass<UPDSkaterBrainComponent>())
	{
		Brain->StartSession(Seed * 1000 + Spawned * 97, bFriend);
	}
	return Skater;
}

void UPDSkaterDirectorComponent::SpawnOpeningCrew(int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		SpawnSkater(false);
	}
}

int32 UPDSkaterDirectorComponent::GetActiveCount() const
{
	int32 N = 0;
	for (const AActor* S : Skaters)
	{
		const UPDSkaterBrainComponent* Brain = S ? S->FindComponentByClass<UPDSkaterBrainComponent>() : nullptr;
		if (Brain && Brain->GetMode() != EPDSkaterMode::Gone && Brain->GetMode() != EPDSkaterMode::Home)
		{
			++N;
		}
	}
	return N;
}

TArray<AActor*> UPDSkaterDirectorComponent::GetSkaters() const
{
	TArray<AActor*> Out;
	for (AActor* S : Skaters)
	{
		if (IsValid(S) && !S->IsHidden())
		{
			Out.Add(S);
		}
	}
	return Out;
}

void UPDSkaterDirectorComponent::TickDirector(float DeltaSeconds, float Elapsed, const FPDPhaseRow& Phase)
{
	ArrivalTimer -= DeltaSeconds;
	if (ArrivalTimer <= 0.0f && GetActiveCount() < Phase.SkaterTarget)
	{
		ArrivalTimer = ArrivalInterval;
		if (AActor* S = SpawnSkater(false))
		{
			UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f skater %s arrives (%d on the plaza)."), Elapsed, *S->GetName(), GetActiveCount());
		}
	}
}

AActor* UPDSkaterDirectorComponent::HandleTrick(const FPDTrickEvent& Trick, float Elapsed, const FPDPhaseRow& Phase)
{
	if (!Config || !Trick.bOnProp || Trick.Wear < Config->FriendTriggerWear || Elapsed - LastFriendTime < Phase.FriendCooldown)
	{
		return nullptr;
	}
	AActor* Friend = SpawnSkater(true);
	if (Friend)
	{
		LastFriendTime = Elapsed;
		UE_LOG(LogPlaza, Display, TEXT("[Plaza] t=%.1f %s on a prop called a friend (%d on the plaza)."), Elapsed, *Trick.TrickName.ToString(), GetActiveCount());
	}
	return Friend;
}

void UPDSkaterDirectorComponent::SendEveryoneHome()
{
	for (AActor* S : Skaters)
	{
		if (IPDSkaterAgent* Agent = Cast<IPDSkaterAgent>(S))
		{
			Agent->GoHome();
		}
	}
}
