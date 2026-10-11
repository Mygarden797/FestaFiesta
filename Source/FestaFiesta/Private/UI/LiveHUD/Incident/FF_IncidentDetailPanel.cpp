// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentDetailPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Core/FF_OutsideClickDismiss.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentDetail"

namespace FFIncidentDetailLocal
{
	FFPauseOptionsLightTeal::FTokens GetTokens()
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		return FFPauseOptionsLightTeal::Get(Palette ? *Palette : *GetDefault<UFF_UIPalette>());
	}

	FLinearColor GetAmber()
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		return (Palette ? Palette : GetDefault<UFF_UIPalette>())->InGame.Amber;
	}
}

FText UFF_IncidentDetailPanel::BuildResponseStatusText(EFF_IncidentResponseStage Stage, float RemainingWorkMinutes)
{
	switch (Stage)
	{
	case EFF_IncidentResponseStage::Travelling:
		return FText::Format(LOCTEXT("StatusTravelling", "스태프 출동 중 · {0}"),
			UFF_UIDisplayTextLibrary::GetIncidentResponseStageText(Stage));
	case EFF_IncidentResponseStage::Working:
		return FText::Format(LOCTEXT("StatusWorking", "스태프 출동 중 · {0} · 남은 시간 약 {1}분"),
			UFF_UIDisplayTextLibrary::GetIncidentResponseStageText(Stage),
			FText::AsNumber(FMath::Max(1, FMath::CeilToInt(RemainingWorkMinutes))));
	case EFF_IncidentResponseStage::None:
	default:
		return FText::GetEmpty();
	}
}

FText UFF_IncidentDetailPanel::BuildSendBlockedReason(EFF_IncidentResponseStage Stage, bool bHasOption, int32 Available, int32 MinStaff)
{
	if (Stage != EFF_IncidentResponseStage::None || !bHasOption || Available >= MinStaff)
	{
		return FText::GetEmpty();
	}
	return UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult::NotEnoughStaff);
}

