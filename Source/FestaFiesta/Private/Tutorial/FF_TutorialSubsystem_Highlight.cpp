// Copyright Epic Games, Inc. All Rights Reserved.

/* 튜토리얼 서브시스템 중 강조(하이라이트)와 강조 대상 등록을 맡는 부분.

   한 파일이 1,500줄을 넘어서 나눴다. 대사 진행(FF_TutorialSubsystem.cpp)과
   화면·레벨의 무엇을 가리킬지 찾는 일은 서로 거의 부르지 않아서 경계가 깔끔하다.
   클래스는 그대로 하나다. 파일만 나눴다.
*/

#include "Tutorial/FF_TutorialSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Tutorial/FF_TutorialMarker.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildCategoryChoice.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"

namespace
{
	// 다시 찾아야 하는 강조 이름의 앞머리. 뒤에는 카테고리 이름 · 시설 정의 이름이 붙는다.
	const TCHAR* const BuildCategoryAnchorPrefix = TEXT("BuildCategory_");
	const TCHAR* const BuildCardAnchorPrefix = TEXT("BuildCard_");
	const TCHAR* const FacilityInfoAnchorPrefix = TEXT("FacilityInfo_");
	const TCHAR* const BuildToolsAnchorPrefix = TEXT("BuildTools_");
}

void UFF_TutorialSubsystem::ApplyHighlight(const FName AnchorId)
{
	if (CurrentAnchor == AnchorId)
	{
		return;
	}

	/* 다시 만들어지는 위젯(카테고리 버튼 · 목록 카드 · 시설 정보창 · 건축 도구)은 지금 찾아 보고, 없어도 알리지 않는다.

	   이런 위젯은 대사가 뜬 뒤에 열리는 경우가 많다(배치 탭을 막 연 순간 · 시설을 눌러 정보창을 열기 전).
	   Tick 이 짧은 주기로 다시 찾으므로(RefreshDynamicAnchor) 나타나는 대로 박스가 씌워진다.
	*/
	if (IsDynamicAnchor(AnchorId))
	{
		if (UWidget* Found = FindDynamicAnchorWidget(AnchorId))
		{
			AnchorWidgets.Add(AnchorId, Found);
		}
		else
		{
			AnchorWidgets.Remove(AnchorId);
		}
		DynamicAnchorScanCountdown = 0.1f;
		CurrentAnchor = AnchorId;
		OnHighlightAnchorChanged.Broadcast(AnchorId);
		return;
	}

	// 화면이 닫히면 위젯이 사라진다. 약참조라 크래시는 안 나지만 등록은 남아 있어서
	// 다시 훑지 않게 된다. 가리킬 대상을 바꾸는 이 시점에 치운다.
	if (!AnchorId.IsNone())
	{
		if (const TWeakObjectPtr<UWidget>* Found = AnchorWidgets.Find(AnchorId))
		{
			if (!Found->IsValid())
			{
				AnchorWidgets.Remove(AnchorId);
			}
		}

		// 아직 등록되지 않았으면 지금 훑는다.
		//
		// 매 프레임 훑으면 낭비고, 시작할 때 한 번만 하면 화면이 나중에 뜰 때 놓친다.
		// 필요해진 순간에만 찾으면 둘 다 피한다.
		if (!AnchorWidgets.Contains(AnchorId))
		{
			ScanPrepHudAnchors();
		}

		// 훑고도 없고 레벨 마커에도 없으면 대사에 적은 이름이 틀린 것이다.
		// 조용히 넘어가면 강조가 왜 안 뜨는지 알 수가 없다.
		if (!AnchorWidgets.Contains(AnchorId))
		{
			FVector Unused = FVector::ZeroVector;
			float UnusedRadius = 0.f;
			if (!FindAnchorWorldPoint(AnchorId, Unused, UnusedRadius))
			{
				UE_LOG(LogFestaFiestaTutorial, Warning,
					TEXT("[튜토리얼] 강조 대상 '%s' 를 UI 에서도 레벨에서도 찾지 못했다."),
					*AnchorId.ToString());
			}
		}
	}

	CurrentAnchor = AnchorId;
	OnHighlightAnchorChanged.Broadcast(AnchorId);
}

void UFF_TutorialSubsystem::RegisterAnchor(const FName AnchorId, UWidget* Widget)
{
	if (AnchorId.IsNone() || !Widget)
	{
		return;
	}
	// 같은 이름이 이미 있으면 덮는다. 화면이 다시 뜨면 위젯이 새로 만들어지므로
	// 옛 참조를 그대로 두면 사라진 위젯을 가리킨다.
	AnchorWidgets.Add(AnchorId, Widget);

	// 지금 가리키려던 대상이 방금 들어왔으면 다시 알린다.
	// 화면이 대사보다 늦게 뜨는 경우가 있어서, 그때 강조가 조용히 빠진다.
	if (CurrentAnchor == AnchorId)
	{
		OnHighlightAnchorChanged.Broadcast(AnchorId);
	}
}

