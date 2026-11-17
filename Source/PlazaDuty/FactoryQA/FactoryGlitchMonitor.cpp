// FactoryQA glitch log. See header.

#include "FactoryGlitchMonitor.h"
#include "FactoryQAPaths.h"
#include "FactoryQASubsystem.h"
#include "ShaderCompiler.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "UnrealClient.h"

// ---------------- log scanner ----------------
class FFactoryLogScanner : public FOutputDevice
{
public:
	TArray<FString> Ignore;
	FCriticalSection Lock;
	TMap<FString, int32> Errors, Warnings;
	TAtomic<bool> bArmed { false };

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		if (!bArmed || Level > ELogVerbosity::Warning || !V)
		{
			return;
		}
		const FString Msg(V);
		for (const FString& I : Ignore)
		{
			if (Msg.Contains(I)) { return; }
		}
		const FString Key = Category.ToString() + TEXT(": ") + Msg.Left(160);
		FScopeLock L(&Lock);
		(Level <= ELogVerbosity::Error ? Errors : Warnings).FindOrAdd(Key)++;
	}
};

namespace FactoryGlitch
{
	static FString Esc(const FString& S) { return S.Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("\""), TEXT("'")); }
}

bool UFactoryGlitchMonitor::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World || !(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("FactoryQA")) || FParse::Param(FCommandLine::Get(), TEXT("GlitchLog"));
}

void UFactoryGlitchMonitor::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LogScanner = MakeShared<FFactoryLogScanner>();
	LogScanner->Ignore = LogIgnore;
	GLog->AddOutputDevice(LogScanner.Get());
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Glitch log active -> %s"), *FactoryQAPaths::OutDir());
}

void UFactoryGlitchMonitor::Deinitialize()
{
	if (Elapsed > 1.0f)
	{
		WriteReport();
	}
	if (LogScanner.IsValid())
	{
		GLog->RemoveOutputDevice(LogScanner.Get());
		LogScanner.Reset();
	}
	Super::Deinitialize();
}

TStatId UFactoryGlitchMonitor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFactoryGlitchMonitor, STATGROUP_Tickables);
}

float UFactoryGlitchMonitor::PlayerFeetZ() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (const ACharacter* C = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr)
	{
		return C->GetActorLocation().Z - C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return TNumericLimits<float>::Lowest();
}

void UFactoryGlitchMonitor::Report(const FString& Kind, const FString& Subject, const FString& Detail, const FVector& Where, float Radius, const UPrimitiveComponent* Comp)
{
	FGlitch& G = Glitches.FindOrAdd(Kind + TEXT("|") + Subject);
	if (G.Count++ > 0)
	{
		return;
	}
	G.Kind = Kind; G.Subject = Subject; G.Detail = Detail; G.FirstTime = Elapsed; G.Where = Where;
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Glitch %s: %s - %s at %s (t=%.1fs)"), *Kind, *Subject, *Detail, *Where.ToCompactString(), Elapsed);
	int32& Shots = ShotsTaken.FindOrAdd(Kind);
	if (Shots < MaxShotsPerKind && FApp::CanEverRender())
	{
		++Shots;
		const FString File = FPaths::Combine(FactoryQAPaths::OutDir(), FString::Printf(TEXT("glitch_%s_%d.png"), *Kind, Shots));
		// A close-up of the subject when the QA subsystem is present (several per frame are fine); otherwise one full screenshot per frame.
		UFactoryQASubsystem* QA = GetWorld()->GetSubsystem<UFactoryQASubsystem>();
		if (!(QA && Kind != TEXT("camera-clip") && QA->CaptureCloseup(Where, Radius, File, Comp)))
		{
			PendingShots.Add(File);
		}
	}
}

