// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/OperationAlert/FF_OperationAlertPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Core/FF_OutsideClickDismiss.h"
#include "UI/LiveHUD/FF_LiveUtilityDock.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertSubsystem.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FFOperationAlertPanel"

TSharedRef<SWidget> UFF_OperationAlertPanel::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_OperationAlertPanel::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	using namespace FFIncidentUiStyle;

	UiFont = LoadUiFont();
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_OperationAlert"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_OperationAlert"));
	Width->SetWidthOverride(CardWidth);
	Width->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_OperationAlert"));
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Width->SetContent(Column);

	// 오른쪽 위에 붙여 두면 카드가 늘어날 때 아래로 자란다
	if (UCanvasPanelSlot* ColumnSlot = Canvas->AddChildToCanvas(Width))
	{
		ColumnSlot->SetAnchors(FAnchors(1.f, 0.f));
		ColumnSlot->SetAlignment(FVector2D(1.f, 0.f));
		ColumnSlot->SetAutoSize(true);
		ColumnSlot->SetPosition(FVector2D(-ScreenMargin.X, ScreenMargin.Y));
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const FFPauseOptionsLightTeal::FTokens Tokens = Palette ? FFPauseOptionsLightTeal::Get(*Palette) : FFPauseOptionsLightTeal::FTokens();

	// 지난 알림 버튼. 누를 수 있는 높이는 디자인 시스템 기준(44px)을 맞춘다
	HistoryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_OperationAlertHistory"));
	HistoryButton->SetStyle(MakeButtonStyle(Tokens, false));
	HistoryButton->OnClicked.AddUniqueDynamic(this, &UFF_OperationAlertPanel::HandleHistoryClicked);
	HistoryButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HistoryButtonLabel->SetFont(MakeFont(UiFont, TEXT("SemiBold"), 13));
	HistoryButtonLabel->SetColorAndOpacity(FSlateColor(GetButtonLabelColor(Tokens, false, true)));
	USizeBox* ButtonHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonHeight->SetMinDesiredHeight(MinInteractiveSize);
	HistoryButton->SetContent(HistoryButtonLabel);
	ButtonHeight->SetContent(HistoryButton);
	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(ButtonHeight))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	}

	HistoryPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_OperationAlertHistory"));
	HistoryPanel->SetBrush(MakeRounded(Tokens.PaperStrong, PanelRadius, Tokens.Line, 1.f));
	HistoryPanel->SetPadding(FMargin(8.f));
	HistoryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_OperationAlertHistory"));
	HistoryPanel->SetContent(HistoryList);
	HistoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* HistorySlot = Column->AddChildToVerticalBox(HistoryPanel))
	{
		HistorySlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	ToastList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_OperationAlertToasts"));
	ToastList->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* ToastSlot = Column->AddChildToVerticalBox(ToastList))
	{
		ToastSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	RefreshButtonLabel();
}

void UFF_OperationAlertPanel::NativeConstruct()
{
	Super::NativeConstruct();
	Bind();
	ApplyVisibility();
	FFOutsideClickDismiss::Register(this,
		[this] { return bHistoryOpen; },
		[this](FVector2D Position)
		{
			return FFOutsideClickDismiss::IsInside(HistoryPanel, Position)
				|| FFOutsideClickDismiss::IsInside(HistoryButton, Position);
		}, FSimpleDelegate::CreateWeakLambda(this, [this] { if (bHistoryOpen) ToggleHistory(); }));
}

void UFF_OperationAlertPanel::NativeDestruct()
{
	FFOutsideClickDismiss::Unregister(this);
	if (BoundAlerts)
	{
		BoundAlerts->OnOperationAlertRaised.RemoveDynamic(this, &UFF_OperationAlertPanel::HandleAlertRaised);
		BoundAlerts = nullptr;
	}
	BoundViewModel = nullptr;
	Super::NativeDestruct();
}

void UFF_OperationAlertPanel::Bind()
{
	if (!BoundViewModel)
	{
		const UGameInstance* GI = GetGameInstance();
		BoundViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	}
	if (!BoundAlerts)
	{
		const UWorld* World = GetWorld();
		BoundAlerts = World ? World->GetSubsystem<UFF_OperationAlertSubsystem>() : nullptr;
		if (BoundAlerts)
		{
			BoundAlerts->OnOperationAlertRaised.AddUniqueDynamic(this, &UFF_OperationAlertPanel::HandleAlertRaised);
			// 위젯이 늦게 생겨도 이미 낸 알림은 목록에서 볼 수 있어야 한다
			History = BoundAlerts->GetHistory();
			RefreshButtonLabel();
		}
	}
}

