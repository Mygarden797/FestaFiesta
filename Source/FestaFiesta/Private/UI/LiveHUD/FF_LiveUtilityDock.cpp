#include "UI/LiveHUD/FF_LiveUtilityDock.h"

#include "Audio/FF_AudioSettings.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Audio/FF_PlaylistPlayerWidget.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/Core/FF_OutsideClickDismiss.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UObject/UObjectIterator.h"

bool UFF_LiveUtilityDock::IsConfigured()
{
	const UFF_AudioSettings* Settings = UFF_AudioSettings::Get();
	const UClass* WidgetClass = Settings ? Settings->PlaylistPlayerWidgetClass.LoadSynchronous() : nullptr;
	return WidgetClass && WidgetClass->IsChildOf(StaticClass());
}

UFF_LiveUtilityDock* UFF_LiveUtilityDock::FindForPlayer(const APlayerController* Player)
{
	for (TObjectIterator<UFF_LiveUtilityDock> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetOwningPlayer() == Player && It->IsInViewport()) return *It;
	}
	return nullptr;
}

bool UFF_LiveUtilityDock::HasUnifiedNews() const
{
	return SnsPanel && SnsPanel->bUnifiedNews;
}

bool UFF_LiveUtilityDock::IsForegroundFlyoutOpen() const
{
	return IsInViewport() && IsVisible() && bDockPresented && OpenPanel != 0 &&
		Panel_Dock && Panel_Dock->IsVisible() && Panel_Flyout && Panel_Flyout->IsVisible();
}

