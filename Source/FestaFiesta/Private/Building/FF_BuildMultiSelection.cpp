// FF_BuildMultiSelection.cpp
// 역할: 여러 시설의 선택 목록, 화면 사각형 선택, 선택 표시와 전용 위젯의 수명을 관리한다.
// 키보드와 마우스 입력의 전달은 BuildModeSubsystem이 담당하고 실제 목록 변경은 여기서 처리한다.

#include "Building/FF_BuildModeSubsystem.h"

#include "Building/FF_BuildMultiSelectionModel.h"
#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 시설의 선택 상태와 화면의 테두리 표시를 항상 함께 바꾼다.
	void SetFacilitySelection(AFFZSFacilityActor& Facility, const bool bSelected)
	{
		Facility.SetBuildModeActionSelected(bSelected);
		Facility.SetSelectionIndicatorState(bSelected
			? EFFZSFacilitySelectionIndicatorState::Selected
			: EFFZSFacilitySelectionIndicatorState::Hidden);
	}

	// 목록 비우기와 모드 종료가 같은 정리 규칙을 사용한다. 알림은 호출자가 정리 완료 후 보낸다.
	void ResetFacilitySelections(TArray<TWeakObjectPtr<AFFZSFacilityActor>>& Facilities)
	{
		for (const TWeakObjectPtr<AFFZSFacilityActor>& Candidate : Facilities)
		{
			if (AFFZSFacilityActor* Facility = Candidate.Get())
			{
				SetFacilitySelection(*Facility, false);
			}
		}
		Facilities.Reset();
	}
}

int32 UFF_BuildModeSubsystem::GetMultiSelectionCount() const
{
	// 화면의 숫자만 갱신할 때는 시설 목록을 새로 할당하지 않고 유효한 선택 수만 센다.
	int32 Count = 0;
	for (const TWeakObjectPtr<AFFZSFacilityActor>& Candidate : MultiSelectedFacilities)
	{
		if (IsRegisteredMultiSelectionFacility(Candidate.Get()))
		{
			++Count;
		}
	}
	return Count;
}

// 철거되었거나 등록이 풀린 시설은 화면에 전달할 목록에서도 제외한다.
TArray<AFFZSFacilityActor*> UFF_BuildModeSubsystem::GetMultiSelectedFacilities() const
{
	TArray<AFFZSFacilityActor*> Result;
	Result.Reserve(MultiSelectedFacilities.Num());
	for (const TWeakObjectPtr<AFFZSFacilityActor>& Candidate : MultiSelectedFacilities)
	{
		AFFZSFacilityActor* Facility = Candidate.Get();
		if (IsRegisteredMultiSelectionFacility(Facility)) Result.Add(Facility);
	}
	return Result;
}

// 포인터가 남아 있어도 현재 월드가 관리하지 않는 시설은 선택할 수 없다.
bool UFF_BuildModeSubsystem::IsRegisteredMultiSelectionFacility(const AFFZSFacilityActor* Facility) const
{
	if (!IsValid(Facility) || Facility->GetFacilityState() == EFFZSFacilityState::Removed) return false;
	const FGuid Id = Facility->GetFacilityInstanceId();
	const UFFZSWorldSubsystem* Zones = GetZoneSystem();
	return Zones && Id.IsValid() && Zones->FindFacility(Id) == Facility;
}

// 지정한 화면이 없거나 잘못된 종류면 기본 C++ 위젯으로 선택 기능을 유지한다.
void UFF_BuildModeSubsystem::CreateMultiSelectionWidget(APlayerController& Controller)
{
	if (MultiSelectionWidget) return;
	UClass* WidgetClass = MultiSelectionWidgetClass.LoadSynchronous();
	if (!WidgetClass || !WidgetClass->IsChildOf(UFF_BuildMultiSelectionWidget::StaticClass()))
		WidgetClass = UFF_BuildMultiSelectionWidget::StaticClass();
	MultiSelectionWidget = CreateWidget<UFF_BuildMultiSelectionWidget>(&Controller, WidgetClass);
	if (MultiSelectionWidget && !MultiSelectionWidget->AddToPlayerScreen(94))
	{
		MultiSelectionWidget = nullptr;
	}
}

