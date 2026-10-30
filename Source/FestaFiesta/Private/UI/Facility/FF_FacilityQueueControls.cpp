#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityQueueControls"

void UFF_FacilityInfoPanel::EnsureQueueSettingsControls()
{
	if (!WidgetTree || IsDesignTime()) return;
	if (!Panel_FacilityQueueSettings)
	{
		UPanelWidget* Host = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Shell")));
		if (!Host) Host = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Content")));
		if (!Host) Host = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details")));
		if (!Host) return;
		auto* Section = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel_FacilityQueueSettings"));
		const auto MakeLabel = [this](const FName Name, const FText& Text)
		{
			auto* Label = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
			Label->SetText(Text);
			FSlateFontInfo Font = Text_Footprint ? Text_Footprint->GetFont() : FFBuildTypography::Make(FFBuildTypography::ERole::Body, 14);
			Font.Size = 14;
			Label->SetFont(Font);
			Label->SetColorAndOpacity(Text_Footprint ? Text_Footprint->GetColorAndOpacity() : FSlateColor(FLinearColor::Black));
			return Label;
		};
		Section->AddChildToVerticalBox(MakeLabel(TEXT("Text_FacilityQueueTitle"), LOCTEXT("Title", "대기열 설정")))
			->SetPadding(FMargin(0, 0, 0, 6));
		auto* Inputs = WidgetTree->ConstructWidget<UHorizontalBox>();
		Section->AddChildToVerticalBox(Inputs)->SetPadding(FMargin(0, 0, 0, 6));
		const auto AddInput = [this, Inputs, &MakeLabel](const FName Name, const FText& Caption, const FText& Hint)
		{
			auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
			auto* ColumnSlot = Inputs->AddChildToHorizontalBox(Column);
			ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColumnSlot->SetPadding(FMargin(Inputs->GetChildrenCount() > 1 ? 8.f : 0.f, 0, 0, 0));
			auto* Label = MakeLabel(NAME_None, Caption);
			Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 0, 0, 4));
			auto* Bounds = WidgetTree->ConstructWidget<USizeBox>();
			Bounds->SetMinDesiredHeight(30.f);
			auto* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
			FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle();
			InputStyle.SetFont(Label->GetFont());
			Input->SetWidgetStyle(InputStyle);
			Input->SetForegroundColor(FLinearColor::Black);
			Input->SetHintText(Hint);
			Input->SetToolTipText(Hint);
			Input->SetSelectAllTextWhenFocused(true);
			Input->SetClearKeyboardFocusOnCommit(false);
			Input->SetRevertTextOnEscape(false);
			Bounds->AddChild(Input);
			Column->AddChildToVerticalBox(Bounds);
			return Input;
		};
		Editable_FacilityQueueTeams = AddInput(TEXT("Editable_FacilityQueueTeams"), LOCTEXT("Teams", "대기 정원 (팀)"), LOCTEXT("TeamRange", "1~512팀"));
		Editable_FacilityQueueSpacing = AddInput(TEXT("Editable_FacilityQueueSpacing"), LOCTEXT("Spacing", "자리 간격 (cm)"), LOCTEXT("SpacingRange", "90~1000cm"));
		Editable_FacilityQueueWidth = AddInput(TEXT("Editable_FacilityQueueWidth"), LOCTEXT("Width", "통로 폭 (cm)"), LOCTEXT("WidthRange", "100~1000cm"));
		Text_FacilityQueueHint = MakeLabel(TEXT("Text_FacilityQueueHint"), FText::GetEmpty());
		Text_FacilityQueueHint->SetAutoWrapText(true);
		Section->AddChildToVerticalBox(Text_FacilityQueueHint)->SetPadding(FMargin(0, 0, 0, 6));
		Button_ApplyFacilityQueueSettings = WidgetTree->ConstructWidget<UFF_BuildToolsActionButton>(UFF_BuildToolsActionButton::StaticClass(), TEXT("Button_ApplyFacilityQueueSettings"));
		if (Button_TabOverview) Button_ApplyFacilityQueueSettings->SetStyle(Button_TabOverview->GetStyle());
		Button_ApplyFacilityQueueSettings->AddChild(MakeLabel(TEXT("Text_ApplyFacilityQueueSettings"), LOCTEXT("Apply", "대기열 설정 적용")));
		auto* ApplyBounds = WidgetTree->ConstructWidget<USizeBox>();
		ApplyBounds->SetMinDesiredHeight(32.f);
		ApplyBounds->AddChild(Button_ApplyFacilityQueueSettings);
		Section->AddChildToVerticalBox(ApplyBounds);
		Host->AddChild(Section);
		Panel_FacilityQueueSettings = Section;
	}
	// Reserve queue controls above the action footer. Only the longer facility
	// details scroll; capacity, both dimensions and Apply stay visible together.
	if (auto* Shell = !Button_TabQueue ? Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Shell"))) : nullptr)
	{
		if (Panel_FacilityQueueSettings->GetParent() != Shell)
		{
			Panel_FacilityQueueSettings->RemoveFromParent();
			Shell->AddChildToVerticalBox(Panel_FacilityQueueSettings);
		}
		if (Panel_Actions && Panel_Actions->GetParent() == Shell)
		{
			// ShiftChild only reorders the UObject slots. Remove/add also updates an
			// already-created SVerticalBox, keeping the visible action rail last.
			const auto* OldSlot = CastChecked<UVerticalBoxSlot>(Panel_Actions->Slot);
			const FMargin FooterPadding = OldSlot->GetPadding();
			const FSlateChildSize Size = OldSlot->GetSize();
			const EHorizontalAlignment HorizontalAlignment = OldSlot->GetHorizontalAlignment();
			const EVerticalAlignment VerticalAlignment = OldSlot->GetVerticalAlignment();
			Panel_Actions->RemoveFromParent();
			auto* NewSlot = Shell->AddChildToVerticalBox(Panel_Actions);
			NewSlot->SetPadding(FooterPadding);
			NewSlot->SetSize(Size);
			NewSlot->SetHorizontalAlignment(HorizontalAlignment);
			NewSlot->SetVerticalAlignment(VerticalAlignment);
		}
		if (auto* QueueSectionSlot = Cast<UVerticalBoxSlot>(Panel_FacilityQueueSettings->Slot))
		{
			QueueSectionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			QueueSectionSlot->SetPadding(FMargin(10, 6, 10, 4));
		}
	}
	for (UEditableTextBox* Input : {Editable_FacilityQueueTeams.Get(), Editable_FacilityQueueSpacing.Get(), Editable_FacilityQueueWidth.Get()})
		if (Input)
		{
			Input->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleQueueSettingsChanged);
			Input->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleQueueSettingsCommitted);
		}
	if (Button_ApplyFacilityQueueSettings)
		Button_ApplyFacilityQueueSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyQueueSettings);
	RefreshQueueSettingsControls();
}

