// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentDetailPanel.h"

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
#include "Engine/World.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentDetail"

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

	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Detail"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Card"));
	Card->SetBrushColor(CardColor);
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

	Text_Title = MakeText(*Column, TEXT("Text_Title"), 16);
	Text_Body = MakeText(*Column, TEXT("Text_Body"), 11);
	Text_Body->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.82f, 0.86f, 1.f)));

	Box_Option = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_Option"));
	if (UVerticalBoxSlot* OptionSlot = Column->AddChildToVerticalBox(Box_Option))
	{
		OptionSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	Text_OptionLabel = MakeText(*Box_Option, TEXT("Text_OptionLabel"), 13);
	Text_StaffRange = MakeText(*Box_Option, TEXT("Text_StaffRange"), 11);
	Text_WorkAmount = MakeText(*Box_Option, TEXT("Text_WorkAmount"), 11);
	Text_Cost = MakeText(*Box_Option, TEXT("Text_Cost"), 11);
	Text_Reward = MakeText(*Box_Option, TEXT("Text_Reward"), 11);
	Text_Reward->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.3f, 1.f)));

	Row_Staff = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_Staff"));
	if (UVerticalBoxSlot* StaffSlot = Box_Option->AddChildToVerticalBox(Row_Staff))
	{
		StaffSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	UTextBlock* StaffCaption = MakeText(*Row_Staff, TEXT("Text_StaffCaption"), 12);
	StaffCaption->SetText(LOCTEXT("StaffCaption", "작업 인원"));

	UTextBlock* Unused = nullptr;
	Button_Minus = MakeButton(*Row_Staff, TEXT("Button_Minus"), LOCTEXT("Minus", "-"), Unused);
	Text_StaffCount = MakeText(*Row_Staff, TEXT("Text_StaffCount"), 14);
	Text_StaffCount->SetJustification(ETextJustify::Center);
	Button_Plus = MakeButton(*Row_Staff, TEXT("Button_Plus"), LOCTEXT("Plus", "+"), Unused);

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
	Button_Send = MakeButton(*Row_Actions, TEXT("Button_Send"), LOCTEXT("Send", "보내기"), SendLabel);
	Text_SendLabel = SendLabel;
	Button_Close = MakeButton(*Row_Actions, TEXT("Button_Close"), LOCTEXT("Close", "닫기"), Unused);
	if (UHorizontalBoxSlot* SendSlot = Cast<UHorizontalBoxSlot>(Button_Send->Slot))
	{
		SendSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	Button_Minus->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleMinusClicked);
	Button_Plus->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandlePlusClicked);
	Button_Send->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleSendClicked);
	Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_IncidentDetailPanel::HandleCloseClicked);
}

UTextBlock* UFF_IncidentDetailPanel::MakeText(UPanelWidget& Parent, FName Name, int32 FontSize)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetAutoWrapText(true);
	Parent.AddChild(Text);
	return Text;
}

UButton* UFF_IncidentDetailPanel::MakeButton(UPanelWidget& Parent, FName Name, const FText& Label, UTextBlock*& OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Name.ToString() + TEXT("_Label")));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 12;
	Text->SetFont(Font);
	Text->SetText(Label);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.07f, 1.f)));
	Button->AddChild(Text);
	Parent.AddChild(Button);
	OutLabel = Text;
	return Button;
}

void UFF_IncidentDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();

	BindIncidents();
	if (!IsOpen())
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_IncidentDetailPanel::NativeDestruct()
{
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
}

void UFF_IncidentDetailPanel::UnbindIncidents()
{
	if (CachedIncident)
	{
		CachedIncident->OnIncidentUpdated.RemoveDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentUpdated);
		CachedIncident->OnIncidentClosed.RemoveDynamic(this, &UFF_IncidentDetailPanel::HandleIncidentClosed);
	}
	CachedIncident = nullptr;
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
		Button_Send->SetIsEnabled(true);
		SelectedStaff = 0;
		return true;
	}

	Box_Option->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Text_SendLabel->SetText(LOCTEXT("Send", "보내기"));

	Text_OptionLabel->SetText(CurrentOption.Label);
	Text_OptionLabel->SetVisibility(CurrentOption.Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	Text_StaffRange->SetText(CurrentOption.MinStaff == CurrentOption.MaxStaff
		? FText::Format(LOCTEXT("StaffExact", "필요 인원: {0}명"), FText::AsNumber(CurrentOption.MinStaff))
		: FText::Format(LOCTEXT("StaffRange", "필요 인원: {0}~{1}명"),
			FText::AsNumber(CurrentOption.MinStaff), FText::AsNumber(CurrentOption.MaxStaff)));

	FNumberFormattingOptions WorkFormat;
	WorkFormat.MinimumFractionalDigits = 0;
	WorkFormat.MaximumFractionalDigits = 1;
	Text_WorkAmount->SetText(FText::Format(LOCTEXT("WorkAmount", "작업량: {0} 사람-분"),
		FText::AsNumber(CurrentOption.WorkAmount, &WorkFormat)));

	Text_Cost->SetText(FText::Format(LOCTEXT("Cost", "비용: {0} G"), FText::AsNumber(CurrentOption.CostGold)));
	Text_Cost->SetVisibility(CurrentOption.CostGold > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	Text_Reward->SetText(FText::Format(LOCTEXT("Reward", "보상: {0}"), CurrentOption.RewardText));
	Text_Reward->SetVisibility(CurrentOption.RewardText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	SelectedStaff = SelectedStaff <= 0
		? CurrentOption.MinStaff
		: FMath::Clamp(SelectedStaff, CurrentOption.MinStaff, CurrentOption.MaxStaff);
	RefreshStaff();
	return true;
}

void UFF_IncidentDetailPanel::RefreshStaff()
{
	Text_StaffCount->SetText(FText::AsNumber(SelectedStaff));
	Button_Minus->SetIsEnabled(SelectedStaff > CurrentOption.MinStaff);
	Button_Plus->SetIsEnabled(SelectedStaff < CurrentOption.MaxStaff);
	Button_Send->SetIsEnabled(SelectedStaff >= CurrentOption.MinStaff && SelectedStaff <= CurrentOption.MaxStaff);
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
	if (!CurrentOption.bValid)
	{
		return;
	}
	SelectedStaff = FMath::Clamp(SelectedStaff - 1, CurrentOption.MinStaff, CurrentOption.MaxStaff);
	RefreshStaff();
}

void UFF_IncidentDetailPanel::HandlePlusClicked()
{
	if (!CurrentOption.bValid)
	{
		return;
	}
	SelectedStaff = FMath::Clamp(SelectedStaff + 1, CurrentOption.MinStaff, CurrentOption.MaxStaff);
	RefreshStaff();
}

void UFF_IncidentDetailPanel::HandleSendClicked()
{
	if (!IsOpen())
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