void UFactoryGlitchMonitor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;
	SinceReport += DeltaTime;
	if (Elapsed < WarmupSeconds)
	{
		return;
	}
	if (LogScanner.IsValid()) { LogScanner->bArmed = true; }
	if (PendingShots.Num() > 0)
	{
		FScreenshotRequest::RequestScreenshot(PendingShots[0], true, false); // one per frame, or they overwrite each other
		PendingShots.RemoveAt(0);
	}

	const float RealMs = FApp::GetDeltaTime() * 1000.0f;
	const bool bCompiling = GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
	// QA's own captures (leg shots, close-ups, screenshots) stall the GPU: skip this frame and the next two.
	const UFactoryQASubsystem* QAForHitch = GetWorld()->GetSubsystem<UFactoryQASubsystem>();
	const int32 QAShots = QAForHitch ? QAForHitch->GetShotCount() : 0;
	if (QAShots != LastQAShots || PendingShots.Num() > 0) { LastQAShots = QAShots; QuietFrames = 3; }
	const bool bOwnCapture = QuietFrames-- > 0;
	if (RealMs > HitchMs && !bCompiling && !bOwnCapture) // shader compiles on a first run are not gameplay hitches
	{
		Hitches.Add(RealMs);
		if (Hitches.Num() <= 5)
		{
			UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Glitch hitch: %.0f ms frame (t=%.1fs)"), RealMs, Elapsed);
		}
	}
	if (GetWorld()->IsPaused())
	{
		return; // menus and pauses: nothing moves, nothing to judge
	}
	CheckRefPose();
	CheckJitter(DeltaTime);
	CheckCamera();
	CheckCharacters(DeltaTime);
	SinceFloatCheck += DeltaTime;
	if (!bFloatChecked || SinceFloatCheck >= FloatCheckInterval)
	{
		bFloatChecked = true;
		SinceFloatCheck = 0.0f;
		CheckFloating();
	}
	if (SinceReport >= 15.0f)
	{
		SinceReport = 0.0f;
		WriteReport();
	}
}

void UFactoryGlitchMonitor::CheckRefPose()
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		USkeletalMeshComponent* Mesh = It->GetMesh();
		if (!Mesh || !Mesh->IsVisible() || It->IsHidden() || !Mesh->GetSkinnedAsset() || !Mesh->WasRecentlyRendered(0.2f))
		{
			continue;
		}
		const TArray<FTransform> Local = Mesh->GetBoneSpaceTransforms();
		const TArray<FTransform>& Ref = Mesh->GetSkinnedAsset()->GetRefSkeleton().GetRefBonePose();
		if (Local.Num() < 8 || Local.Num() != Ref.Num())
		{
			continue; // follower / leader-pose meshes have no own pose
		}
		double Sum = 0.0;
		for (int32 i = 1; i < Local.Num(); ++i)
		{
			Sum += FMath::RadiansToDegrees(Local[i].GetRotation().AngularDistance(Ref[i].GetRotation()));
		}
		const double Mean = Sum / (Local.Num() - 1);
		if (Mean < RefPoseMaxDeg)
		{
			Report(TEXT("ref-pose"), It->GetName(), FString::Printf(TEXT("mesh in its bind pose (mean bone offset %.2f deg)"), Mean), It->GetActorLocation(), 120.0f, nullptr);
		}
	}
}

