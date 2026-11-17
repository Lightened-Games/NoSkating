// Copyright Plaza Duty. All Rights Reserved.

#include "Editor/PDEditorTools.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Engine/BlockingVolume.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#endif

namespace PDEditor
{
	template <typename TVolume>
	TVolume* SpawnBrushVolume(UObject* WorldContextObject, const FVector& Location, const FVector& HalfExtent, const FString& Label)
	{
#if WITH_EDITOR
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TVolume* Volume = World->SpawnActor<TVolume>(TVolume::StaticClass(), FTransform(Location), Params);
		if (!Volume)
		{
			return nullptr;
		}
		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		Builder->X = HalfExtent.X * 2.0f;
		Builder->Y = HalfExtent.Y * 2.0f;
		Builder->Z = HalfExtent.Z * 2.0f;
		UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
		Volume->SetActorLabel(Label);
		return Volume;
#else
		return nullptr;
#endif
	}
}

AActor* UPDEditorTools::SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label)
{
	return PDEditor::SpawnBrushVolume<ABlockingVolume>(WorldContextObject, Location, HalfExtent, Label);
}

AActor* UPDEditorTools::SpawnNavMeshBounds(UObject* WorldContextObject, FVector Location, FVector HalfExtent)
{
	AActor* Bounds = PDEditor::SpawnBrushVolume<ANavMeshBoundsVolume>(WorldContextObject, Location, HalfExtent, TEXT("NavMeshBounds"));
#if WITH_EDITOR
	// The commandlet world has no navigation system to create the nav data actor, so place one (built at runtime).
	UWorld* World = Bounds ? Bounds->GetWorld() : nullptr;
	if (World && !TActorIterator<ARecastNavMesh>(World))
	{
		if (ARecastNavMesh* Nav = World->SpawnActor<ARecastNavMesh>(ARecastNavMesh::StaticClass(), FTransform::Identity))
		{
			Nav->SetActorLabel(TEXT("RecastNavMesh"));
		}
	}
#endif
	return Bounds;
}

bool UPDEditorTools::SetMeshComplexCollision(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return false;
	}
	Mesh->CreateBodySetup();
	UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body)
	{
		return false;
	}
	Body->Modify();
	Body->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UPDEditorTools::SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || !Material)
	{
		return 0;
	}
	System->Modify();
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data)
		{
			continue;
		}
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer)
			{
				continue;
			}
			Renderer->Modify();
			if (bSpritesOnly && !Renderer->IsA<UNiagaraSpriteRendererProperties>())
			{
				Renderer->SetIsEnabled(false);
				continue;
			}
			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->Material = Material;
			}
			else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Ribbon->Material = Material;
			}
			else if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				MeshRenderer->bOverrideMaterials = true;
				MeshRenderer->OverrideMaterials.SetNum(FMath::Max(1, MeshRenderer->OverrideMaterials.Num()));
				for (FNiagaraMeshMaterialOverride& Override : MeshRenderer->OverrideMaterials)
				{
					Override.ExplicitMat = Material;
				}
			}
			else
			{
				continue;
			}
			FPropertyChangedEvent Event(nullptr);
			Renderer->PostEditChangeProperty(Event);
			++Changed;
		}
	}
	System->MarkPackageDirty();
#endif
	return Changed;
}

UFont* UPDEditorTools::CreateCompositeFont(FString PackagePath, TArray<FName> TypefaceNames, TArray<UFontFace*> Faces)
{
#if WITH_EDITOR
	if (TypefaceNames.Num() != Faces.Num() || Faces.Num() == 0)
	{
		return nullptr;
	}
	const FString AssetName = FPackageName::GetShortName(PackagePath);
	UPackage* Package = CreatePackage(*PackagePath);
	UFont* Font = FindObject<UFont>(Package, *AssetName);
	if (!Font)
	{
		Font = NewObject<UFont>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}
	Font->FontCacheType = EFontCacheType::Runtime;
	Font->CompositeFont = FCompositeFont();
	for (int32 i = 0; i < Faces.Num(); ++i)
	{
		if (Faces[i])
		{
			FTypefaceEntry& Entry = Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TypefaceNames[i]));
			Entry.Font = FFontData(Faces[i]);
		}
	}
	Font->MarkPackageDirty();
	return Font;
#else
	return nullptr;
#endif
}

