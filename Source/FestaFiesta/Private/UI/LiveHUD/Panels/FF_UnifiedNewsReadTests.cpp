#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"

#include "Components/ScrollBox.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UnifiedNewsReadTest,
	"FestaFiesta.UI.SNS.UnifiedIndividualRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_UnifiedNewsReadTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UClass* PanelClass = LoadClass<UFF_LivePanelSNS>(nullptr,
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout.WBP_FF_LiveSnsFlyout_C"));
	if (!TestNotNull(TEXT("에디터 월드"), World) || !TestNotNull(TEXT("배포 소식 패널 클래스"), PanelClass)) { return false; }

	TStrongObjectPtr<UFF_LivePanelSNS> Panel(CreateWidget<UFF_LivePanelSNS>(World, PanelClass));
	if (!TestNotNull(TEXT("임시 소식 패널"), Panel.Get())) { return false; }
	Panel->SetFlags(RF_Transient);
	const TSharedRef<SWidget> Slate = Panel->TakeWidget();
	int32 BadgeCount = INDEX_NONE;
	const FDelegateHandle BadgeHandle = Panel->OnPendingPostsChanged.AddLambda(
		[&BadgeCount](int32 Count) { BadgeCount = Count; });
	ON_SCOPE_EXIT
	{
		// 에디터 월드·원본 기록을 수정하지 않는다. 중간 실패에서도 구독을 남기지 않는다.
		Panel->OnPendingPostsChanged.Remove(BadgeHandle);
		Panel->Unbind();
		Panel->RemoveFromParent();
		Panel->ReleaseSlateResources(true);
	};
	if (!TestTrue(TEXT("실제 WBP는 통합 소식 모드"), Panel->bUnifiedNews)
		|| !TestNotNull(TEXT("실제 목록 스크롤"), Panel->Scroll_Posts.Get())) { return false; }
	Panel->bPreserveFeedWhileClosed = true;
	Panel->bAutoShowNewPosts = true;
	Panel->DisplayLimit = 80;
	Panel->HandlePanelOpened();

	// 원본 조회/구독 경로를 한 번 거친 뒤 이 임시 패널의 데이터만 비운다.
	// 운영 알림 원본은 Game/PIE 전용이므로 에디터 월드에는 없어도 된다.
	Panel->Feed = FFF_LiveSnsFeedState();
	Panel->RecentNews.Reset();
	Panel->UnreadNewsSerials.Reset();
	Panel->NewNewsSerials.Reset();
	Panel->NewMarkShownAt.Reset();
	Panel->bNewsInitialized = true;
	Panel->SyncFromViewModel();
	Panel->bFeedInitialized = true;

	const auto MakePost = [](int32 Number, bool bOperation)
	{
		FFF_LiveSocialPost Post;
		Post.Author = FText::FromString(bOperation ? TEXT("운영 알림 테스트") : TEXT("관객 테스트"));
		Post.Body = FText::Format(NSLOCTEXT("FFUnifiedNewsTest", "Body", "개별 읽음 테스트 글 {0}"), FText::AsNumber(Number));
		Post.bOperationAlert = bOperation;
		Post.OperationDay = bOperation ? 2 : 0;
		return Post;
	};
	const auto VerifyCounts = [this, &Panel, &BadgeCount](int32 Social, int32 Operation)
	{
		const int32 Total = Social + Operation;
		TestEqual(TEXT("전체 미읽음 수"), Panel->GetPendingPostCount(), Total);
		TestEqual(TEXT("도크에 전달한 수와 전체 수 일치"), BadgeCount, Total);
		TestEqual(TEXT("SNS 필터 미읽음 수"), Panel->CountUnreadNews(false), Social);
		TestEqual(TEXT("운영 알림 필터 미읽음 수"), Panel->CountUnreadNews(true), Operation);
		TestEqual(TEXT("빨간 표시와 미읽음의 수 일치"), Panel->NewNewsSerials.Num(), Total);
		for (int64 Serial : Panel->UnreadNewsSerials)
		{
			TestTrue(TEXT("미읽음 글에는 같은 번호의 빨간 표시가 있다"), Panel->NewNewsSerials.Contains(Serial));
		}
		TestEqual(TEXT("표시행과 도착 번호 수 일치"), Panel->PostEntries.Num(), Panel->Feed.DisplayedSerials.Num());
		for (int32 Index = 0; Index < Panel->PostEntries.Num() && Panel->Feed.DisplayedSerials.IsValidIndex(Index); ++Index)
		{
			const UFF_LiveSnsPostEntry* Row = Panel->PostEntries[Index];
			if (TestNotNull(TEXT("실제 글 행"), Row))
			{
				TestEqual(TEXT("실제 행의 빨간 표시와 미읽음 일치"), Row->HasNewMark(),
					Panel->UnreadNewsSerials.Contains(Panel->Feed.DisplayedSerials[Index]));
			}
		}
	};
	const auto FinishScrollRestore = [&Panel]()
	{
		Panel->ForceLayoutPrepass();
		for (int32 Frame = 0; Frame < 2; ++Frame)
		{
			Panel->TickScrollAnchor();
			Panel->RestoreNewsReadingPosition();
		}
	};
	const auto SelectFilter = [&Panel, &FinishScrollRestore](EFF_LiveSnsFilter Filter)
	{
		Panel->SetFilter(Filter);
		// 원본이 없는 에디터에서도 다음 도착을 열린 목록에 반영한다.
		Panel->bFeedInitialized = true;
		FinishScrollRestore();
	};

	Panel->RecordNews(MakePost(1, false));
	Panel->RecordNews(MakePost(2, true));
	Panel->RecordNews(MakePost(3, false));
	VerifyCounts(2, 1);
	if (!TestEqual(TEXT("세 글이 실제 행으로 생성됨"), Panel->PostEntries.Num(), 3)) { return false; }
	Panel->ForceLayoutPrepass();
	Panel->Scroll_Posts->SetScrollOffset(96.f);
	const float BeforeRead = Panel->Scroll_Posts->GetScrollOffset();
	const int64 MiddleSerial = Panel->Feed.DisplayedSerials[1];
	TestEqual(TEXT("중간 글은 두 번째 도착"), MiddleSerial, int64(2));
	Panel->PostEntries[1]->OnPostActivated.Broadcast(MiddleSerial);
	VerifyCounts(2, 0);
	TestFalse(TEXT("클릭한 중간 글만 읽음"), Panel->UnreadNewsSerials.Contains(2));
	TestTrue(TEXT("더 오래된 글은 미읽음 유지"), Panel->UnreadNewsSerials.Contains(1));
	TestTrue(TEXT("더 새로운 글은 미읽음 유지"), Panel->UnreadNewsSerials.Contains(3));
	TestEqual(TEXT("읽음 처리 시 스크롤 위치 유지"), Panel->Scroll_Posts->GetScrollOffset(), BeforeRead);
	Panel->PostEntries[1]->OnPostActivated.Broadcast(MiddleSerial);
	Panel->HandleNewsRead(999);
	VerifyCounts(2, 0);

	// 필터를 왕복해도 읽음 상태와 각 필터의 읽던 위치가 유지된다.
	SelectFilter(EFF_LiveSnsFilter::Social);
	Panel->Scroll_Posts->SetScrollOffset(48.f);
	SelectFilter(EFF_LiveSnsFilter::Operation);
	Panel->Scroll_Posts->SetScrollOffset(24.f);
	SelectFilter(EFF_LiveSnsFilter::All);
	TestTrue(TEXT("전체 필터의 위치 복원"), FMath::IsNearlyEqual(Panel->Scroll_Posts->GetScrollOffset(), BeforeRead, 0.1f));
	SelectFilter(EFF_LiveSnsFilter::Social);
	TestTrue(TEXT("SNS 필터의 위치 복원"), FMath::IsNearlyEqual(Panel->Scroll_Posts->GetScrollOffset(), 48.f, 0.1f));
	SelectFilter(EFF_LiveSnsFilter::Operation);
	TestTrue(TEXT("운영 필터의 위치 복원"), FMath::IsNearlyEqual(Panel->Scroll_Posts->GetScrollOffset(), 24.f, 0.1f));
	SelectFilter(EFF_LiveSnsFilter::All);
	VerifyCounts(2, 0);

	Panel->NewMarkClear = EFF_LiveSnsNewMarkClear::OnPanelClose;
	Panel->HandlePanelClosed();
	VerifyCounts(2, 0);
	Panel->HandlePanelOpened();
	FinishScrollRestore();
	Panel->bFeedInitialized = true;
	VerifyCounts(2, 0);
	TestTrue(TEXT("새 도착 없는 재열기에서 미읽음 때문에 위치가 초기화되지 않음"),
		FMath::IsNearlyEqual(Panel->Scroll_Posts->GetScrollOffset(), BeforeRead, 0.1f));

	Panel->NewMarkClear = EFF_LiveSnsNewMarkClear::AfterSeconds;
	Panel->NewMarkSeconds = 0.5f;
	for (int64 Serial : Panel->UnreadNewsSerials)
	{
		Panel->NewMarkShownAt.Add(Serial, FPlatformTime::Seconds() - 1.0);
	}
	Panel->TickNewMarks();
	Panel->ClearNewMarks();
	VerifyCounts(2, 0);

	// 닫힌 동안 다른 종류의 글이 도착해도 미읽음 수에 들어가며 현재 필터 위치를 유지한다.
	SelectFilter(EFF_LiveSnsFilter::Social);
	Panel->HandlePanelClosed();
	Panel->RecordNews(MakePost(4, true));
	VerifyCounts(2, 1);
	Panel->HandlePanelOpened();
	FinishScrollRestore();
	Panel->bFeedInitialized = true;
	VerifyCounts(2, 1);
	TestTrue(TEXT("닫힌 동안 다른 종류가 도착해도 읽던 SNS 위치 복원"),
		FMath::IsNearlyEqual(Panel->Scroll_Posts->GetScrollOffset(), 48.f, 0.1f));
	Panel->RecordNews(MakePost(5, false));
	FinishScrollRestore();
	VerifyCounts(3, 1);
	TestTrue(TEXT("열린 목록에 자동 추가된 글도 미읽음"), Panel->UnreadNewsSerials.Contains(5));

	// SNS 필터를 둔 채 운영 알림만 추가해 보관 한도를 넘긴다.
	// 필터 밖 도착으로 밀려난 SNS도 표시행과 두 Set에서 함께 제거되어야 한다.
	for (int32 Number = 6; Number <= 80; ++Number) { Panel->RecordNews(MakePost(Number, true)); }
	VerifyCounts(3, 76);
	Panel->RecordNews(MakePost(81, true));
	VerifyCounts(2, 77);
	TestFalse(TEXT("용량에서 밀려난 미읽음 글 제거"), Panel->UnreadNewsSerials.Contains(1));
	TestFalse(TEXT("필터 밖 도착으로 밀려난 SNS 행도 제거"), Panel->Feed.DisplayedSerials.Contains(1));
	Panel->RecordNews(MakePost(82, true));
	VerifyCounts(2, 78);
	for (int32 Number = 83; Number <= 85; ++Number) { Panel->RecordNews(MakePost(Number, true)); }
	VerifyCounts(0, 80);
	TestEqual(TEXT("두 원본을 합쳐 최근 80건만 보관"), Panel->RecentNews.Num(), 80);
	TestEqual(TEXT("보관 기간 밖 SNS는 화면에도 남지 않음"), Panel->Feed.DisplayedPosts.Num(), 0);
	for (int64 Serial : Panel->UnreadNewsSerials)
	{
		TestTrue(TEXT("보관 범위 안의 도착 번호만 미읽음에 남음"), Serial >= 6 && Serial <= 85);
	}
	Panel->HandleNewsRead(1);
	VerifyCounts(0, 80);
	return true;
}

#endif