void UFF_LiveUtilityDock::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyLayout(FVector2D::ZeroVector);
	for (UTextBlock* Label : { Text_SNSLabel.Get(), Text_MusicLabel.Get() })
	{
		if (Label) { Label->SetVisibility(bShowButtonLabels ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	}
}

void UFF_LiveUtilityDock::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Button_SNS->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleSNS);
	Button_Music->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleMusic);
	Panel_Dock->SetVisibility(ESlateVisibility::Collapsed);
	SizeBox_SNS->SetVisibility(ESlateVisibility::Collapsed);
	bDockAvailable = false;
	bDockPresented = false;
	bSnsAvailable = false;
	if (!SnsPanel)
	{
		if (UClass* PanelClass = SnsPanelClass.LoadSynchronous()) SnsPanel = CreateWidget<UFF_LivePanelSNS>(GetOwningPlayer(), PanelClass);
		if (SnsPanel)
		{
			SnsPanel->InitializePanel(nullptr, EFF_LiveTab::SNS, SnsPanel->bUnifiedNews ? NSLOCTEXT("FFUtilityDock", "News", "소식") : NSLOCTEXT("FFUtilityDock", "SNS", "SNS"));
			SnsPanel->OnCloseRequested.BindUObject(this, &ThisClass::CloseFlyout);
			PendingPostsHandle = SnsPanel->OnPendingPostsChanged.AddUObject(this, &ThisClass::RefreshUnread);
			UOverlaySlot* ContentSlot = Panel_Flyout->AddChildToOverlay(SnsPanel);
			ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
			SnsPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (HasUnifiedNews())
	{
		Button_SNS->SetToolTipText(NSLOCTEXT("FFUtilityDock", "NewsTooltip", "소식 · 관객 이야기와 운영 알림"));
		if (Text_SNSLabel) { Text_SNSLabel->SetText(NSLOCTEXT("FFUtilityDock", "News", "소식")); }
	}
	if (!MusicPanel)
	{
		if (UClass* PanelClass = MusicPanelClass.LoadSynchronous()) MusicPanel = CreateWidget<UFF_PlaylistPlayerWidget>(GetOwningPlayer(), PanelClass);
		if (MusicPanel)
		{
			MusicPanel->OnCloseRequested.AddUniqueDynamic(this, &ThisClass::CloseFlyout);
			UOverlaySlot* ContentSlot = Panel_Flyout->AddChildToOverlay(MusicPanel);
			ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
			MusicPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	RefreshUnread(SnsPanel ? SnsPanel->GetPendingPostCount() : 0);
	RefreshAvailability();
	RefreshSelection();
	FFOutsideClickDismiss::Register(this,
		[this] { return bDockPresented && OpenPanel != 0; },
		[this](FVector2D Position)
		{
			return FFOutsideClickDismiss::IsInside(Panel_Flyout, Position)
				|| FFOutsideClickDismiss::IsInside(SizeBox_SNS, Position)
				|| FFOutsideClickDismiss::IsInside(SizeBox_Music, Position);
		}, FSimpleDelegate::CreateUObject(this, &ThisClass::CloseFlyout));
}

void UFF_LiveUtilityDock::NativeDestruct()
{
	FFOutsideClickDismiss::Unregister(this);
	CloseFlyout();
	Button_SNS->OnClicked.RemoveDynamic(this, &ThisClass::ToggleSNS);
	Button_Music->OnClicked.RemoveDynamic(this, &ThisClass::ToggleMusic);
	if (SnsPanel)
	{
		SnsPanel->OnCloseRequested.Unbind();
		SnsPanel->OnPendingPostsChanged.Remove(PendingPostsHandle);
		SnsPanel->RemoveFromParent(); SnsPanel = nullptr;
	}
	if (MusicPanel)
	{
		MusicPanel->OnCloseRequested.RemoveDynamic(this, &ThisClass::CloseFlyout);
		MusicPanel->RemoveFromParent(); MusicPanel = nullptr;
	}
	Super::NativeDestruct();
}

void UFF_LiveUtilityDock::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAvailability();
	const FVector2D Available = MyGeometry.GetLocalSize();
	if (!Available.Equals(LastAvailableSize)) ApplyLayout(Available);
}

void UFF_LiveUtilityDock::RefreshAvailability()
{
	UGameInstance* GI = GetGameInstance();
	const UFF_UIFlowSubsystem* Flow = GI ? GI->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const bool bAvailable = (!Flow || Flow->GetMode() != EFF_UIFlowMode::Frontend) && (!Tutorial || !Tutorial->IsDialogueActive());
	const UFF_FestivalViewModelSubsystem* VM = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	const bool bShowSNS = bAvailable && SnsPanel && VM && FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::SNS, VM->GetCurrentStage(), false);
	if ((!bAvailable && OpenPanel != 0) || (!bShowSNS && OpenPanel == 1)) CloseFlyout();
	bDockAvailable = bAvailable;
	const UFF_UIRootLayout* Root = Flow ? Flow->GetRootLayout() : nullptr;
	const bool bPresentDock = bAvailable && (!Root || !Root->HasForegroundStackContent());
	if (bPresentDock != bDockPresented)
	{
		bDockPresented = bPresentDock;
		// 도크는 일반 창/월드 라벨보다 앞 레이어지만 확인창·ESC에는 표시/입력을 양보한다.
		// 위젯을 내렸다 다시 만들지 않으므로 재생 상태와 SNS 읽던 위치는 유지된다.
		Panel_Dock->SetVisibility(bDockPresented ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowSNS != bSnsAvailable)
	{
		bSnsAvailable = bShowSNS;
		SizeBox_SNS->SetVisibility(bSnsAvailable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ApplyLayout(LastAvailableSize);
	}
	SizeBox_Music->SetVisibility(MusicPanel ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UFF_LiveUtilityDock::ToggleSNS()
{
	RefreshAvailability();
	if (!bDockPresented || !bSnsAvailable || !SnsPanel) return;
	const bool bWasOpen = OpenPanel == 1;
	CloseFlyout();
	if (!bWasOpen)
	{
		OpenPanel = 1;
		SnsPanel->SetVisibility(ESlateVisibility::Visible);
		SnsPanel->HandlePanelOpened();
		RefreshSelection();
	}
}

void UFF_LiveUtilityDock::ToggleMusic()
{
	RefreshAvailability();
	if (!bDockPresented || !bDockAvailable || !MusicPanel) return;
	const bool bWasOpen = OpenPanel == 2;
	CloseFlyout();
	if (!bWasOpen)
	{
		OpenPanel = 2;
		MusicPanel->SetVisibility(ESlateVisibility::Visible);
		RefreshSelection();
	}
}

void UFF_LiveUtilityDock::CloseFlyout()
{
	if (OpenPanel == 1 && SnsPanel) SnsPanel->HandlePanelClosed();
	if (SnsPanel) SnsPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (MusicPanel) MusicPanel->SetVisibility(ESlateVisibility::Collapsed);
	OpenPanel = 0;
	RefreshSelection();
}

void UFF_LiveUtilityDock::RefreshSelection()
{
	Panel_Flyout->SetVisibility(OpenPanel != 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	// 열린 쪽 버튼만 색을 바꾼다. 옆에 펼쳐진 패널이 선택 상태를 함께 보여 준다.
	const auto Paint = [this](UImage* Icon, UTextBlock* Label, const bool bSelected)
	{
		const FLinearColor Tint = bSelected ? SelectedIconColor : IconColor;
		if (Icon) { Icon->SetColorAndOpacity(Tint); }
		if (Label) { Label->SetColorAndOpacity(FSlateColor(Tint)); }
	};
	Paint(Image_SNS, Text_SNSLabel, OpenPanel == 1);
	Paint(Image_Music, Text_MusicLabel, OpenPanel == 2);
	for (UTextBlock* OldCheck : { Text_SNSSelected.Get(), Text_MusicSelected.Get() })
	{
		if (OldCheck) { OldCheck->SetVisibility(ESlateVisibility::Collapsed); }
	}
	ApplyLayout(LastAvailableSize);
}

void UFF_LiveUtilityDock::RefreshUnread(int32 Count)
{
	const bool bShow = Count > 0;
	Text_Unread->SetText(Count > 99 ? FText::FromString(TEXT("99+")) : FText::AsNumber(Count));
	Text_Unread->SetVisibility(bShow && !bUnreadAsDot ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Badge = Text_Unread->GetParent())
	{
		Badge->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		// 점 모드: 글자를 접고 여백만으로 정사각 칸을 만든다. 배지 브러시가 높이 절반 둥글기라 원이 된다.
		if (UBorder* BadgeBorder = Cast<UBorder>(Badge); BadgeBorder && bUnreadAsDot)
		{
			BadgeBorder->SetPadding(FMargin(FMath::Max(2.f, UnreadDotSize * .5f)));
		}
	}
}

void UFF_LiveUtilityDock::ApplyLayout(FVector2D AvailableSize)
{
	LastAvailableSize = AvailableSize;
	const float Diameter = FMath::Max(32.f, ButtonDiameter);
	FVector2D Position = DockPosition;
	if (AvailableSize.X > 0.f && AvailableSize.Y > 0.f)
	{
		Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, AvailableSize.X - Diameter));
		Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, AvailableSize.Y - Diameter * 2.f - ButtonGap - ReservedBottomSpace));
	}
	const auto PlaceButton = [Diameter](USizeBox* Widget, FVector2D At)
	{
		if (!Widget) return;
		Widget->SetWidthOverride(Diameter); Widget->SetHeightOverride(Diameter);
		if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Widget->Slot)) { ButtonSlot->SetPosition(At); ButtonSlot->SetSize(FVector2D(Diameter)); }
	};
	PlaceButton(SizeBox_SNS, Position);
	PlaceButton(SizeBox_Music, Position + FVector2D(0.f, (bSnsAvailable || IsDesignTime()) ? Diameter + ButtonGap : 0.f));
	FVector2D PanelSize = OpenPanel == 2 ? MusicPanelSize : SnsPanelSize;
	PanelSize.X = FMath::Max(240.f, PanelSize.X); PanelSize.Y = FMath::Max(240.f, PanelSize.Y);
	FVector2D PanelPosition = Position + PanelOffset;
	if (AvailableSize.X > 0.f && AvailableSize.Y > 0.f)
	{
		PanelSize.X = FMath::Min(PanelSize.X, FMath::Max(1.f, AvailableSize.X - 16.f));
		PanelSize.Y = FMath::Min(PanelSize.Y, FMath::Max(1.f, AvailableSize.Y - ReservedBottomSpace - 16.f));
		PanelPosition.X = FMath::Clamp(PanelPosition.X, 0.f, FMath::Max(0.f, AvailableSize.X - PanelSize.X));
		PanelPosition.Y = FMath::Clamp(PanelPosition.Y, 0.f, FMath::Max(0.f, AvailableSize.Y - ReservedBottomSpace - PanelSize.Y));
	}
	if (Panel_Flyout)
	{
		if (UCanvasPanelSlot* FlyoutSlot = Cast<UCanvasPanelSlot>(Panel_Flyout->Slot)) { FlyoutSlot->SetPosition(PanelPosition); FlyoutSlot->SetSize(PanelSize); }
	}
}