TSharedRef<SWidget> UFF_IncidentDetailPanel::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_IncidentDetailPanel::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	using namespace FFIncidentUiStyle;
	const FFPauseOptionsLightTeal::FTokens Tokens = FFIncidentDetailLocal::GetTokens();
	UiFont = LoadUiFont();

	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Detail"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// 패널: PaperStrong 바탕 · Line 테두리 · 패널 모서리 8px
	Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Card"));
	Card->SetBrush(MakeRounded(Tokens.PaperStrong, PanelRadius, Tokens.Line, 1.f));
	Card->SetPadding(FMargin(16.f, 14.f));
	Card->SetVisibility(ESlateVisibility::Visible);

	if (UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(Card))
	{
		CardSlot->SetAnchors(FAnchors(1.f, 0.5f));
		CardSlot->SetAlignment(FVector2D(1.f, 0.5f));
		CardSlot->SetAutoSize(true);
		CardSlot->SetPosition(CardOffset);
	}

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Card"));
	WidthBox->SetWidthOverride(CardWidth);
	Card->SetContent(WidthBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_Card"));
	WidthBox->SetContent(Column);

	Text_Title = MakeText(*Column, TEXT("Text_Title"), TEXT("SemiBold"), PanelTitleFontSize, Tokens.Ink);
	Text_Body = MakeText(*Column, TEXT("Text_Body"), TEXT("Regular"), 12, Tokens.InkMuted);

	// 출동 상태 칩: TealSoft 바탕 · 칩 모서리 4px 위에 TealDeep 글자. 출동 중일 때만 보인다.
	Border_Status = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Status"));
	Border_Status->SetBrush(MakeRounded(Tokens.TealSoft, ChipRadius, Tokens.TealSoft, 0.f));
	Border_Status->SetPadding(FMargin(12.f, 4.f));
	Border_Status->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(Border_Status))
	{
		StatusSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		StatusSlot->SetHorizontalAlignment(HAlign_Left);
	}
	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Text_Status->SetFont(MakeFont(UiFont, TEXT("SemiBold"), 12));
	Text_Status->SetColorAndOpacity(FSlateColor(Tokens.TealDeep));
	Text_Status->SetAutoWrapText(true);
	Border_Status->SetContent(Text_Status);

	Box_Option = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_Option"));
	if (UVerticalBoxSlot* OptionSlot = Column->AddChildToVerticalBox(Box_Option))
	{
		OptionSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	Text_OptionLabel = MakeText(*Box_Option, TEXT("Text_OptionLabel"), TEXT("SemiBold"), 13, Tokens.Ink);
	Text_StaffRange = MakeText(*Box_Option, TEXT("Text_StaffRange"), TEXT("Regular"), 12, Tokens.InkMuted);
	Text_WorkAmount = MakeText(*Box_Option, TEXT("Text_WorkAmount"), TEXT("Regular"), 12, Tokens.InkMuted);
	Text_Reward = MakeText(*Box_Option, TEXT("Text_Reward"), TEXT("SemiBold"), 12, FFIncidentDetailLocal::GetAmber());

	Row_Staff = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_Staff"));
	if (UVerticalBoxSlot* StaffSlot = Box_Option->AddChildToVerticalBox(Row_Staff))
	{
		StaffSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	UTextBlock* StaffCaption = MakeText(*Row_Staff, TEXT("Text_StaffCaption"), TEXT("Regular"), 12, Tokens.Ink);
	StaffCaption->SetText(LOCTEXT("StaffCaption", "출동 인원"));

	UTextBlock* MinusLabel = nullptr;
	Button_Minus = MakeButton(*Row_Staff, TEXT("Button_Minus"), LOCTEXT("Minus", "-"), false, MinusLabel);
	Label_Minus = MinusLabel;
	Text_StaffCount = MakeText(*Row_Staff, TEXT("Text_StaffCount"), TEXT("SemiBold"), 14, Tokens.Ink);
	Text_StaffCount->SetJustification(ETextJustify::Center);
	UTextBlock* PlusLabel = nullptr;
	Button_Plus = MakeButton(*Row_Staff, TEXT("Button_Plus"), LOCTEXT("Plus", "+"), false, PlusLabel);
	Label_Plus = PlusLabel;

	for (UWidget* Child : Row_Staff->GetAllChildren())
	{
		if (UHorizontalBoxSlot* ChildSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			ChildSlot->SetVerticalAlignment(VAlign_Center);
			ChildSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	}

	UHorizontalBox* Row_Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_Actions"));
	if (UVerticalBoxSlot* ActionSlot = Column->AddChildToVerticalBox(Row_Actions))
	{
		ActionSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		ActionSlot->SetHorizontalAlignment(HAlign_Right);
	}

	UTextBlock* SendLabel = nullptr;
	Button_Send = MakeButton(*Row_Actions, TEXT("Button_Send"), LOCTEXT("Send", "보내기"), true, SendLabel);
	Text_SendLabel = SendLabel;
	UTextBlock* CloseLabel = nullptr;
	Button_Close = MakeButton(*Row_Actions, TEXT("Button_Close"), LOCTEXT("Close", "닫기"), false, CloseLabel);
	if (UHorizontalBoxSlot* SendSlot = Button_Send->GetParent() ? Cast<UHorizontalBoxSlot>(Button_Send->GetParent()->Slot) : nullptr)
	{
		SendSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	// [보내기]가 꺼진 이유. 상태 줄로 드러나지 않는 경우에만 보인다.
	Text_Reason = MakeText(*Column, TEXT("Text_Reason"), TEXT("Regular"), 12, Tokens.Danger);
	Text_Reason->SetJustification(ETextJustify::Right);
	Text_Reason->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ReasonSlot = Cast<UVerticalBoxSlot>(Text_Reason->Slot))
	{
		ReasonSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	Button_Minus->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleMinusClicked);
	Button_Plus->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandlePlusClicked);
	Button_Send->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleSendClicked);
	Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleCloseClicked);
}

UTextBlock* UFF_IncidentDetailPanel::MakeText(UPanelWidget& Parent, FName Name, const TCHAR* Typeface, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetFont(FFIncidentUiStyle::MakeFont(UiFont, Typeface, FontSize));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetAutoWrapText(true);
	Parent.AddChild(Text);
	return Text;
}

UButton* UFF_IncidentDetailPanel::MakeButton(UPanelWidget& Parent, FName Name, const FText& Label, bool bPrimary, UTextBlock*& OutLabel)
{
	const FFPauseOptionsLightTeal::FTokens Tokens = FFIncidentDetailLocal::GetTokens();
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetStyle(FFIncidentUiStyle::MakeButtonStyle(Tokens, bPrimary));

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Label")));
	Text->SetFont(FFIncidentUiStyle::MakeFont(UiFont, TEXT("SemiBold"), 12));
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(FFIncidentUiStyle::GetButtonLabelColor(Tokens, bPrimary, true)));
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 누를 수 있는 크기 44px 이상 (DesignSystem_v2 README 적용 규칙 6)
	USizeBox* HitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(Name.ToString() + TEXT("_Size")));
	HitBox->SetMinDesiredHeight(FFIncidentUiStyle::MinInteractiveSize);
	HitBox->SetMinDesiredWidth(FFIncidentUiStyle::MinInteractiveSize);
	HitBox->SetContent(Button);
	Parent.AddChild(HitBox);
	OutLabel = Text;
	return Button;
}