void UFactoryGlitchMonitor::CheckFloating()
{
	UWorld* World = GetWorld();
	const float FeetZ = PlayerFeetZ();
	VisualBounds.Reset();
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* M = *It;
		if (M->GetWorld() == World && M->IsRegistered() && M->IsVisible() && !M->IsA<UInstancedStaticMeshComponent>())
		{
			VisualBounds.Add(M, M->Bounds.GetBox());
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A) || A->IsHidden() || A->IsA<APawn>() || A->ActorHasTag(NoCollideTag))
		{
			continue;
		}
		if (const AActor* Parent = A->GetAttachParentActor(); Parent && Parent->IsA<APawn>())
		{
			continue;
		}
		if (const AActor* Holder = A->GetOwner(); Holder && Holder->IsA<APawn>())
		{
			continue;  // carried or dragged by a character right now (PlazaDuty D32; upstream to reference_tools)
		}
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps)
		{
			if (!C || C->IsA<UInstancedStaticMeshComponent>() || !C->IsVisible() || C->bHiddenInGame || !C->GetStaticMesh() || C->ComponentHasTag(NoCollideTag)
				|| (bFloatShadowCastersOnly && !C->CastShadow))
			{
				continue;
			}
			if (C->GetAttachParent() && C->GetAttachParent()->IsSimulatingPhysics())
			{
				continue; // carried by a physics body
			}
			const FBox B = C->Bounds.GetBox();
			const FVector Size = B.GetSize();
			if (Size.GetMax() < FloatMinSizeCm || Size.X > FloatMaxSizeCm || Size.Y > FloatMaxSizeCm || Size.GetMin() < 1.0f)
			{
				continue;
			}
			if (FeetZ > TNumericLimits<float>::Lowest() && B.Min.Z > FeetZ + FloatMaxHeightCm)
			{
				continue; // lamps, signs, shelves above head height
			}
			// Supported if a trace under any of five bottom points hits within FloatGapCm, or anything touches the box.
			FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryGlitchFloat), false);
			Q.AddIgnoredComponent(C);
			float BestGap = TNumericLimits<float>::Max();
			const FVector Ctr = B.GetCenter();
			const FVector Ext = B.GetExtent() * 0.6f;
			const FVector Pts[] = { Ctr, Ctr + FVector(Ext.X, Ext.Y, 0), Ctr + FVector(-Ext.X, Ext.Y, 0), Ctr + FVector(Ext.X, -Ext.Y, 0), Ctr + FVector(-Ext.X, -Ext.Y, 0) };
			for (const FVector& P : Pts)
			{
				FHitResult Hit;
				const FVector Start(P.X, P.Y, B.Min.Z + 2.0f);
				if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 100.0f), ECC_Visibility, Q))
				{
					BestGap = FMath::Min(BestGap, B.Min.Z - Hit.ImpactPoint.Z);
				}
			}
			if (BestGap <= FloatGapCm)
			{
				continue;
			}
			TArray<FOverlapResult> Touch;
			FCollisionObjectQueryParams Obj(FCollisionObjectQueryParams::AllStaticObjects);
			Obj.AddObjectTypesToQuery(ECC_WorldDynamic);
			Obj.AddObjectTypesToQuery(ECC_PhysicsBody);
			World->OverlapMultiByObjectType(Touch, Ctr, FQuat::Identity, Obj, FCollisionShape::MakeBox(B.GetExtent() + FVector(2.0f)), Q);
			bool bTouching = false;
			for (const FOverlapResult& O : Touch)
			{
				if (O.GetComponent() && O.GetComponent() != C && !(O.GetActor() && O.GetActor()->IsA<APawn>())) { bTouching = true; break; }
			}
			if (!bTouching)
			{
				// Visual support: another visible mesh's bounds touching this one (a bracket, a bunk rail, a counter without collision).
				const FBox Grown = B.ExpandBy(2.0f);
				for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FBox>& V : VisualBounds)
				{
					if (V.Key.Get() != C && V.Value.Intersect(Grown)) { bTouching = true; break; }
				}
				if (bTouching)
				{
					Report(TEXT("support-gap"), A->GetName() + TEXT(".") + C->GetName(),
						FString::Printf(TEXT("%s: sits on or against a mesh whose collision is %.1f cm away (resting on a non-colliding prop?)"), *C->GetStaticMesh()->GetName(),
							BestGap < TNumericLimits<float>::Max() ? BestGap : 100.0f), Ctr, B.GetExtent().Size(), C);
					continue;
				}
			}
			if (!bTouching)
			{
				Report(TEXT("floating"), A->GetName() + TEXT(".") + C->GetName(),
					FString::Printf(TEXT("%s: nothing under it within %.0f cm (gap %s) and nothing touching it"), *C->GetStaticMesh()->GetName(), FloatGapCm,
						BestGap < TNumericLimits<float>::Max() ? *FString::Printf(TEXT("%.1f cm"), BestGap) : TEXT("> 1 m")), Ctr, B.GetExtent().Size(), C);
			}
		}
	}
}

