// Copyright Plaza Duty. All Rights Reserved.

#include "Data/PDMatchConfig.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "PlazaDuty"

namespace PDConfigDefaults
{
	FPDPropTuning Prop(EPDPropKind Kind, const FText& Name, const FText& Trick, float Wear, float Clear, float Ride, const FText& Launch, float LaunchWear)
	{
		FPDPropTuning P;
		P.Kind = Kind;
		P.DisplayName = Name;
		P.TrickName = Trick;
		P.TrickWear = Wear;
		P.ClearHeight = Clear;
		P.RideLength = Ride;
		P.LaunchTrickName = Launch;
		P.LaunchWear = LaunchWear;
		return P;
	}

	FPDPhaseRow Phase(float Start, const FText& Name, const FText& Banner, int32 Target, float Speed, float PropAppeal, float FriendCooldown)
	{
		FPDPhaseRow R;
		R.StartTime = Start;
		R.Name = Name;
		R.BannerText = Banner;
		R.SkaterTarget = Target;
		R.SpeedScale = Speed;
		R.PropAppealScale = PropAppeal;
		R.FriendCooldown = FriendCooldown;
		return R;
	}
}

UPDMatchConfig::UPDMatchConfig()
{
	using namespace PDConfigDefaults;
	Props = {
		Prop(EPDPropKind::Cone, LOCTEXT("Cone", "CONE"), LOCTEXT("ConeTrick", "Cone vault"), 3.0f, 95.0f, 0.0f, LOCTEXT("ConeLaunch", "Cone leap"), 5.0f),
		Prop(EPDPropKind::Bench, LOCTEXT("Bench", "BENCH"), LOCTEXT("BenchTrick", "Bench run"), 5.0f, 50.0f, 150.0f, LOCTEXT("BenchLaunch", "Bench leap"), 6.0f),
		Prop(EPDPropKind::Planter, LOCTEXT("Planter", "PLANTER"), LOCTEXT("PlanterTrick", "Planter vault"), 5.0f, 80.0f, 0.0f, LOCTEXT("PlanterLaunch", "Planter launch"), 9.0f),
		Prop(EPDPropKind::TrashCan, LOCTEXT("Trash", "TRASH CAN"), LOCTEXT("TrashTrick", "Trash can vault"), 4.0f, 105.0f, 0.0f, LOCTEXT("TrashLaunch", "Trash can leap"), 6.0f),
	};
	Phases = {
		Phase(0.0f, LOCTEXT("PhaseAfternoon", "EVENING"), FText::GetEmpty(), 3, 1.0f, 1.0f, 8.0f),
		Phase(25.0f, LOCTEXT("PhaseRush", "RUSH"), LOCTEXT("RushBanner", "School's out. The plaza fills up"), 4, 1.06f, 1.2f, 7.0f),
		Phase(55.0f, LOCTEXT("PhaseGolden", "GOLDEN HOUR"), LOCTEXT("GoldenBanner", "Golden hour. Everyone wants the clip"), 5, 1.12f, 1.35f, 5.0f),
		Phase(80.0f, LOCTEXT("PhaseClosing", "CLOSING"), LOCTEXT("ClosingBanner", "Ten seconds to closing"), 5, 1.12f, 1.35f, 99.0f),
	};
}

const FPDPropTuning& UPDMatchConfig::GetPropTuning(EPDPropKind Kind) const
{
	for (const FPDPropTuning& P : Props)
	{
		if (P.Kind == Kind)
		{
			return P;
		}
	}
	static const FPDPropTuning Fallback;
	return Props.Num() > 0 ? Props[0] : Fallback;
}

int32 UPDMatchConfig::GetPhaseIndex(float Elapsed) const
{
	int32 Index = 0;
	for (int32 i = 0; i < Phases.Num(); ++i)
	{
		if (Elapsed >= Phases[i].StartTime)
		{
			Index = i;
		}
	}
	return Index;
}

#undef LOCTEXT_NAMESPACE
