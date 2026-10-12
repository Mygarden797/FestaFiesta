#include "NPC/Insights/FF_CrowdInsightsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalBuzzSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "NPC/Insights/FF_SnsFeedGenerator.h"
#include "Save/FF_SaveArchive.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

namespace FFSnsReactionTests
{
struct FHarness
{
	FTestWorldWrapper Fixture;
	UFF_CrowdInsightsSubsystem* Insights = nullptr;
	UFF_FestivalBuzzSubsystem* Buzz = nullptr;
	UFF_FestivalProgressionSubsystem* Progression = nullptr;
	UFF_FestivalStageSubsystem* Stage = nullptr;
	UFF_FestivalViewModelSubsystem* ViewModel = nullptr;

	bool Initialize(FAutomationTestBase& Test)
	{
		if (!Fixture.CreateTestWorld(EWorldType::Game))
		{
			Fixture.ForwardErrorMessages(&Test);
			return false;
		}
		UGameInstance* Game = Fixture.GetTestWorld()->GetGameInstance();
		Insights = Game->GetSubsystem<UFF_CrowdInsightsSubsystem>();
		Buzz = Game->GetSubsystem<UFF_FestivalBuzzSubsystem>();
		Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
		Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
		ViewModel = Game->GetSubsystem<UFF_FestivalViewModelSubsystem>();
		if (!Insights || !Buzz || !Progression || !Stage || !ViewModel) return false;

		UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(Game);
		UDataTable* Costs = NewObject<UDataTable>(Game);
		Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
		Costs->AddRow(TEXT("Cost.SnsReactionTest"), FFF_FacilityCostRow());
		Definition->FestivalId = TEXT("SnsReactionTest");
		Definition->FacilityCostTable = Costs;
		Definition->FacilityRevenueTable.Reset();
		Definition->RequiredSetupFacilities.Reset();
		Definition->BuzzPerSnsReaction = 0.1f;
		Progression->StartFestival(Definition);
		Stage->bFestivalStarted = true;
		// 월드 프레임을 진행하지 않고 Bind + Sync만 실행한다.
		ViewModel->Tick(0.f);
		return true;
	}
};

FFF_SnsPostRow MakePost(const int32 Sequence)
{
	FFF_SnsFeedSignals Signals;
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> Templates;
	FFF_SnsPostRow Post = FFF_SnsFeedGenerator::GeneratePost(20261012, Sequence, Signals, Templates, 0.f);
	Post.Sentiment = Sequence % 2 == 0 ? EFF_SnsSentiment::Positive : EFF_SnsSentiment::Negative;
	Post.Body = FText::FromString(TEXT("공감 원본 명령 검사"));
	Post.MessageTemplateId = TEXT("SnsReactionAutomation");
	Post.TopicSalience0To100 = 0.f;
	return Post;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsReactionRewardTest,
	"FestaFiesta.NPC.Insights.ReactionRewardAndGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_SnsReactionRewardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSnsReactionTests;
	FHarness H;
	if (!TestTrue(TEXT("공감 원본과 ViewModel 검사 환경"), H.Initialize(*this))) return false;
	H.Insights->HandleFestivalStarted();
	const float Before = H.Buzz->GetBuzz();
	FGuid LastId;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const FFF_SnsPostRow Post = MakePost(Index);
		LastId = Post.PostId;
		H.Insights->PushPost(Post);
		if (!TestTrue(TEXT("ViewModel이 새 글을 중계한다"), H.ViewModel->GetRecentSnsPosts().Num() > 0)) return false;
		TestEqual(TEXT("원본 PostId를 표시 모델에 보존한다"), H.ViewModel->GetRecentSnsPosts()[0].PostId, LastId);
		const FFF_SnsReactionOutcome Outcome = H.ViewModel->RequestSnsReaction(LastId);
		TestEqual(TEXT("긍정·부정 모두 일일 제한 없이 공감한다"), Outcome.Result, EFF_SnsReactionResult::Success);
		TestEqual(TEXT("실제 적용량은 0.1이다"), Outcome.AppliedBuzz, 0.1f, 0.0001f);
		TestTrue(TEXT("성공 이후 원본 공감 상태를 읽는다"), H.ViewModel->GetSnsReactionState(LastId).bReacted);
	}
	TestEqual(TEXT("12건에도 일일 제한이 없다"), H.Buzz->GetBuzz(), Before + 1.2f, 0.001f);
	TestEqual(TEXT("같은 글의 두 번째 요청은 지급하지 않는다"), H.ViewModel->RequestSnsReaction(LastId).Result,
		EFF_SnsReactionResult::AlreadyReacted);
	TestEqual(TEXT("피드에 노출되지 않은 글은 거절한다"), H.Insights->TryReactToPost(FGuid::NewGuid()).Result,
		EFF_SnsReactionResult::PostUnavailable);