void UFactoryGlitchMonitor::CheckJitter(float Dt)
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Prims(*It);
		for (UPrimitiveComponent* P : Prims)
		{
			if (!P || !P->IsSimulatingPhysics() || P->IsA<USkeletalMeshComponent>())
			{
				continue;
			}
			const float Speed = P->GetPhysicsLinearVelocity().Size();
			float& T = JitterTime.FindOrAdd(P);
			T = (P->RigidBodyIsAwake() && Speed > JitterMinSpeed && Speed < JitterMaxSpeed) ? T + Dt : 0.0f;
			if (T >= JitterSeconds)
			{
				Report(TEXT("jitter"), It->GetName() + TEXT(".") + P->GetName(), FString::Printf(TEXT("physics body awake at %.1f cm/s for %.1f s (never settles)"), Speed, T), P->GetComponentLocation(), P->Bounds.SphereRadius, P);
			}
		}
	}
}

void UFactoryGlitchMonitor::CheckCamera()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryGlitchCam), false);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
	TArray<FOverlapResult> Hits;
	if (GetWorld()->OverlapMultiByChannel(Hits, Cam, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(CameraClipRadius), Q))
	{
		for (const FOverlapResult& H : Hits)
		{
			if (H.GetComponent() && H.GetComponent()->GetCollisionResponseToChannel(ECC_Camera) == ECR_Block)
			{
				Report(TEXT("camera-clip"), GetNameSafe(H.GetActor()) + TEXT(".") + H.GetComponent()->GetName(), TEXT("player camera inside blocking geometry"), Cam, 60.0f, nullptr);
				break;
			}
		}
	}
}

void UFactoryGlitchMonitor::CheckCharacters(float Dt)
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		if (!Move || C->IsHidden())
		{
			continue;
		}
		FCharTrack& T = Chars.FindOrAdd(C);
		// Stuck: asking to move, not moving.
		const bool bTrying = Move->GetCurrentAcceleration().SizeSquared() > 1.0f && Move->IsMovingOnGround();
		if (bTrying && Move->Velocity.Size2D() < StuckMaxSpeed)
		{
			T.StuckTime += Dt;
			if (T.StuckTime >= StuckSeconds && !T.bInStuck)
			{
				T.bInStuck = true;
				Report(TEXT("stuck"), C->GetName(), FString::Printf(TEXT("accelerating but below %.0f cm/s for %.1f s"), StuckMaxSpeed, StuckSeconds), C->GetActorLocation(), 150.0f, nullptr);
			}
		}
		else
		{
			T.StuckTime = 0.0f;
			T.bInStuck = false;
		}
		// Spin: sustained fast yaw.
		const float Yaw = C->GetActorRotation().Yaw;
		if (T.bHavePrev && Dt > KINDA_SMALL_NUMBER)
		{
			const float Rate = FMath::Abs(FMath::FindDeltaAngleDegrees(T.PrevYaw, Yaw)) / Dt;
			T.SpinTime = Rate > SpinDegPerSec ? T.SpinTime + Dt : 0.0f;
			if (T.SpinTime >= SpinSeconds && !T.bInSpin)
			{
				T.bInSpin = true;
				Report(TEXT("spin"), C->GetName(), FString::Printf(TEXT("turning over %.0f deg/s for %.1f s"), SpinDegPerSec, SpinSeconds), C->GetActorLocation(), 150.0f, nullptr);
			}
			if (T.SpinTime == 0.0f) { T.bInSpin = false; }
		}
		T.PrevYaw = Yaw;
		T.bHavePrev = true;
	}
}