void UFF_IncidentDetailPanel::SetButtonEnabled(UButton* Button, UTextBlock* Label, bool bPrimary, bool bEnabled)
{
	if (Button)
	{
		Button->SetIsEnabled(bEnabled);
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(
			FFIncidentUiStyle::GetButtonLabelColor(FFIncidentDetailLocal::GetTokens(), bPrimary, bEnabled)));
	}
}

void UFF_IncidentDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();
	FFOutsideClickDismiss::Register(this,
		[this] { return IsOpen(); },
		[this](FVector2D Position) { return FFOutsideClickDismiss::IsInside(Card, Position); },
		FSimpleDelegate::CreateUObject(this, &ThisClass::Close));

	BindIncidents();
	if (!IsOpen())
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_IncidentDetailPanel::NativeDestruct()
{
	FFOutsideClickDismiss::Unregister(this);
	UnbindIncidents();
	CurrentHandle = FFF_IncidentHandle();
	Super::NativeDestruct();
}

UFF_IncidentSubsystem* UFF_IncidentDetailPanel::GetIncidentSubsystem() const
{
	if (CachedIncident)
	{
		return CachedIncident;
	}

	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_IncidentSubsystem>() : nullptr;
}

void UFF_IncidentDetailPanel::BindIncidents()
{
	UFF_IncidentSubsystem* Incident = GetIncidentSubsystem();
	if (!Incident)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 사건 서브시스템을 찾지 못했다. 상세 패널이 갱신되지 않는다."), *GetName());
		return;
	}

	CachedIncident = Incident;
	Incident->OnIncidentUpdated.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentUpdated);
	Incident->OnIncidentClosed.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentClosed);

	if (const UWorld* World = GetWorld())
	{
		CachedStaffing = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
		if (CachedStaffing)
		{
			CachedStaffing->OnStaffingChanged.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleStaffingChanged);
		}
	}
}

void UFF_IncidentDetailPanel::UnbindIncidents()
{
	if (CachedIncident)
	{
		CachedIncident->OnIncidentUpdated.RemoveDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentUpdated);
		CachedIncident->OnIncidentClosed.RemoveDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentClosed);
	}
	CachedIncident = nullptr;

	if (CachedStaffing)
	{
		CachedStaffing->OnStaffingChanged.RemoveDynamic(this, &UFF_IncidentDetailPanel::HandleStaffingChanged);
	}
	CachedStaffing = nullptr;
}

int32 UFF_IncidentDetailPanel::GetDispatchableStaff() const
{
	return CachedStaffing ? CachedStaffing->GetDispatchableStaffCount() : 0;
}

void UFF_IncidentDetailPanel::HandleStaffingChanged()
{
	if (IsOpen() && CurrentOption.bValid)
	{
		RefreshStaff();
	}
}

