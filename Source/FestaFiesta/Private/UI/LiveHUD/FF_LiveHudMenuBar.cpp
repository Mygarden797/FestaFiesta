// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudMenuBar.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

// =============================================================================
// UFF_LiveHudTabButton
// =============================================================================

void UFF_LiveHudTabButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Root)
	{
		Button_Root->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudTabButton::HandleClicked);
	}
	else
	{
		// 이게 없으면 탭이 눌리지 않는데 화면은 정상으로 보인다. 조용히 넘기면 원인을 못 찾는다.
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Button_Root가 없어 탭을 누를 수 없다."), *GetName());
	}

	RefreshVisualState();
}

void UFF_LiveHudTabButton::Configure(EFF_LiveHudTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush)
{
	Tab = InTab;

	if (Text_Label)
	{
		Text_Label->SetText(InLabel);
	}

	// 리소스가 없는 브러시를 넣으면 아이콘이 빈 사각형이 된다. 그때는 WBP 기본값을 유지한다.
	if (Image_Icon && InIconBrush.GetResourceObject())
	{
		Image_Icon->SetBrush(InIconBrush);
	}

	RefreshVisualState();
}

void UFF_LiveHudTabButton::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisualState();
}

void UFF_LiveHudTabButton::RefreshVisualState()
{
	// 상태 표현을 한 곳에서 "전체를 다시 선언"한다. 어떤 순서로 호출돼도 결과가 같다(멱등).
	const FLinearColor ContentColor = bSelected ? SelectedContentColor : NormalContentColor;

	if (Text_Label)
	{
		Text_Label->SetColorAndOpacity(FSlateColor(ContentColor));
	}

	if (Image_Icon)
	{
		// 흰색 단색 아이콘을 틴트한다. 상태별로 텍스처를 따로 두지 않는다.
		Image_Icon->SetColorAndOpacity(ContentColor);
	}

	if (!Button_Root)
	{
		return;
	}

	// 선택 상태에서 버튼 스타일을 덮어쓰기 전에 WBP 원본을 한 번만 보관한다.
	// 선택 해제 뒤 Normal·Hovered·Pressed를 모두 원래 상태로 되돌리는 데 필요하다.
	if (!bCapturedDefaultBrushes)
	{
		const FButtonStyle& InitialStyle = Button_Root->GetStyle();
		DefaultNormalBrush = InitialStyle.Normal;
		DefaultHoveredBrush = InitialStyle.Hovered;
		DefaultPressedBrush = InitialStyle.Pressed;
		bCapturedDefaultBrushes = true;
	}

	// 배경은 버튼 스타일이 그린다. WBP의 Pressed 슬롯을 선택 브러시로 사용한다.
	// 선택된 버튼은 Hovered 상태에서도 선택 브러시를 유지해야 한다. Normal만 바꾸면
	// 마우스를 올리는 순간 WBP의 회색 Hovered 브러시로 돌아가 선택이 풀린 것처럼 보인다.
	// 색만 바꾸지 않고 배경 형태가 함께 바뀌어야 한다 (AT-011).
	// **WBP에서 선택 상태 텍스처를 Pressed 슬롯에 넣어야 한다.**
	FButtonStyle Style = Button_Root->GetStyle();
	if (bSelected)
	{
		Style.SetNormal(DefaultPressedBrush);
		Style.SetHovered(DefaultPressedBrush);
		Style.SetPressed(DefaultPressedBrush);
	}
	else
	{
		Style.SetNormal(DefaultNormalBrush);
		Style.SetHovered(DefaultHoveredBrush);
		Style.SetPressed(DefaultPressedBrush);
	}
	Button_Root->SetStyle(Style);
}

void UFF_LiveHudTabButton::HandleClicked()
{
	OnTabButtonClicked.ExecuteIfBound(Tab);
}

// =============================================================================
// UFF_LiveHudMenuBar
// =============================================================================

void UFF_LiveHudMenuBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CollectTabs();

	// 초기 선택은 운영이다. 목업 01-operations 기준.
	SelectTab(EFF_LiveHudTab::Operations);
}