void UFactoryGlitchMonitor::WriteReport()
{
	using FactoryGlitch::Esc;
	const FString Dir = FactoryQAPaths::OutDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	TMap<FString, int32> ByKind;
	for (const TPair<FString, FGlitch>& P : Glitches) { ByKind.FindOrAdd(P.Value.Kind)++; }
	TMap<FString, int32> Errors, Warnings;
	if (LogScanner.IsValid())
	{
		FScopeLock L(&LogScanner->Lock);
		Errors = LogScanner->Errors;
		Warnings = LogScanner->Warnings;
	}
	float Worst = 0.0f;
	for (float H : Hitches) { Worst = FMath::Max(Worst, H); }
	const int32 Fails = ByKind.FindRef(TEXT("ref-pose")) + ByKind.FindRef(TEXT("floating")) + ByKind.FindRef(TEXT("jitter")) + ByKind.FindRef(TEXT("camera-clip"))
		+ ByKind.FindRef(TEXT("stuck")) + ByKind.FindRef(TEXT("spin")) + Errors.Num() + (Hitches.Num() > MaxHitches ? 1 : 0);

	FString J = FString::Printf(TEXT("{\"elapsedSeconds\":%.1f,\"pass\":%s,\"hitches\":{\"count\":%d,\"worstMs\":%.0f,\"limit\":%d},\"counts\":{"),
		Elapsed, Fails == 0 ? TEXT("true") : TEXT("false"), Hitches.Num(), Worst, MaxHitches);
	int32 k = 0;
	for (const TPair<FString, int32>& P : ByKind) { J += FString::Printf(TEXT("%s\"%s\":%d"), k++ ? TEXT(",") : TEXT(""), *P.Key, P.Value); }
	J += TEXT("},\"events\":[");
	k = 0;
	for (const TPair<FString, FGlitch>& P : Glitches)
	{
		const FGlitch& G = P.Value;
		J += FString::Printf(TEXT("%s{\"kind\":\"%s\",\"subject\":\"%s\",\"detail\":\"%s\",\"firstTime\":%.1f,\"frames\":%d,\"where\":\"%s\"}"),
			k++ ? TEXT(",") : TEXT(""), *G.Kind, *Esc(G.Subject), *Esc(G.Detail), G.FirstTime, G.Count, *G.Where.ToCompactString());
	}
	auto Lines = [](const TMap<FString, int32>& M)
	{
		FString S; int32 n = 0;
		for (const TPair<FString, int32>& P : M) { S += FString::Printf(TEXT("%s{\"line\":\"%s\",\"count\":%d}"), n++ ? TEXT(",") : TEXT(""), *FactoryGlitch::Esc(P.Key), P.Value); }
		return S;
	};
	J += FString::Printf(TEXT("],\"logErrors\":[%s],\"logWarnings\":[%s]}"), *Lines(Errors), *Lines(Warnings));
	FFileHelper::SaveStringToFile(J, *FPaths::Combine(Dir, TEXT("glitch.json")));

	FString T = FString::Printf(TEXT("FactoryQA glitch log (elapsed %.1fs): %s\n"), Elapsed, Fails == 0 ? TEXT("PASS") : TEXT("FAIL"));
	T += FString::Printf(TEXT("hitches over %.0f ms: %d (worst %.0f ms, limit %d)\n"), HitchMs, Hitches.Num(), Worst, MaxHitches);
	for (const TCHAR* Kind : { TEXT("ref-pose"), TEXT("floating"), TEXT("support-gap"), TEXT("jitter"), TEXT("camera-clip"), TEXT("stuck"), TEXT("spin") })
	{
		T += FString::Printf(TEXT("%s: %d\n"), Kind, ByKind.FindRef(Kind));
	}
	for (const TPair<FString, FGlitch>& P : Glitches)
	{
		T += FString::Printf(TEXT("  [%s] t=%.1fs %s - %s @ %s\n"), *P.Value.Kind, P.Value.FirstTime, *P.Value.Subject, *P.Value.Detail, *P.Value.Where.ToCompactString());
	}
	T += FString::Printf(TEXT("log errors: %d unique, warnings: %d unique\n"), Errors.Num(), Warnings.Num());
	for (const TPair<FString, int32>& P : Errors) { T += FString::Printf(TEXT("  E x%d %s\n"), P.Value, *P.Key); }
	int32 w = 0;
	for (const TPair<FString, int32>& P : Warnings) { if (++w > 30) { break; } T += FString::Printf(TEXT("  W x%d %s\n"), P.Value, *P.Key); }
	FFileHelper::SaveStringToFile(T, *FPaths::Combine(Dir, TEXT("glitch.txt")));
}