void UFF_TutorialSubsystem::UnregisterAnchor(const FName AnchorId)
{
	AnchorWidgets.Remove(AnchorId);
}

UWidget* UFF_TutorialSubsystem::FindAnchorWidget(const FName AnchorId) const
{
	// 조회는 읽기만 한다. 죽은 참조 정리는 ApplyHighlight 에서 한다.
	//
	// 조회 함수가 상태를 바꾸면 어디서 무엇이 지워지는지 추적하기 어렵다.
	const TWeakObjectPtr<UWidget>* Found = AnchorWidgets.Find(AnchorId);
	return Found ? Found->Get() : nullptr;
}

namespace
{
	// 탭 종류를 대사에서 쓸 이름으로. 열거형 이름을 그대로 따서 오타가 끼지 않게 한다.
	FName MakePrepTabAnchorId(const EFF_PrepHudTab Tab)
	{
		const UEnum* EnumClass = StaticEnum<EFF_PrepHudTab>();
		if (!EnumClass)
		{
			return NAME_None;
		}

		// "EFF_PrepHudTab::Build" 가 아니라 "Build" 만 쓴다.
		const FString Short = EnumClass->GetNameStringByValue(static_cast<int64>(Tab));
		return Short.IsEmpty() ? NAME_None : FName(*FString::Printf(TEXT("PrepTab_%s"), *Short));
	}
}

int32 UFF_TutorialSubsystem::ScanPrepHudAnchors()
{
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this, FoundWidgets, UFF_PrepHudTabButton::StaticClass(), false);

	int32 Registered = 0;

	for (UUserWidget* Widget : FoundWidgets)
	{
		UFF_PrepHudTabButton* TabButton = Cast<UFF_PrepHudTabButton>(Widget);
		if (!TabButton)
		{
			continue;
		}

		const FName AnchorId = MakePrepTabAnchorId(TabButton->GetTab());
		if (AnchorId.IsNone())
		{
			continue;
		}

		RegisterAnchor(AnchorId, TabButton);
		++Registered;
	}

	/* 준비 화면의 잔액도 강조 대상으로 등록한다. 예산을 설명할 때 가리킨다(PrepHud_Gold).

	   탭처럼 전용 위젯 클래스가 없어서 이름으로 찾는다. 아이콘까지 묶은 Box_Gold 가 있으면
	   그것을, 없으면 숫자(Text_Gold)만 가리킨다. 숫자만 잡혀도 여백을 두고 그리므로 알아볼 수 있다.
	   UI 파일을 고치지 않으려고 공개 조회(GetWidgetFromName)만 쓴다.
	*/
	TArray<UUserWidget*> Bars;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Bars, UFF_PrepHudBar::StaticClass(), false);
	for (UUserWidget* Bar : Bars)
	{
		UWidget* Gold = Bar ? Bar->GetWidgetFromName(TEXT("Box_Gold")) : nullptr;
		if (!Gold && Bar)
		{
			Gold = Bar->GetWidgetFromName(TEXT("Text_Gold"));
		}

		// 화면에 보이는 바를 쓴다. 옛 바가 숨은 채 남아 있으면 엉뚱한 자리를 가리킨다.
		if (Gold && Gold->IsVisible())
		{
			RegisterAnchor(TEXT("PrepHud_Gold"), Gold);
			++Registered;
			break;
		}
	}

	if (Registered > 0)
	{
		UE_LOG(LogFestaFiestaTutorial, Log,
			TEXT("[튜토리얼] 준비 화면에서 %d개를 강조 대상으로 등록했다."), Registered);
	}

	return Registered;
}

bool UFF_TutorialSubsystem::IsDynamicAnchor(const FName AnchorId)
{
	if (AnchorId.IsNone())
	{
		return false;
	}
	const FString Id = AnchorId.ToString();
	return Id.StartsWith(BuildCategoryAnchorPrefix) || Id.StartsWith(BuildCardAnchorPrefix)
		|| Id.StartsWith(FacilityInfoAnchorPrefix) || Id.StartsWith(BuildToolsAnchorPrefix);
}

bool UFF_TutorialSubsystem::IsWidgetOnScreen(const UWidget* Widget)
{
	if (!Widget || !Widget->GetCachedWidget().IsValid())
	{
		return false;
	}

	// 자기 자신부터 화면 맨 위까지 하나라도 숨어 있으면 안 보인다. 접힌 패널 안의 카드는 옛 자리를 들고 있다.
	TSharedPtr<SWidget> Slate = Widget->GetCachedWidget();
	for (int32 Depth = 0; Slate.IsValid() && Depth < 128; ++Depth)
	{
		if (!Slate->GetVisibility().IsVisible())
		{
			return false;
		}
		Slate = Slate->GetParentWidget();
	}
	return true;
}

