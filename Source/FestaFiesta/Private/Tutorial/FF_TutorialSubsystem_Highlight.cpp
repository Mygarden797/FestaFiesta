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
#include "Tutorial/FF_TutorialMarker.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"

void UFF_TutorialSubsystem::ApplyHighlight(const FName AnchorId)
{
	if (CurrentAnchor == AnchorId)
	{
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