void UFF_LiveHudMenuBar::CollectTabs()
{
	TabButtons.Reset();

	// 필드와 enum 값을 한 곳에서 짝짓는다. 탭을 늘리면 이 표에 한 줄만 추가한다.
	const TPair<EFF_LiveHudTab, UFF_LiveHudTabButton*> Bindings[] = {
		{ EFF_LiveHudTab::Operations, Tab_Operations },
		{ EFF_LiveHudTab::Facilities, Tab_Facilities },
		{ EFF_LiveHudTab::Guests,     Tab_Guests },
		{ EFF_LiveHudTab::Queues,     Tab_Queues },
		{ EFF_LiveHudTab::Density,    Tab_Density },
		{ EFF_LiveHudTab::Events,     Tab_Events },
		{ EFF_LiveHudTab::SNS,        Tab_SNS },
		{ EFF_LiveHudTab::Report,     Tab_Report },
		{ EFF_LiveHudTab::Staff,      Tab_Staff }
	};

	const UEnum* TabEnum = StaticEnum<EFF_LiveHudTab>();
	if (!TabEnum)
	{
		return;
	}

	TArray<FString> MissingTabs;

	for (const TPair<EFF_LiveHudTab, UFF_LiveHudTabButton*>& Binding : Bindings)
	{
		const int32 EnumIndex = TabEnum->GetIndexByValue(static_cast<int64>(Binding.Key));
		const FString EnumName = TabEnum->GetNameStringByIndex(EnumIndex);

		if (!Binding.Value)
		{
			MissingTabs.Add(EnumName);
			continue;
		}

		// 라벨과 아이콘은 코드가 채운다. WBP에 문구를 박으면 enum과 화면이 갈라진다.
		Binding.Value->Configure(
			Binding.Key,
			TabEnum->GetDisplayNameTextByIndex(EnumIndex),
			ResolveTabIcon(Binding.Key, EnumName));

		Binding.Value->OnTabButtonClicked.BindUObject(this, &UFF_LiveHudMenuBar::HandleTabButtonClicked);
		TabButtons.Add(Binding.Value);
	}

	// 빠진 탭을 조용히 넘기지 않는다. WBP에서 이름을 틀렸을 때 이 로그가 유일한 단서다.
	if (!MissingTabs.IsEmpty())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] WBP에 없는 탭 %d개: %s — 위젯 이름이 Tab_<enum 이름>과 같아야 한다."),
			*GetName(), MissingTabs.Num(), *FString::Join(MissingTabs, TEXT(", ")));
	}
}

FSlateBrush UFF_LiveHudMenuBar::ResolveTabIcon(EFF_LiveHudTab Tab, const FString& EnumName) const
{
	// 예외로 지정된 것이 있으면 그것을 쓴다.
	if (const FSlateBrush* Override = TabIconOverrides.Find(Tab))
	{
		return *Override;
	}

	FSlateBrush Brush;

	if (TabIconFolder.IsEmpty() || TabIconNameFormat.IsEmpty())
	{
		return Brush;
	}

	// enum 이름으로 경로를 만든다. 탭을 추가하면 아이콘 파일만 규칙에 맞춰 넣으면 된다.
	const FString AssetName = FString::Format(*TabIconNameFormat, { EnumName });
	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *TabIconFolder, *AssetName, *AssetName);

	// 작은 UI 아이콘 9개라 동기 로드로 둔다. 위젯 생성 시 한 번만 돈다.
	UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!IconTexture)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 탭 아이콘을 찾지 못했다 — %s. TabIconFolder를 확인한다."), *GetName(), *ObjectPath);
		return Brush;
	}

	Brush.SetResourceObject(IconTexture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetImageSize(FVector2D(TabIconSize, TabIconSize));
	return Brush;
}

void UFF_LiveHudMenuBar::SelectTab(EFF_LiveHudTab Tab)
{
	SelectedTab = Tab;

	// 전체를 다시 선언한다. 이전 선택을 지우고 새 것을 켜는 식으로 하면 상태가 두 개 켜질 수 있다.
	for (const TObjectPtr<UFF_LiveHudTabButton>& TabButton : TabButtons)
	{
		if (TabButton)
		{
			TabButton->SetSelected(TabButton->GetTab() == Tab);
		}
	}

	OnTabSelected.Broadcast(Tab);
}

void UFF_LiveHudMenuBar::HandleTabButtonClicked(EFF_LiveHudTab Tab)
{
	// 같은 탭을 다시 눌러도 방송한다. 패널을 닫았다가 다시 여는 동작이 있을 수 있다.
	SelectTab(Tab);
}

#undef LOCTEXT_NAMESPACE