void UFF_FacilityInfoPanel::RefreshQueueSettingsText()
{
	TGuardValue<bool> Guard(bRefreshingQueueSettings, true);
	const FFFZSQueueSettings& Settings = CurrentViewData.QueueSettings;
	if (Editable_FacilityQueueTeams) Editable_FacilityQueueTeams->SetText(FText::FromString(FString::FromInt(Settings.MaxWaitingTeams)));
	if (Editable_FacilityQueueSpacing) Editable_FacilityQueueSpacing->SetText(FText::FromString(FString::SanitizeFloat(Settings.SlotSpacingCm)));
	if (Editable_FacilityQueueWidth) Editable_FacilityQueueWidth->SetText(FText::FromString(FString::SanitizeFloat(Settings.LaneWidthCm)));
	for (UEditableTextBox* Input : {Editable_FacilityQueueTeams.Get(), Editable_FacilityQueueSpacing.Get(), Editable_FacilityQueueWidth.Get()})
		if (Input) Input->SetError(FText::GetEmpty());
	bQueueSettingsDirty = false;
}

void UFF_FacilityInfoPanel::RefreshQueueSettingsControls()
{
	const bool bShow = CurrentViewData.bHasQueueSettings && CurrentViewData.FacilityInstanceId.IsValid() &&
		(Button_TabQueue ? ActivePage == EFF_FacilityInfoPage::Queue :
		 ActivePage == EFF_FacilityInfoPage::Overview || ActivePage == EFF_FacilityInfoPage::Operation);
	if (Panel_FacilityQueueSettings) Panel_FacilityQueueSettings->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (UEditableTextBox* Input : {Editable_FacilityQueueTeams.Get(), Editable_FacilityQueueSpacing.Get(), Editable_FacilityQueueWidth.Get()})
		if (Input) Input->SetIsReadOnly(!CurrentViewData.bCanEditQueueSettings);
	if (Button_ApplyFacilityQueueSettings) Button_ApplyFacilityQueueSettings->SetIsEnabled(bShow && CurrentViewData.bCanEditQueueSettings);
	SetText(Text_FacilityQueueHint, CurrentViewData.QueueSettingsHint);
	if (!CurrentViewData.bCanEditQueueSettings) bQueueSettingsDirty = false;
	if (!CurrentViewData.bCanEditQueueSettings || (!bQueueSettingsDirty && !IsQueueTextInputFocused())) RefreshQueueSettingsText();
}

