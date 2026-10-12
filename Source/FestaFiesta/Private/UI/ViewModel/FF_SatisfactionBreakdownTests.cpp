#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/ViewModel/FF_SatisfactionBreakdown.h"
#include "UI/ViewModel/FF_DailyReportComment.h"
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"
#include "UI/LiveHUD/Panels/FF_SatisfactionBreakdownWidget.h"

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// 욕구 60 · 대기 70 · 밀도 80 · 취향 50, 사건 감점 3. 즐길 거리 시설 5/10곳, 즐길 거리를 못 찾은 손님 20%
	FFF_SatisfactionBreakdownInputs MakeInputs()
	{
		FFF_SatisfactionBreakdownInputs Inputs;
		Inputs.NeedScore = 60;
		Inputs.WaitScore = 70;
		Inputs.DensityScore = 80;
		Inputs.PreferenceScore = 50;
		Inputs.BaseSatisfaction = 64;
		Inputs.Satisfaction = 61;
		Inputs.AverageWaitSeconds = 42.f;
		Inputs.QueueSuccessRate = 0.8f;
		Inputs.LongestWaitFacility = FText::FromString(TEXT("피자 부스"));
		Inputs.CrowdedShare = 0.25f;
		Inputs.PreferenceMatchShare = 0.4f;
		Inputs.PreferenceSampleCount = 20;
		Inputs.CoverageTarget[0] = 7.f;
		Inputs.CoverageTarget[1] = 2.f;
		Inputs.CoverageTarget[2] = 10.f;
		Inputs.ReliefFacilityCount[0] = 7;
		Inputs.ReliefFacilityCount[1] = 2;
		Inputs.ReliefFacilityCount[2] = 5;
		Inputs.UnmetPercent[0] = 5.f;
		Inputs.UnmetPercent[1] = 10.f;
		Inputs.UnmetPercent[2] = 20.f;
		return Inputs;
	}
}

