// Copyright Epic Games, Inc. All Rights Reserved.

/* 튜토리얼 서브시스템 중 "직접 해 보기" 조작 대기를 판정하는 부분(2026-10-07).

   카메라 회전 · 시점 전환 · 건축 카테고리 선택 · 설치 미리보기 시작 · 미리보기 회전 · 조합 성립 ·
   스태프 배정 · 시설 목록 복귀를 본다. 대사 진행(FF_TutorialSubsystem.cpp)과 강조(_Highlight.cpp)에서 나눴다.
   조작을 기다리는 동안 대사창이 비켜 설 배치 패널(RefreshDialogueLiftPanel)도 여기서 찾는다.

   ─── 왜 신호가 아니라 읽기인가 ─────────────────────────────────────────────
   건축 · 도감 · 스태프 · 건축 목록 UI 는 팀원 코드다. 튜토리얼을 위한 신호를 그쪽에 더하지 않고,
   이미 공개된 값(상태 · 선택된 시설 · 미리보기 방향 · 도감 항목 · 출동 가능 인원)을 짧은 주기로 읽는다.
   카메라 감시(TickCameraWatch)와 같은 방식이다.

   ─── 안 하는 것 ─────────────────────────────────────────────────────────────
   사건(병목 · 미아 …)을 일부러 일으켜 대응을 연습시키지 않는다. 손님 NPC 가 실제로 와야 생기는 일이라
   튜토리얼에서는 대사로만 설명한다(2026-10-07 결정).
*/

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_CameraTypes.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "TimerManager.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

bool UFF_TutorialSubsystem::IsBuildCategorySelected(const FName CategoryName) const
{
	const UFF_PrepBuildCategoryChoice* Chip = FindBuildCategoryChoice(CategoryName);
	return Chip && Chip->IsSelected();
}

FName UFF_TutorialSubsystem::ResolveFacilityDefinitionId(const FGuid& FacilityInstanceId) const
{
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const AFFZSFacilityActor* Facility = Zone ? Zone->FindFacility(FacilityInstanceId) : nullptr;
	return Facility ? Facility->GetFacilitySpec().FacilityDefinitionId : NAME_None;
}

void UFF_TutorialSubsystem::PrepareActionWatch(const FFF_DialogueLine& Line)
{
	UWorld* World = GetWorld();

	bViewModeBaselineValid = false;
	if (const AFF_FestivalPlayerController* Controller =
		World ? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController()) : nullptr)
	{
		ViewModeBaseline = static_cast<uint8>(Controller->GetViewMode());
		bViewModeBaselineValid = true;
	}

	// 미리보기 방향은 미리보기가 떠 있을 때 잡는다. 아직 카드를 안 눌렀으면 Tick 이 처음 볼 때 잡는다.
	bPlacementYawBaselineValid = false;

	// 기다릴 시설을 이 줄이 뜨기 전에 이미 놓았으면 바로 통과시킨다. 줄 전환 중이라 다음 틱에 알린다.
	if (Line.WaitCondition == EFF_TutorialWaitCondition::FacilityPlaced
		&& (Line.WaitTarget.IsNone() ? EarlyPlacedFacilityIds.Num() > 0 : EarlyPlacedFacilityIds.Contains(Line.WaitTarget)))
	{
		EarlyPlacedFacilityIds.Reset();
		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				NotifyAction(EFF_TutorialWaitCondition::FacilityPlaced);
			}));
		}
	}

	/* 카드를 눌러야 하는 줄이면 그 카드를 목록 앞으로 꺼낸다.

	   한 쪽에 카드가 6장뿐이라 기다리는 시설이 다음 쪽에 있을 수 있다. 그러면 노란 박스를 씌울 카드가
	   화면에 없다. 목록의 공개 함수(RequestFacilityFocus)가 그 카드가 보이게 쪽을 넘기고 거름을 푼다.
	*/
	if (Line.WaitCondition == EFF_TutorialWaitCondition::PlacementStarted && !Line.WaitTarget.IsNone())
	{
		TArray<UUserWidget*> Panels;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Panels, UFF_PrepPanelBuild::StaticClass(), false);
		for (UUserWidget* Widget : Panels)
		{
			UFF_PrepPanelBuild* Panel = Cast<UFF_PrepPanelBuild>(Widget);
			if (Panel && IsWidgetOnScreen(Panel))
			{
				Panel->RequestFacilityFocus(Line.WaitTarget);
				break;
			}
		}
	}
}

