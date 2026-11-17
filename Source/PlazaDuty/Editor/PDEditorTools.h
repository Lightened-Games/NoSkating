// Copyright Plaza Duty. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PDEditorTools.generated.h"

class UNiagaraSystem;
class UMaterialInterface;
class UStaticMesh;
class UFont;
class UFontFace;

/** Editor-only helpers for the content scripts (Python cannot build volume brushes or reach Niagara renderers). */
UCLASS()
class PLAZADUTY_API UPDEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Point every renderer of the system at Material. bSpritesOnly disables non-sprite renderers. Returns renderers changed. */
	UFUNCTION(BlueprintCallable, Category = "PD|Editor")
	static int32 SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly);

	/** Spawn a BlockingVolume with a real cube brush. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "PD|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label);

	/** Spawn a NavMeshBoundsVolume with a real cube brush. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "PD|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnNavMeshBounds(UObject* WorldContextObject, FVector Location, FVector HalfExtent);

	/** Make a static mesh use its render geometry as collision (ledges, stairs, the bank). */
	UFUNCTION(BlueprintCallable, Category = "PD|Editor")
	static bool SetMeshComplexCollision(UStaticMesh* Mesh);

	/** Create (or rebuild) a runtime composite UFont whose default typeface lists Faces under TypefaceNames
	 *  ("Light", "Regular", "Bold"...). Python cannot author FTypefaceEntry; the script saves the returned asset. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "PD|Editor")
	static UFont* CreateCompositeFont(FString PackagePath, TArray<FName> TypefaceNames, TArray<UFontFace*> Faces);
};