// 항목 감점을 더하면 정확히 100 - 만족도. 큰 순서로 적고, 근거와 줄이는 방법을 붙인다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionBreakdown_LossesAddUpToScore,
	"FestaFiesta.UI.SatisfactionBreakdown.LossesAddUpToScore",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionBreakdown_LossesAddUpToScore::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("점수를 모르면 분해하지 않는다"), FFF_SatisfactionBreakdownRules::Build(FFF_SatisfactionBreakdownInputs()).bValid);

	const FFF_SatisfactionBreakdown Breakdown = FFF_SatisfactionBreakdownRules::Build(MakeInputs());
	if (!TestTrue(TEXT("분해했다"), Breakdown.bValid))
	{
		return false;
	}
	int32 Sum = 0;
	for (int32 Index = 0; Index < Breakdown.Losses.Num(); ++Index)
	{
		Sum += Breakdown.Losses[Index].Points;
		if (Index > 0)
		{
			TestTrue(TEXT("많이 깎은 순"), Breakdown.Losses[Index - 1].Points >= Breakdown.Losses[Index].Points);
		}
	}
	TestEqual(TEXT("감점 합 = 100 - 만족도"), Sum, 100 - 61);
	TestEqual(TEXT("비중이 가장 큰 욕구가 가장 많이 깎았다"), static_cast<int32>(Breakdown.Losses[0].Factor), static_cast<int32>(EFF_SatisfactionFactor::Need));
	TestEqual(TEXT("욕구 감점"), Breakdown.GetLoss(EFF_SatisfactionFactor::Need), 16);
	TestEqual(TEXT("취향 감점"), Breakdown.GetLoss(EFF_SatisfactionFactor::Preference), 10);
	TestEqual(TEXT("대기 감점"), Breakdown.GetLoss(EFF_SatisfactionFactor::Wait), 6);
	TestEqual(TEXT("밀도 감점"), Breakdown.GetLoss(EFF_SatisfactionFactor::Density), 4);
	TestEqual(TEXT("사건 감점은 감점 전 점수와의 차이"), Breakdown.GetLoss(EFF_SatisfactionFactor::Penalty), 3);

	const FString NeedEvidence = Breakdown.Losses[0].Evidence.ToString();
	TestTrue(TEXT("모자란 시설 수를 근거로 든다"), NeedEvidence.Contains(TEXT("즐길 거리 시설 5/10곳")));
	TestFalse(TEXT("목표를 채운 시설은 말하지 않는다"), NeedEvidence.Contains(TEXT("화장실 시설")));
	const FString WaitEvidence = Breakdown.Losses[2].Evidence.ToString();
	TestTrue(TEXT("대기 근거: 평균 초와 돌아선 비율과 시설"), WaitEvidence.Contains(TEXT("42초")) && WaitEvidence.Contains(TEXT("20%")) && WaitEvidence.Contains(TEXT("피자 부스")));

	// 즐길 거리 5곳을 더 지으면 Need 의 그 항이 (100 - 20) × 0.5 / 3 만큼 오르고, 그 비중 0.39 를 곱해 약 5점
	const FString Hint = Breakdown.Hint.ToString();
	TestTrue(TEXT("가장 많이 깎은 욕구를 시설 수로 줄이는 방법"), Hint.Contains(TEXT("즐길 거리")) && Hint.Contains(TEXT("5곳")) && Hint.Contains(TEXT("5점")));

	// 화장실은 목표를 채웠는데 손님 72%가 못 찾았다: 시설 수가 아니라 거리 · 줄 문제로 짚는다
	FFF_SatisfactionBreakdownInputs Unreached = MakeInputs();
	Unreached.ReliefFacilityCount[1] = 3;
	Unreached.UnmetPercent[1] = 72.f;
	const FFF_SatisfactionBreakdown UnreachedBreakdown = FFF_SatisfactionBreakdownRules::Build(Unreached);
	const FString UnreachedEvidence = UnreachedBreakdown.Losses[0].Evidence.ToString();
	TestTrue(TEXT("가장 많이 깎은 욕구가 맨 앞: 시설 수와 못 찾은 비율"),
		UnreachedEvidence.StartsWith(TEXT("화장실 시설 3곳")) && UnreachedEvidence.Contains(TEXT("화장실을 못 찾은 손님 72%")));
	TestTrue(TEXT("모자란 다른 시설도 함께"), UnreachedEvidence.Contains(TEXT("즐길 거리 시설 5/10곳")));
	TestTrue(TEXT("시설이 있는데 못 찾으면 옮기라고 한다"), UnreachedBreakdown.Hint.ToString().Contains(TEXT("화장실 시설은 있는데")));

	FFF_SatisfactionBreakdownInputs FewDecisions = MakeInputs();
	FewDecisions.PreferenceSampleCount = 3;
	TestTrue(TEXT("결정이 적으면 취향 비율을 근거로 쓰지 않는다"),
		FFF_SatisfactionBreakdownRules::Build(FewDecisions).Losses[1].Evidence.IsEmpty());

	FFF_SatisfactionBreakdownInputs NoPenalty = MakeInputs();
	NoPenalty.Satisfaction = NoPenalty.BaseSatisfaction;
	TestEqual(TEXT("사건 감점이 없으면 그 줄을 넣지 않는다"), FFF_SatisfactionBreakdownRules::Build(NoPenalty).GetLoss(EFF_SatisfactionFactor::Penalty), static_cast<int32>(INDEX_NONE));
	return true;
}

// 불만 1위는 원점수가 아니라 실제로 깎은 점수로 고른다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionBreakdown_ComplaintFollowsLoss,
	"FestaFiesta.UI.SatisfactionBreakdown.ComplaintFollowsLoss",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionBreakdown_ComplaintFollowsLoss::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	Inputs.bHasSatisfaction = true;
	Inputs.NeedScore = 65;
	Inputs.WaitScore = 90;
	Inputs.DensityScore = 45;
	Inputs.PreferenceScore = 90;
	TestEqual(TEXT("깎은 점수를 모르면 원점수가 가장 낮은 밀도"),
		static_cast<int32>(FFF_DailyReportCommentRules::FindComplaint(Inputs).Axis), static_cast<int32>(EFF_DailyComplaintAxis::Density));

	Inputs.NeedLoss = 14;
	Inputs.WaitLoss = 2;
	Inputs.DensityLoss = 11;
	Inputs.PreferenceLoss = 2;
	TestEqual(TEXT("비중이 커서 더 많이 깎은 욕구"),
		static_cast<int32>(FFF_DailyReportCommentRules::FindComplaint(Inputs).Axis), static_cast<int32>(EFF_DailyComplaintAxis::Need));

	Inputs.NeedScore = 75;
	TestEqual(TEXT("기준 점수 이상인 항목은 많이 깎았어도 불만으로 짚지 않는다"),
		static_cast<int32>(FFF_DailyReportCommentRules::FindComplaint(Inputs).Axis), static_cast<int32>(EFF_DailyComplaintAxis::Density));
	return true;
}