void UFF_BuildModeSubsystem::RemoveMultiSelectionWidget()
{
	if (!MultiSelectionWidget) return;
	MultiSelectionWidget->RemoveFromParent();
	MultiSelectionWidget = nullptr;
}

void UFF_BuildModeSubsystem::ClearMultiSelection()
{
	if (MultiSelectedFacilities.IsEmpty()) return;
	ResetFacilitySelections(MultiSelectedFacilities);
	OnBuildMultiSelectionChanged.Broadcast();
	OnBuildToolsChanged.Broadcast();
}

// 드래그, 선택 표시, 전용 화면을 모두 정리한 다음 외부 화면에 한 번씩 알린다.
void UFF_BuildModeSubsystem::EndMultiSelection()
{
	if (!bMultiSelectionActive && MultiSelectedFacilities.IsEmpty() && !MultiSelectionWidget) return;
	bMultiSelectionPointerArmed = false;
	bMultiSelectionMarqueeVisible = false;
	if (MultiSelectionWidget)
	{
		MultiSelectionWidget->SetMarquee(false, MultiSelectionDragStartPixels, MultiSelectionDragCurrentPixels);
	}
	bMultiSelectionActive = false;
	ResetFacilitySelections(MultiSelectedFacilities);
	RemoveMultiSelectionWidget();
	OnBuildMultiSelectionChanged.Broadcast();
	OnBuildToolsChanged.Broadcast();
}

// 일시정지 중에는 전환하지 않는다. 시작할 때 일반 건축 상태로 먼저 돌아온다.
void UFF_BuildModeSubsystem::ToggleMultiSelection()
{
	if (bInputSuspendedForPauseMenu || bIsExitingBuildMode ||
		!FFBuildModeModel::IsBuildSessionActive(State)) return;
	NotifyBuildToolsInteraction();
	if (bMultiSelectionActive)
	{
		EndMultiSelection();
		ShowStatus(TEXT("다중 선택을 종료했습니다."), false, 1.5f);
		return;
	}
	if (State != EFF_BuildModeState::BuildMode) EnterBuildMode();
	if (State != EFF_BuildModeState::BuildMode) return;
	ClearPendingRemovalSelection();
	bMultiSelectionActive = true;
	if (APlayerController* Controller = GetLocalPlayerController()) CreateMultiSelectionWidget(*Controller);
	OnBuildMultiSelectionChanged.Broadcast();
	OnBuildToolsChanged.Broadcast();
	ShowStatus(TEXT("다중 선택 · 시설 클릭으로 선택/해제 · 드래그로 여러 시설 선택"), false, 3.0f);
}

// 여러 시설 보기에서 한 시설의 상세 보기로 전환한다.
bool UFF_BuildModeSubsystem::InspectMultiSelectedFacility(AFFZSFacilityActor* Facility)
{
	if (!bMultiSelectionActive || !IsRegisteredMultiSelectionFacility(Facility) ||
		!MultiSelectedFacilities.Contains(TWeakObjectPtr<AFFZSFacilityActor>(Facility))) return false;
	EndMultiSelection();
	return InspectFacility(Facility);
}

void UFF_BuildModeSubsystem::AddMultiSelectedFacility(AFFZSFacilityActor* Facility)
{
	if (!bMultiSelectionActive || !IsRegisteredMultiSelectionFacility(Facility) ||
		MultiSelectedFacilities.Contains(TWeakObjectPtr<AFFZSFacilityActor>(Facility))) return;
	MultiSelectedFacilities.Add(Facility);
	SetFacilitySelection(*Facility, true);
	OnBuildMultiSelectionChanged.Broadcast();
	OnBuildToolsChanged.Broadcast();
}