void UFF_TutorialSubsystem::RefreshDialogueLiftPanel()
{
	DialogueLiftPanel = nullptr;
	if (!IsWaitingForAction() || ActiveScope == EFF_TutorialScope::LobbyIntro)
	{
		return;
	}

	TArray<UUserWidget*> Panels;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Panels, UFF_PrepPanelBuild::StaticClass(), false);
	for (UUserWidget* Panel : Panels)
	{
		if (Panel && IsWidgetOnScreen(Panel))
		{
			DialogueLiftPanel = Panel;
			return;
		}
	}
}

void UFF_TutorialSubsystem::TickActionWatch()
{
	const FFF_DialogueLine& Line = GetCurrentLine();
	const EFF_TutorialWaitCondition Condition = Line.WaitCondition;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	switch (Condition)
	{
	case EFF_TutorialWaitCondition::CameraRotated:
	{
		// 기준점은 카메라 감시와 같이 잡는다(ResetCameraBaseline). 늦게 뜬 카메라면 지금 잡는다.
		const AFF_OverviewCameraPawn* Pawn = FindOverviewPawn();
		if (!Pawn)
		{
			return;
		}
		if (!bCameraBaselineValid)
		{
			ResetCameraBaseline();
			return;
		}

		// 한 칸(기본 45도)의 절반을 넘게 돌면 돌린 것으로 본다. 표시 방향이 돌아가는 도중에 넘어간다.
		const float Step = FMath::Max(1.f, Pawn->CameraSettings.YawSnapDegrees);
		const float Turned = FMath::Abs(FMath::FindDeltaAngleDegrees(CameraBaselineYaw, Pawn->GetCameraState().Yaw));
		if (Turned >= Step * 0.5f)
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::ViewToggled:
	{
		const AFF_FestivalPlayerController* Controller =
			Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController());
		if (!Controller)
		{
			return;
		}
		const uint8 Mode = static_cast<uint8>(Controller->GetViewMode());
		if (!bViewModeBaselineValid)
		{
			ViewModeBaseline = Mode;
			bViewModeBaselineValid = true;
			return;
		}
		if (Mode != ViewModeBaseline)
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::BuildCategorySelected:
		// 고른 카테고리는 패널을 다시 열어도 남는다. 이미 골라져 있으면 바로 넘어간다.
		if (!Line.WaitTarget.IsNone() && IsBuildCategorySelected(Line.WaitTarget))
		{
			NotifyAction(Condition);
		}
		return;

	case EFF_TutorialWaitCondition::PlacementStarted:
	{
		const UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
		if (Build && Build->GetBuildModeState() == EFF_BuildModeState::Placing
			&& (Line.WaitTarget.IsNone() || Build->GetSelectedFacilityDefinitionId() == Line.WaitTarget))
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::PlacementRotated:
	{
		const UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
		if (!Build || Build->GetBuildModeState() != EFF_BuildModeState::Placing)
		{
			// 미리보기가 내려갔다. 다시 띄우면 그때 방향을 새로 잡는다.
			bPlacementYawBaselineValid = false;
			return;
		}

		const float Yaw = Build->GetPlacementYawDegrees();
		if (!bPlacementYawBaselineValid)
		{
			PlacementYawBaseline = Yaw;
			bPlacementYawBaselineValid = true;
			return;
		}
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(PlacementYawBaseline, Yaw)) >= 1.f)
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::SynergyActive:
	{
		/* 도감 항목의 "지금 조건 충족" 을 본다. "발견" 은 보지 않는다.

		   발견은 판을 넘어 남아서(예전에 한 번 짝지었으면 이미 참이다) 지금 제대로 놓았는지 알 수 없다.
		*/
		const UFF_FacilityCodexSubsystem* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
		FFF_FacilityCodexEntry Entry;
		if (Codex && !Line.WaitTarget.IsNone() && Codex->FindEntry(Line.WaitTarget, Entry)
			&& (Entry.bConditionMet || Entry.bEffectApplied))
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::StaffAssigned:
	{
		// 안내 시설에 배정돼 출동할 수 있는 스태프가 생겼는가. 배정 전에는 0 이다.
		const UFF_StaffFacilitySubsystem* Staff = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
		if (Staff && Staff->GetDispatchableStaffCount() > 0)
		{
			NotifyAction(Condition);
		}
		return;
	}

	case EFF_TutorialWaitCondition::BuildCatalogOpened:
	{
		const UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
		if (Build && Build->GetBuildModeState() == EFF_BuildModeState::CatalogOpen)
		{
			NotifyAction(Condition);
		}
		return;
	}

	default:
		return;
	}
}