bool UFF_FacilityInfoPanel::IsQueueTextInputFocused() const
{
	if (!Panel_FacilityQueueSettings || !Panel_FacilityQueueSettings->IsVisible()) return false;
	for (const UEditableTextBox* Input : {Editable_FacilityQueueTeams.Get(), Editable_FacilityQueueSpacing.Get(), Editable_FacilityQueueWidth.Get()})
		if (Input && (Input->HasKeyboardFocus() || Input->HasFocusedDescendants())) return true;
	return false;
}

FReply UFF_FacilityInfoPanel::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (IsQueueTextInputFocused())
	{
		if (Event.GetKey() == EKeys::Escape)
		{
			RefreshQueueSettingsText();
			return FReply::Handled();
		}
		if (Event.GetKey() == EKeys::Enter)
		{
			if (!Event.IsRepeat()) HandleApplyQueueSettings();
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UFF_FacilityInfoPanel::HandleQueueSettingsChanged(const FText& Text)
{
	if (!bRefreshingQueueSettings) bQueueSettingsDirty = true;
}

void UFF_FacilityInfoPanel::HandleQueueSettingsCommitted(const FText& Text, const ETextCommit::Type Method)
{
	if (bRefreshingQueueSettings) return;
	if (Method == ETextCommit::OnEnter) HandleApplyQueueSettings();
	else if (Method == ETextCommit::OnCleared) RefreshQueueSettingsText();
}

void UFF_FacilityInfoPanel::HandleApplyQueueSettings()
{
	if (!CurrentViewData.bHasQueueSettings || !CurrentViewData.bCanEditQueueSettings ||
		!CurrentViewData.FacilityInstanceId.IsValid() || !Panel_FacilityQueueSettings || !Panel_FacilityQueueSettings->IsVisible() ||
		!Editable_FacilityQueueTeams || !Editable_FacilityQueueSpacing || !Editable_FacilityQueueWidth) return;
	FFFZSQueueSettings Settings;
	Settings.bEnabled = true;
	if (!FFBuildToolsModel::TryParseQueueSettingsText(Editable_FacilityQueueTeams->GetText().ToString(),
		Editable_FacilityQueueSpacing->GetText().ToString(), Editable_FacilityQueueWidth->GetText().ToString(),
		Settings.MaxWaitingTeams, Settings.SlotSpacingCm, Settings.LaneWidthCm))
	{
		const FText Error = LOCTEXT("Invalid", "정원은 1~512팀, 자리 간격은 90~1000cm, 통로 폭은 100~1000cm로 입력하세요.");
		SetText(Text_FacilityQueueHint, Error);
		for (UEditableTextBox* Input : {Editable_FacilityQueueTeams.Get(), Editable_FacilityQueueSpacing.Get(), Editable_FacilityQueueWidth.Get()}) Input->SetError(Error);
		return;
	}
	const FGuid ReviewedFacility = CurrentViewData.FacilityInstanceId;
	if (OnQueueSettingsRequested.IsBound() && OnQueueSettingsRequested.Execute(ReviewedFacility, Settings) &&
		CurrentViewData.FacilityInstanceId == ReviewedFacility)
		RefreshQueueSettingsText();
}

bool UFF_FacilityInfoSubsystem::ApplySelectedQueueSettings(const FGuid FacilityInstanceId, const FFFZSQueueSettings& Settings)
{
	if (!FacilityInstanceId.IsValid() || SelectedFacilityId != FacilityInstanceId ||
		SelectionContext != EFF_FacilityInfoContext::Build || bPauseMenuPresentationSuppressed ||
		bRevenueRankingPresentationOpen || !Panel || !Panel->IsVisible()) return false;
	AFFZSFacilityActor* Facility = GetSelectedFacility();
	UFFZSWorldSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Facility || !Zones || !Facility->GetFacilitySpec().UsesServiceQueue() ||
		Facility->GetFacilitySpec().QueueRules.MaxWaitingAgents <= 0 || !Settings.bEnabled) return false;
	FText Error;
	const bool bApplied = Zones->SetFacilityQueueSettings(FacilityInstanceId, Settings, Facility->GetQueueLayout(), Error);
	SetFeedback(bApplied ? LOCTEXT("Applied", "대기열 설정을 변경했습니다.") : Error, !bApplied);
	return bApplied;
}

#undef LOCTEXT_NAMESPACE