	const FFF_SnsPostRow Pending = MakePost(12);
	H.Insights->PushPost(Pending);
	{
		TGuardValue<bool> NestedCommand(H.Insights->bReactionInFlight, true);
		TestEqual(TEXT("원본 명령 처리 중 재진입은 차단한다"), H.Insights->TryReactToPost(Pending.PostId).Result,
			EFF_SnsReactionResult::Unavailable);
	}
	TestFalse(TEXT("거절한 요청은 공감 이력에 쓰지 않는다"), H.Insights->GetReactionState(Pending.PostId).bReacted);
	H.Buzz->AddBuzz(100.f, TEXT("AutomationCap"));
	const FFF_SnsReactionOutcome AtCap = H.Insights->TryReactToPost(Pending.PostId);
	TestEqual(TEXT("화제성 상한에서도 공감은 완료된다"), AtCap.Result, EFF_SnsReactionResult::Success);
	TestEqual(TEXT("상한에서 실제 보상은 0으로 반환한다"), AtCap.AppliedBuzz, 0.f);

	const FFF_SnsPostRow Unreacted = MakePost(13);
	H.Insights->PushPost(Unreacted);
	H.Stage->CurrentDay = 3;
	H.Stage->CurrentStage = EFF_FestivalStage::Report;
	TestTrue(TEXT("원본 축제 종료 명령 성공"), H.Stage->FinishFestival());
	TestFalse(TEXT("평가 확정 뒤에는 새 공감을 허용하지 않는다"), H.Insights->GetReactionState(Unreacted.PostId).bCanReact);
	TestEqual(TEXT("평가 확정 이후 명령도 거절한다"), H.Insights->TryReactToPost(Unreacted.PostId).Result,
		EFF_SnsReactionResult::Unavailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsReactionPersistenceTest,
	"FestaFiesta.NPC.Insights.ReactionPersistenceAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_SnsReactionPersistenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFSnsReactionTests;
	FHarness H;
	if (!TestTrue(TEXT("SNS 저장 검사 환경"), H.Initialize(*this))) return false;
	H.Insights->HandleFestivalStarted();
	H.Insights->GlobalSimulationSeed = 20261012;
	H.Insights->PostSequenceIndex = 41;
	const FFF_SnsPostRow Post = MakePost(40);
	H.Insights->PushPost(Post);
	TestEqual(TEXT("저장 전 첫 공감"), H.Insights->TryReactToPost(Post.PostId).Result, EFF_SnsReactionResult::Success);
	TArray<uint8> FeedBytes;
	TArray<uint8> BuzzBytes;
	H.Insights->WriteSaveData(FeedBytes);
	H.Buzz->WriteSaveData(BuzzBytes);
	const float SavedBuzz = H.Buzz->GetBuzz();

	H.Insights->HandleFestivalStarted();
	TestTrue(TEXT("새 축제에서는 피드를 비운다"), H.ViewModel->GetRecentSnsPosts().IsEmpty());
	TestFalse(TEXT("새 축제에서는 공감 이력을 비운다"), H.Insights->GetReactionState(Post.PostId).bReacted);
	H.Buzz->ResetForNewFestival();
	H.Buzz->ReadSaveData(BuzzBytes);
	H.Insights->ReadSaveData(FeedBytes);
	TestEqual(TEXT("글 복원으로 생성 효과를 재적용하지 않는다"), H.Buzz->GetBuzz(), SavedBuzz);
	if (!TestEqual(TEXT("저장된 최근 피드가 ViewModel에도 복원된다"), H.ViewModel->GetRecentSnsPosts().Num(), 1)) return false;
	TestEqual(TEXT("복원한 글의 원본 식별자를 유지한다"), H.ViewModel->GetRecentSnsPosts()[0].PostId, Post.PostId);
	TestTrue(TEXT("공감 표시도 원본 저장에서 되돌아온다"), H.ViewModel->GetSnsReactionState(Post.PostId).bReacted);
	TestEqual(TEXT("복원 직후 중복 보상을 막는다"), H.ViewModel->RequestSnsReaction(Post.PostId).Result,
		EFF_SnsReactionResult::AlreadyReacted);
	TestEqual(TEXT("생성 순번도 함께 복원한다"), H.Insights->PostSequenceIndex, 41);
	TestEqual(TEXT("생성 시드도 함께 복원한다"), H.Insights->GlobalSimulationSeed, 20261012);
	const FFF_SnsPostRow Next = H.Insights->Debug_ForceGeneratePost();
	TestTrue(TEXT("복원 뒤 생성한 글은 이전 글과 다른 ID다"), Next.PostId != Post.PostId);
	TestEqual(TEXT("복원 뒤 생성은 저장된 다음 순번을 쓴다"), Next.PostId, MakePost(41).PostId);

	const float BeforeLegacy = H.Buzz->GetBuzz();
	H.Insights->ReadSaveData({});
	TestTrue(TEXT("공감 이전 저장에는 세션의 기존 글을 남기지 않는다"), H.Insights->GetRecentPosts().IsEmpty());
	TestFalse(TEXT("공감 이전 저장에는 세션의 공감 이력도 남기지 않는다"), H.Insights->GetReactionState(Post.PostId).bReacted);
	TestEqual(TEXT("피드가 없는 옛 저장을 읽어도 경제 값을 건드리지 않는다"), H.Buzz->GetBuzz(), BeforeLegacy);
	return true;
}

#endif
