// FactoryQA shared output folder: one per process, shared by every world and every FactoryQA subsystem.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace FactoryQAPaths
{
	inline const FString& Stamp()
	{
		static const FString S = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		return S;
	}

	inline FString OutDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QA"), TEXT("FactoryQA"), Stamp()));
	}
}
