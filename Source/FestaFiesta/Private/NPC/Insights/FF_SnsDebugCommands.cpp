// Fill out your copyright notice in the Description page of Project Settings.

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "NPC/Insights/FF_CrowdInsightsSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace FFSnsConsole
{
	UFF_CrowdInsightsSubsystem* GetInsights(UWorld* World, FOutputDevice& Ar)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UFF_CrowdInsightsSubsystem* Insights = GI ? GI->GetSubsystem<UFF_CrowdInsightsSubsystem>() : nullptr;
		if (!Insights)
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("CrowdInsightsSubsystem을 찾을 수 없습니다 (게임이 실행 중인지 확인)"));
		}
		return Insights;
	}

	int32 ParseIntArg(const TArray<FString>& Args, int32 Index, int32 DefaultValue)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : DefaultValue;
	}

	void LogPost(FOutputDevice& Ar, const FFF_SnsPostRow& Post)
	{
		Ar.Logf(TEXT("  [화제%2d/%s] %s: %s  (세기=%.0f, TemplateId=%s, t=%.1fs)"),
			Post.TopicId,
			Post.Sentiment == EFF_SnsSentiment::Positive ? TEXT("긍정") : TEXT("부정"),
			*Post.DisplayHandle,
			Post.Body.IsEmpty() ? TEXT("(비어있음 — DataTable 미연결 가능성)") : *Post.Body.ToString(),
			Post.TopicSalience0To100,
			*Post.MessageTemplateId.ToString(),
			Post.GameTimestampSeconds);
	}
}

// DataTable·계수가 제대로 연결됐는지 확인하는 명령. 타이머(3~10초)를 기다리지 않고
// 지금 즉시 한 건(또는 Count건) 만들어 보여준다 — 링버퍼에도 실제로 쌓인다.
static void SnsForceCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdInsightsSubsystem* Insights = FFSnsConsole::GetInsights(World, Ar);
	if (!Insights)
	{
		return;
	}

	const int32 Count = FMath::Max(1, FFSnsConsole::ParseIntArg(Args, 0, 1));
	Ar.Logf(TEXT("SNS 글 %d건 강제 생성:"), Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FFSnsConsole::LogPost(Ar, Insights->Debug_ForceGeneratePost());
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSnsForceCommand(
	TEXT("FF.SNS.Force"),
	TEXT("SNS 글을 지금 즉시 N건(기본 1) 강제로 만든다. 예: FF.SNS.Force 5"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SnsForceCommand));

// 지금까지 쌓인 링버퍼 내용을 그대로 본다 — 실제로 주기 생성이 도는지, 문장이 채워지는지 확인용
static void SnsListCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdInsightsSubsystem* Insights = FFSnsConsole::GetInsights(World, Ar);
	if (!Insights)
	{
		return;
	}

	const TArray<FFF_SnsPostRow>& Posts = Insights->GetRecentPosts();
	if (Posts.IsEmpty())
	{
		Ar.Logf(TEXT("아직 쌓인 글이 없습니다 — Live 단계가 아니거나, 아직 첫 간격이 안 지났을 수 있습니다. FF.SNS.Force로 강제 생성해 보세요"));
		return;
	}

	const int32 Count = FMath::Min(Posts.Num(), FMath::Max(1, FFSnsConsole::ParseIntArg(Args, 0, 10)));
	Ar.Logf(TEXT("최근 SNS 글 %d/%d건 (최신 순):"), Count, Posts.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FFSnsConsole::LogPost(Ar, Posts[i]);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSnsListCommand(
	TEXT("FF.SNS.List"),
	TEXT("최근에 쌓인 SNS 글을 N건(기본 10) 본다. 예: FF.SNS.List 20"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SnsListCommand));

#endif // !UE_BUILD_SHIPPING