void UFF_BuildModeSubsystem::ToggleMultiSelectedFacility(AFFZSFacilityActor* Facility)
{
	if (!bMultiSelectionActive || !IsRegisteredMultiSelectionFacility(Facility)) return;
	if (MultiSelectedFacilities.Remove(TWeakObjectPtr<AFFZSFacilityActor>(Facility)) > 0)
	{
		SetFacilitySelection(*Facility, false);
		OnBuildMultiSelectionChanged.Broadcast();
		OnBuildToolsChanged.Broadcast();
		return;
	}
	AddMultiSelectedFacility(Facility);
}

// 외부에서 삭제된 시설을 뒤에서부터 제거해 검사하지 않은 항목을 건너뛰지 않는다.
void UFF_BuildModeSubsystem::PruneMultiSelectedFacilities()
{
	bool bChanged = false;
	for (int32 Index = MultiSelectedFacilities.Num() - 1; Index >= 0; --Index)
	{
		AFFZSFacilityActor* Facility = MultiSelectedFacilities[Index].Get();
		if (IsRegisteredMultiSelectionFacility(Facility)) continue;
		if (Facility)
		{
			SetFacilitySelection(*Facility, false);
		}
		MultiSelectedFacilities.RemoveAtSwap(Index);
		bChanged = true;
	}
	if (bChanged)
	{
		OnBuildMultiSelectionChanged.Broadcast();
		OnBuildToolsChanged.Broadcast();
	}
}

// 시설 외곽의 여덟 꼭짓점을 화면 좌표로 바꾸어 드래그 사각형과 겹치는 시설을 선택한다.
void UFF_BuildModeSubsystem::SelectFacilitiesInScreenRect(
	APlayerController& Controller, const FBox2D& ScreenRect)
{
	if (!bMultiSelectionActive || State != EFF_BuildModeState::BuildMode) return;
	UWorld* World = GetWorld();
	if (!World) return;
	bool bChanged = false;
	for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
	{
		AFFZSFacilityActor* Facility = *It;
		if (!IsRegisteredMultiSelectionFacility(Facility) ||
			MultiSelectedFacilities.Contains(TWeakObjectPtr<AFFZSFacilityActor>(Facility))) continue;

		const FFFZSFacilitySpec& Spec = Facility->GetFacilitySpec();
		const FVector Extent(FMath::Max(1, Spec.FootprintSizeCells.X) * 0.5f,
			FMath::Max(1, Spec.FootprintSizeCells.Y) * 0.5f,
			FMath::Max(1.0f, Spec.PlacementObstacleHeightCm));
		const FTransform Transform = Facility->GetActorTransform();
		FVector2D ProjectedMin(FLT_MAX, FLT_MAX);
		FVector2D ProjectedMax(-FLT_MAX, -FLT_MAX);
		bool bProjected = false;
		for (const float X : { -1.0f, 1.0f })
		for (const float Y : { -1.0f, 1.0f })
		for (const float Z : { -1.0f, 1.0f })
		{
			FVector2D ScreenPoint;
			const FVector WorldPoint = Transform.TransformPosition(
				FVector(Extent.X * X, Extent.Y * Y, Z < 0.0f ? 0.0f : Extent.Z));
			if (!Controller.ProjectWorldLocationToScreen(WorldPoint, ScreenPoint, true)) continue;
			ProjectedMin.X = FMath::Min(ProjectedMin.X, ScreenPoint.X);
			ProjectedMin.Y = FMath::Min(ProjectedMin.Y, ScreenPoint.Y);
			ProjectedMax.X = FMath::Max(ProjectedMax.X, ScreenPoint.X);
			ProjectedMax.Y = FMath::Max(ProjectedMax.Y, ScreenPoint.Y);
			bProjected = true;
		}
		if (!bProjected || !FFBuildMultiSelectionModel::Intersects(
			ScreenRect, ProjectedMin, ProjectedMax)) continue;
		MultiSelectedFacilities.Add(Facility);
		SetFacilitySelection(*Facility, true);
		bChanged = true;
	}
	if (bChanged)
	{
		OnBuildMultiSelectionChanged.Broadcast();
		OnBuildToolsChanged.Broadcast();
	}
}