void UFF_OperationAlertPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ApplyVisibility();
	if (!Toasts.IsEmpty())
	{
		AdvanceTime(InDeltaTime);
	}
}

void UFF_OperationAlertPanel::HandleAlertRaised(const FFF_OperationAlert& Alert)
{
	AddAlert(Alert);
}

void UFF_OperationAlertPanel::HandleHistoryClicked()
{
	ToggleHistory();
}

UBorder* UFF_OperationAlertPanel::MakeCard(const FFF_OperationAlert& Alert, bool bInHistory, UTextBlock*& OutText)
{
	using namespace FFIncidentUiStyle;
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return nullptr;
	}
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

	// 목록 안의 줄은 패널 바탕 위에 있으므로 테두리 없이 한 단계 낮은 바탕을 쓴다
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrush(bInHistory
		? MakeRounded(Tokens.PaperSunken, ChipRadius, FLinearColor::Transparent, 0.f)
		: MakeRounded(Tokens.PaperStrong, CardRadius, Tokens.Line, 1.f));
	Card->SetPadding(FMargin(8.f, 8.f, 12.f, 8.f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Card->SetContent(Row);

	// 주의 색 띠와 앞머리 글자를 함께 써서 색만으로 뜻을 전하지 않는다
	USizeBox* StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetWidthOverride(4.f);
	UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Strip->SetBrush(MakePill(Palette->InGame.BandCaution));
	StripBox->SetContent(Strip);
	if (UHorizontalBoxSlot* StripSlot = Row->AddChildToHorizontalBox(StripBox))
	{
		StripSlot->SetVerticalAlignment(VAlign_Fill);
		StripSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetFont(MakeFont(UiFont, TEXT("SemiBold"), 13));
	Label->SetColorAndOpacity(FSlateColor(bInHistory ? Tokens.InkMuted : Palette->InGame.BandCaution));
	Label->SetText(bInHistory
		? FText::Format(LOCTEXT("HistoryDay", "{0}일차"), FText::AsNumber(Alert.Day))
		: LOCTEXT("ToastLabel", "운영"));
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UTextBlock* Message = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Message->SetFont(MakeFont(UiFont, TEXT("Regular"), bInHistory ? 13 : 14));
	Message->SetColorAndOpacity(FSlateColor(Tokens.Ink));
	Message->SetAutoWrapText(true);
	Message->SetText(Alert.Message);
	if (UHorizontalBoxSlot* MessageSlot = Row->AddChildToHorizontalBox(Message))
	{
		MessageSlot->SetVerticalAlignment(VAlign_Center);
		MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	OutText = Message;
	return Card;
}

void UFF_OperationAlertPanel::AddAlert(const FFF_OperationAlert& Alert)
{
	BuildTree();
	History.Add(Alert);
	RefreshButtonLabel();
	if (bHistoryOpen)
	{
		RefreshHistoryList();
	}

	if (!WidgetTree || !ToastList)
	{
		return;
	}
	while (Toasts.Num() >= MaxToasts)
	{
		RemoveToastAt(Toasts.Num() - 1);
	}

	UTextBlock* Text = nullptr;
	UBorder* Card = MakeCard(Alert, false, Text);
	if (!Card)
	{
		return;
	}
	ToastList->InsertChildAt(0, Card);
	FToast Toast;
	Toast.Card = Card;
	Toast.Text = Text;
	Toast.RemainingSeconds = FMath::Max(DisplaySeconds, KINDA_SMALL_NUMBER);
	Toasts.Insert(Toast, 0);

	for (int32 Index = 0; Index < Toasts.Num(); ++Index)
	{
		if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Toasts[Index].Card->Slot))
		{
			CardSlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : 6.f, 0.f, 0.f));
		}
	}
	ApplyVisibility();
}

void UFF_OperationAlertPanel::RemoveToastAt(int32 Index)
{
	if (!Toasts.IsValidIndex(Index))
	{
		return;
	}
	if (ToastList && Toasts[Index].Card)
	{
		ToastList->RemoveChild(Toasts[Index].Card);
	}
	Toasts.RemoveAt(Index);
}

FText UFF_OperationAlertPanel::GetToastMessageAt(int32 Index) const
{
	return Toasts.IsValidIndex(Index) && Toasts[Index].Text ? Toasts[Index].Text->GetText() : FText::GetEmpty();
}