UFF_PrepBuildCategoryChoice* UFF_TutorialSubsystem::FindBuildCategoryChoice(const FName CategoryName) const
{
	const UEnum* Categories = StaticEnum<EFFZSFacilityCategory>();
	const int64 Wanted = Categories ? Categories->GetValueByNameString(CategoryName.ToString()) : INDEX_NONE;
	if (Wanted == INDEX_NONE)
	{
		return nullptr;
	}

	TArray<UUserWidget*> Chips;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Chips, UFF_PrepBuildCategoryChoice::StaticClass(), false);
	for (UUserWidget* Widget : Chips)
	{
		UFF_PrepBuildCategoryChoice* Chip = Cast<UFF_PrepBuildCategoryChoice>(Widget);
		if (Chip && Chip->GetChoiceId() == Wanted && IsWidgetOnScreen(Chip))
		{
			return Chip;
		}
	}
	return nullptr;
}

UWidget* UFF_TutorialSubsystem::FindDynamicAnchorWidget(const FName AnchorId) const
{
	const FString Id = AnchorId.ToString();

	// 배치 탭의 카테고리 버튼. BuildCategory_Performance 처럼 카테고리 열거형 이름을 붙인다.
	if (Id.StartsWith(BuildCategoryAnchorPrefix))
	{
		return FindBuildCategoryChoice(FName(*Id.RightChop(FCString::Strlen(BuildCategoryAnchorPrefix))));
	}

	// 배치 목록의 시설 카드. BuildCard_Booth_FoodStall_Burger 처럼 시설 정의 이름을 붙인다.
	if (Id.StartsWith(BuildCardAnchorPrefix))
	{
		const FName FacilityId(*Id.RightChop(FCString::Strlen(BuildCardAnchorPrefix)));
		TArray<UUserWidget*> Cards;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Cards, UFF_PrepBuildTileWidget::StaticClass(), false);
		for (UUserWidget* Widget : Cards)
		{
			const UFF_PrepBuildTileWidget* Card = Cast<UFF_PrepBuildTileWidget>(Widget);
			const UFF_PrepBuildTileData* Item = Card ? Card->GetPresentedItem() : nullptr;
			if (Item && Item->GetFacilityDefinitionId() == FacilityId && IsWidgetOnScreen(Card))
			{
				return Widget;
			}
		}
		return nullptr;
	}

	// 화면 하나 안의 이름 붙은 단추. 그 화면의 공개 조회(GetWidgetFromName)로만 찾는다.
	const auto FindNamedButton = [this](UClass* ScreenClass, const FString& ButtonName) -> UWidget*
	{
		TArray<UUserWidget*> Screens;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Screens, ScreenClass, false);
		for (UUserWidget* Screen : Screens)
		{
			UWidget* Button = Screen ? Screen->GetWidgetFromName(FName(*ButtonName)) : nullptr;
			if (Button && IsWidgetOnScreen(Button))
			{
				return Button;
			}
		}
		return nullptr;
	};

	// 시설 정보창 안의 단추. FacilityInfo_AssignStaff 는 "+ 배정"(Button_AssignStaff)이다.
	if (Id.StartsWith(FacilityInfoAnchorPrefix))
	{
		return FindNamedButton(UFF_FacilityInfoPanel::StaticClass(),
			TEXT("Button_") + Id.RightChop(FCString::Strlen(FacilityInfoAnchorPrefix)));
	}

	// 설치 미리보기 중 오른쪽 아래 도구 단추. BuildTools_Catalog 는 "시설 목록"(Button_Catalog)이다.
	if (Id.StartsWith(BuildToolsAnchorPrefix))
	{
		return FindNamedButton(UFF_BuildToolsWidget::StaticClass(),
			TEXT("Button_") + Id.RightChop(FCString::Strlen(BuildToolsAnchorPrefix)));
	}

	return nullptr;
}

void UFF_TutorialSubsystem::RefreshDynamicAnchor()
{
	if (!IsDynamicAnchor(CurrentAnchor))
	{
		return;
	}

	UWidget* Found = FindDynamicAnchorWidget(CurrentAnchor);
	if (Found == FindAnchorWidget(CurrentAnchor))
	{
		return;
	}

	// 바뀐 때만 알린다. 매 주기 알리면 화면이 박스를 계속 새로 그린다.
	if (Found)
	{
		RegisterAnchor(CurrentAnchor, Found);
	}
	else
	{
		AnchorWidgets.Remove(CurrentAnchor);
		OnHighlightAnchorChanged.Broadcast(CurrentAnchor);
	}
}

bool UFF_TutorialSubsystem::FindAnchorWorldPoint(
	const FName AnchorId, FVector& OutLocation, float& OutRadius) const
{
	if (AnchorId.IsNone())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 마커는 몇 개 안 되므로 매번 훑어도 된다. 캐시해두면 레벨 스트리밍으로
	// 마커가 늦게 뜰 때 영영 못 찾는다.
	for (TActorIterator<AFF_TutorialMarker> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AFF_TutorialMarker* Marker = *It;
		if (!IsValid(Marker) || Marker->ResolveAnchorId() != AnchorId)
		{
			continue;
		}

		OutLocation = Marker->GetActorLocation();
		OutRadius = Marker->RadiusCm;
		return true;
	}

	return false;
}