// 결산 패널: WBP 를 건드리지 않고 만족도 카드 안 평가 만족도 줄 바로 위에 끼운다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionBreakdown_ReportPanelHostsWidget,
	"FestaFiesta.UI.SatisfactionBreakdown.ReportPanelHostsWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionBreakdown_ReportPanelHostsWidget::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* PanelClass = LoadClass<UFF_LivePanelReport>(nullptr,
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport.WBP_FF_LivePanelReport_C"));
	if (!TestNotNull(TEXT("에디터 월드"), World) || !TestNotNull(TEXT("결산 WBP"), PanelClass))
	{
		return false;
	}
	TStrongObjectPtr<UFF_LivePanelReport> Panel(CreateWidget<UFF_LivePanelReport>(World, PanelClass));
	if (!TestNotNull(TEXT("결산 패널"), Panel.Get()))
	{
		return false;
	}
	Panel->TakeWidget();
	const UVerticalBox* Body = Cast<UVerticalBox>(Panel->WidgetTree->FindWidget(TEXT("Report_SatisfactionBody")));
	const UWidget* ScoreRow = Panel->WidgetTree->FindWidget(TEXT("Report_ScoreRow"));
	if (!TestNotNull(TEXT("만족도 카드 본문"), Body) || !TestNotNull(TEXT("평가 만족도 줄"), ScoreRow))
	{
		return false;
	}
	const int32 ScoreRowIndex = Body->GetChildIndex(ScoreRow);
	UFF_SatisfactionBreakdownWidget* Breakdown = ScoreRowIndex > 0
		? Cast<UFF_SatisfactionBreakdownWidget>(Body->GetChildAt(ScoreRowIndex - 1)) : nullptr;
	if (!TestNotNull(TEXT("평가 만족도 줄 바로 위에 분해 위젯"), Breakdown))
	{
		return false;
	}

	TestEqual(TEXT("결산 전에는 줄이 없다"), Breakdown->GetLossRowCount(), 0);
	TestTrue(TEXT("결산 전에는 접힌다"), Breakdown->GetVisibility() == ESlateVisibility::Collapsed);
	Breakdown->SetBreakdown(FFF_SatisfactionBreakdownRules::Build(MakeInputs()));
	TestEqual(TEXT("욕구 · 대기 · 밀도 · 취향 · 사건 다섯 줄"), Breakdown->GetLossRowCount(), 5);
	TestTrue(TEXT("요약은 깎인 점수와 만족도"), Breakdown->GetSummaryText().ToString().Contains(TEXT("39점이 깎여 61점")));
	TestFalse(TEXT("줄이는 방법"), Breakdown->GetHintText().IsEmpty());

	// 눈으로 확인할 때만: -SatisfactionBreakdownCapture 를 주고 -nullrhi 없이 돌리면 결산 패널을 PNG 로 남긴다
	if (FParse::Param(FCommandLine::Get(), TEXT("SatisfactionBreakdownCapture")) && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		const TSharedRef<SWidget> Slate = Panel->TakeWidget();
		const FIntPoint Resolution(1140, 1400);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Resolution), TF_Bilinear, true));
		if (!TestNotNull(TEXT("캡처 대상"), Target.Get()))
		{
			return false;
		}
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Slate, FVector2D(Resolution), 0.f, false);
			FlushRenderingCommands();
			FAssetCompilingManager::Get().FinishAllCompilation();
			FSlateApplication::Get().InvalidateAllWidgets(false);
		}
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (TestTrue(TEXT("픽셀을 읽었다"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
		{
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
			const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SatisfactionBreakdown"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Path = Directory / TEXT("ReportPanel.png");
			TestTrue(TEXT("PNG 저장"), FFileHelper::SaveArrayToFile(Png, *Path));
			AddInfo(FPaths::ConvertRelativePathToFull(Path));
		}
	}
	Panel->ReleaseSlateResources(true);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