void UFF_OperationAlertPanel::AdvanceTime(float DeltaSeconds)
{
	const float Step = FMath::Max(0.f, DeltaSeconds);
	for (int32 Index = Toasts.Num() - 1; Index >= 0; --Index)
	{
		FToast& Toast = Toasts[Index];
		Toast.RemainingSeconds -= Step;
		if (Toast.RemainingSeconds <= 0.f)
		{
			RemoveToastAt(Index);
			continue;
		}
		if (Toast.Card)
		{
			Toast.Card->SetRenderOpacity(FadeSeconds > 0.f ? FMath::Clamp(Toast.RemainingSeconds / FadeSeconds, 0.f, 1.f) : 1.f);
		}
	}
	if (!Toasts.IsEmpty())
	{
		if (UVerticalBoxSlot* TopSlot = Cast<UVerticalBoxSlot>(Toasts[0].Card->Slot))
		{
			TopSlot->SetPadding(FMargin(0.f));
		}
	}
}

void UFF_OperationAlertPanel::ToggleHistory()
{
	if (UsesUnifiedNews())
	{
		CachedNewsDock->ToggleSNS();
		return;
	}
	BuildTree();
	bHistoryOpen = !bHistoryOpen;
	if (bHistoryOpen)
	{
		RefreshHistoryList();
	}
	if (HistoryPanel)
	{
		HistoryPanel->SetVisibility(bHistoryOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshButtonLabel();
}

void UFF_OperationAlertPanel::RefreshHistoryList()
{
	if (!HistoryList || !WidgetTree)
	{
		return;
	}
	HistoryList->ClearChildren();

	if (History.IsEmpty())
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetFont(FFIncidentUiStyle::MakeFont(UiFont, TEXT("Regular"), 13));
		if (const UFF_UIPalette* Palette = UFF_UIPalette::Get())
		{
			Empty->SetColorAndOpacity(FSlateColor(FFPauseOptionsLightTeal::Get(*Palette).InkMuted));
		}
		Empty->SetText(LOCTEXT("HistoryEmpty", "아직 알림이 없어요"));
		HistoryList->AddChildToVerticalBox(Empty);
		return;
	}

	// 최근 것부터 MaxHistoryRows 줄
	const int32 First = History.Num() - 1;
	const int32 Last = FMath::Max(0, History.Num() - MaxHistoryRows);
	for (int32 Index = First; Index >= Last; --Index)
	{
		UTextBlock* Text = nullptr;
		if (UBorder* Row = MakeCard(History[Index], true, Text))
		{
			if (UVerticalBoxSlot* RowSlot = HistoryList->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, Index == First ? 0.f : 4.f, 0.f, 0.f));
			}
		}
	}
}

void UFF_OperationAlertPanel::RefreshButtonLabel()
{
	if (!HistoryButtonLabel)
	{
		return;
	}
	HistoryButtonLabel->SetText(bHistoryOpen
		? LOCTEXT("HistoryClose", "지난 알림 닫기")
		: FText::Format(LOCTEXT("HistoryOpen", "지난 알림 {0}"), FText::AsNumber(History.Num())));
}

bool UFF_OperationAlertPanel::ShouldShow() const
{
	return bForceVisible || (BoundViewModel && BoundViewModel->GetCurrentStage() == EFF_FestivalStage::Live);
}

bool UFF_OperationAlertPanel::UsesUnifiedNews()
{
	if (!CachedNewsDock.IsValid()) { CachedNewsDock = UFF_LiveUtilityDock::FindForPlayer(GetOwningPlayer()); }
	return CachedNewsDock.IsValid() && CachedNewsDock->HasUnifiedNews();
}

void UFF_OperationAlertPanel::ApplyVisibility()
{
	// 운영 기록은 왼쪽 소식으로 통합한다. 기존 도크가 없는 화면에서는 예전 진입점을 보존한다.
	const bool bUnified = UsesUnifiedNews();
	if (HistoryButton && HistoryButton->GetParent())
	{
		HistoryButton->GetParent()->SetVisibility(bUnified ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (bUnified)
	{
		bHistoryOpen = false;
		if (HistoryPanel) { HistoryPanel->SetVisibility(ESlateVisibility::Collapsed); }
	}
	const bool bShow = ShouldShow();
	// Live 를 벗어나면 남은 카드와 펼친 목록을 치운다. 결산 화면 위에 운영 알림이 얹혀 있으면 안 된다
	if (bWasShown && !bShow)
	{
		while (!Toasts.IsEmpty())
		{
			RemoveToastAt(Toasts.Num() - 1);
		}
		if (bHistoryOpen)
		{
			ToggleHistory();
		}
	}
	bWasShown = bShow;
	const ESlateVisibility Desired = bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (GetVisibility() != Desired)
	{
		SetVisibility(Desired);
	}
}

#undef LOCTEXT_NAMESPACE