void UFF_IncidentDetailPanel::OpenFor(FFF_IncidentHandle Handle)
{
	BuildTree();

	const FFF_IncidentHandle Previous = CurrentHandle;
	CurrentHandle = Handle;
	if (Previous != Handle)
	{
		SelectedStaff = 0;
	}

	if (!Refresh())
	{
		Close();
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UFF_IncidentDetailPanel::Close()
{
	CurrentHandle = FFF_IncidentHandle();
	CurrentOption = FFF_IncidentOptionView();
	SelectedStaff = 0;
	ResponseStage = EFF_IncidentResponseStage::None;
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UFF_IncidentDetailPanel::Refresh()
{
	const UFF_IncidentSubsystem* Incident = GetIncidentSubsystem();
	FFF_IncidentView View;
	if (!Incident || !CurrentHandle.IsSet() || !Incident->GetIncidentView(CurrentHandle, View))
	{
		return false;
	}

	Text_Title->SetText(View.Title);
	Text_Body->SetText(View.Body);
	Text_Body->SetVisibility(View.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	ResponseStage = View.ResponseStage;
	const FText Status = BuildResponseStatusText(View.ResponseStage, View.RemainingWorkMinutes);
	Text_Status->SetText(Status);
	Border_Status->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	CurrentOption = FFF_IncidentOptionView();
	for (const FFF_IncidentOptionView& Option : View.Options)
	{
		if (Option.bValid)
		{
			CurrentOption = Option;
			break;
		}
	}

	if (!CurrentOption.bValid)
	{
		Box_Option->SetVisibility(ESlateVisibility::Collapsed);
		Text_SendLabel->SetText(LOCTEXT("Acknowledge", "확인"));
		SetButtonEnabled(Button_Send, Text_SendLabel, true, !IsResponding());
		Text_Reason->SetVisibility(ESlateVisibility::Collapsed);
		SelectedStaff = 0;
		return true;
	}

	Box_Option->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Text_SendLabel->SetText(LOCTEXT("Send", "보내기"));

	Text_OptionLabel->SetText(CurrentOption.Label);
	Text_OptionLabel->SetVisibility(CurrentOption.Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	Text_Reward->SetText(FText::Format(LOCTEXT("Reward", "보상: {0}"), CurrentOption.RewardText));
	Text_Reward->SetVisibility(CurrentOption.RewardText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	RefreshStaff();
	return true;
}

void UFF_IncidentDetailPanel::RefreshStaff()
{
	const int32 Available = GetDispatchableStaff();
	const int32 MaxSelectable = FMath::Max(CurrentOption.MinStaff, Available);
	SelectedStaff = SelectedStaff <= 0
		? CurrentOption.MinStaff
		: FMath::Clamp(SelectedStaff, CurrentOption.MinStaff, MaxSelectable);

	Text_StaffRange->SetText(FText::Format(LOCTEXT("StaffRange", "최소 {0}명 · 출동 가능 {1}명"),
		FText::AsNumber(CurrentOption.MinStaff), FText::AsNumber(Available)));
	Text_StaffRange->SetVisibility(IsResponding() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	FNumberFormattingOptions WorkFormat;
	WorkFormat.MinimumFractionalDigits = 0;
	WorkFormat.MaximumFractionalDigits = 1;
	Text_WorkAmount->SetText(CurrentOption.WorkAmount > 0.f && SelectedStaff > 0
		? FText::Format(LOCTEXT("WorkAmountEstimate", "작업량: {0} 사람-분 · 예상 {1}분"),
			FText::AsNumber(CurrentOption.WorkAmount, &WorkFormat),
			FText::AsNumber(CurrentOption.WorkAmount / SelectedStaff, &WorkFormat))
		: FText::Format(LOCTEXT("WorkAmount", "작업량: {0} 사람-분"),
			FText::AsNumber(CurrentOption.WorkAmount, &WorkFormat)));

	Text_StaffCount->SetText(FText::AsNumber(SelectedStaff));
	Row_Staff->SetVisibility(IsResponding() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	SetButtonEnabled(Button_Minus, Label_Minus, false, !IsResponding() && SelectedStaff > CurrentOption.MinStaff);
	SetButtonEnabled(Button_Plus, Label_Plus, false, !IsResponding() && SelectedStaff < Available);
	SetButtonEnabled(Button_Send, Text_SendLabel, true,
		!IsResponding() && SelectedStaff >= CurrentOption.MinStaff && SelectedStaff <= Available);

	const FText Reason = BuildSendBlockedReason(ResponseStage, true, Available, CurrentOption.MinStaff);
	Text_Reason->SetText(Reason);
	Text_Reason->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UFF_IncidentDetailPanel::HandleIncidentUpdated(FFF_IncidentHandle Handle)
{
	if (Handle == CurrentHandle && !Refresh())
	{
		Close();
	}
}

void UFF_IncidentDetailPanel::HandleIncidentClosed(FFF_IncidentHandle Handle)
{
	if (Handle == CurrentHandle)
	{
		Close();
	}
}

void UFF_IncidentDetailPanel::HandleMinusClicked()
{
	if (!CurrentOption.bValid || IsResponding())
	{
		return;
	}
	SelectedStaff = FMath::Max(CurrentOption.MinStaff, SelectedStaff - 1);
	RefreshStaff();
}

void UFF_IncidentDetailPanel::HandlePlusClicked()
{
	if (!CurrentOption.bValid || IsResponding())
	{
		return;
	}
	SelectedStaff = FMath::Min(SelectedStaff + 1, FMath::Max(CurrentOption.MinStaff, GetDispatchableStaff()));
	RefreshStaff();
}

void UFF_IncidentDetailPanel::HandleSendClicked()
{
	if (!IsOpen() || IsResponding())
	{
		return;
	}

	const FFF_IncidentHandle Handle = CurrentHandle;
	const int32 OptionIndex = CurrentOption.bValid ? CurrentOption.OptionIndex : INDEX_NONE;
	const int32 StaffCount = CurrentOption.bValid ? SelectedStaff : 0;

	Close();
	OnDispatchRequested.Broadcast(Handle, OptionIndex, StaffCount);
}

void UFF_IncidentDetailPanel::HandleCloseClicked()
{
	Close();
}

#undef LOCTEXT_NAMESPACE
