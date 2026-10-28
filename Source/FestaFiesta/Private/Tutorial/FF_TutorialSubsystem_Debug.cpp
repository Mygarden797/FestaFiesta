// Copyright Epic Games, Inc. All Rights Reserved.

/* 튜토리얼 서브시스템의 개발용 콘솔 명령(FF.Tutorial.*).

   게임 흐름과 상관없는 코드라 따로 뒀다. 대사 진행을 읽을 때 백 줄 넘는 진단 로그를
   건너뛰지 않아도 된다. 배포 빌드에서는 등록 자체가 안 된다(FFF_DebugConsoleCommands).
*/

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Camera/FF_FestivalGameMode.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tutorial/FF_TutorialMarker.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

void UFF_TutorialSubsystem::RegisterDebugConsoleCommands()
{
	// 개발용이다. 배포 빌드에서 콘솔로 건너뛰면 안내가 전역으로 꺼진 채 저장된다.
#if !UE_BUILD_SHIPPING
	UnregisterDebugConsoleCommands();

	DebugCommands.Add(
		TEXT("FF.Tutorial.Skip"),
		TEXT("진행 중인 대사를 끝낸다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			StopScript();
		}));

	DebugCommands.Add(
		TEXT("FF.Tutorial.SEnding"),
		TEXT("S 엔딩 장면을 지금 튼다(신뢰도가 높으면 신뢰 줄도). 로비에서 쓴다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			PlaySGradeEnding();
		}));

	DebugCommands.Add(
		TEXT("FF.Tutorial.SkipAll"),
		TEXT("튜토리얼을 통째로 건너뛴다. 확인 없이 바로 끝난다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			SkipTutorial();
		}));

	DebugCommands.Add(
		TEXT("FF.Tutorial.Highlight"),
		TEXT("강조를 직접 켜본다. 사용법: FF.Tutorial.Highlight PrepTab_Build (인자 없으면 해제)"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			// 대사 흐름과 상관없이 강조만 확인할 수 있어야 한다.
			// 대사를 끝까지 넘겨가며 확인하면 한 번 보는 데 시간이 오래 걸린다.
			const FName AnchorId = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
			ApplyHighlight(AnchorId);

			// 로그 인자 안에서 함수를 부르면 형식 문자열 검사에 걸린다.
			// 값을 먼저 구해두고 넘긴다.
			FVector MarkerLocation = FVector::ZeroVector;
			float MarkerRadius = 0.f;
			const bool bHasMarker = FindAnchorWorldPoint(AnchorId, MarkerLocation, MarkerRadius);
			const bool bHasWidget = FindAnchorWidget(AnchorId) != nullptr;

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("[튜토리얼] 강조 = %s / UI 등록 %s / 레벨 마커 %s"),
				AnchorId.IsNone() ? TEXT("해제") : *AnchorId.ToString(),
				bHasWidget ? TEXT("있음") : TEXT("없음"),
				bHasMarker ? TEXT("있음") : TEXT("없음"));
		}));

	DebugCommands.Add(
		TEXT("FF.Tutorial.Stat"),
		TEXT("대사 진행 상태를 출력한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("[튜토리얼] 진행=%s Script=%s 줄=%d/%d"),
				bDialogueActive ? TEXT("예") : (bTutorialSkipped ? TEXT("건너뜀") : TEXT("아니오")),
				ActiveScriptId.IsNone() ? TEXT("없음") : *ActiveScriptId.ToString(),
				CurrentLineIndex,
				ActiveLines.Num());

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("  조작 대기=%s 조건=%d 강조=%s 등록된 UI 대상 %d개"),
				IsWaitingForAction() ? TEXT("예") : TEXT("아니오"),
				static_cast<int32>(GetCurrentLine().WaitCondition),
				CurrentAnchor.IsNone() ? TEXT("없음") : *CurrentAnchor.ToString(),
				AnchorWidgets.Num());

			// 무엇을 구독했는지 본다. 안 붙었으면 조작해도 안 넘어간다.
			// 어디서 대사를 가져왔는지 본다. 두 군데서 올 수 있어 헷갈리기 쉽다.
			const UWorld* World = GetWorld();
			const AFF_FestivalGameMode* GameMode =
				World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr;
			const bool bLevelTable = GameMode && !GameMode->TutorialStartTable.IsNull();
			const bool bLevelScript = GameMode && !GameMode->TutorialStartScript.IsNull();

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("  시작 대사 출처: %s"),
				bLevelTable ? TEXT("GameMode 표")
				: bLevelScript ? TEXT("GameMode 애셋")
				: TEXT("Project Settings 또는 없음"));

			// 레벨에 마커가 몇 개 있는지. 강조가 안 될 때 여기부터 본다.
			int32 MarkerCount = 0;
			if (const UWorld* MarkerWorld = GetWorld())
			{
				for (TActorIterator<AFF_TutorialMarker> It(const_cast<UWorld*>(MarkerWorld)); It; ++It)
				{
					if (IsValid(*It))
					{
						++MarkerCount;
					}
				}
			}

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("  레벨의 세계 마커 %d개"), MarkerCount);

			// 어떤 이름이 등록됐는지 본다. 대사에 적은 이름과 다르면 강조가 안 된다.
			if (AnchorWidgets.Num() > 0)
			{
				TArray<FString> Names;
				Names.Reserve(AnchorWidgets.Num());
				for (const TPair<FName, TWeakObjectPtr<UWidget>>& Pair : AnchorWidgets)
				{
					Names.Add(FString::Printf(TEXT("%s%s"),
						*Pair.Key.ToString(),
						Pair.Value.IsValid() ? TEXT("") : TEXT("(죽음)")));
				}
				UE_LOG(LogFestaFiestaTutorial, Display,
					TEXT("  등록된 이름: %s"), *FString::Join(Names, TEXT(", ")));
			}

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("  탭 바 구독=%s"),
				BoundPrepHudBar.IsValid() ? TEXT("예") : TEXT("아니오"));

			UE_LOG(LogFestaFiestaTutorial, Display,
				TEXT("  구독: Zone=%s 단계=%s / 카메라 기준점=%s"),
				bBoundZoneWorld ? TEXT("예") : TEXT("아니오"),
				bBoundStage ? TEXT("예") : TEXT("아니오"),
				bCameraBaselineValid ? TEXT("있음") : TEXT("없음"));
		}));
#endif
}

void UFF_TutorialSubsystem::UnregisterDebugConsoleCommands()
{
#if !UE_BUILD_SHIPPING
	// 내가 건 것만 지운다. 규칙은 FFF_DebugConsoleCommands 주석 참고.
	DebugCommands.UnregisterAll();
#endif
}
