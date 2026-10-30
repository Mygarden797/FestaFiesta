// FF_BuildMultiSelectionWidget.cpp
// 역할: 선택한 시설을 가로 목록에 보여 주고 상세 보기·선택 해제·종료 요청을 전달한다.
// 편집용 위젯 에셋이 없어도 같은 이름의 버튼과 표시 요소를 C++로 만들어 기능을 유지한다.

#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildCategoryIcons.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

#define LOCTEXT_NAMESPACE "FFBuildMultiSelection"

namespace
{
FSlateBrush Surface(const FLinearColor Color, const float Radius,
	const FLinearColor Outline = FLinearColor::Transparent)
{
	return FFBuildPearl::Rounded(Color, Radius, Outline, Outline.A > 0.f ? 1.f : 0.f);
}

UTextBlock* MakeText(UWidgetTree& Tree, const FName Name, const FText& Value,
	const int32 FontSize, const FLinearColor Color, const bool bStrong = false)
{
	UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetText(Value);
	Text->SetFont(FFBuildTypography::Make(bStrong
		? FFBuildTypography::ERole::Label : FFBuildTypography::ERole::Body, FontSize));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

void AddToRow(UHorizontalBox& Row, UWidget& Child, const float RightPadding = 0.0f,
	const bool bFill = false)
{
	UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
	Slot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
	Slot->SetVerticalAlignment(VAlign_Center);
	Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
}

UButton* MakeAction(UWidgetTree& Tree, const FName Name, const FText& Caption,
	const bool bPrimary, const int32 FontSize = 12)
{
	UButton* Button = Tree.ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle Style;
	Style.Normal = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f,
		bPrimary ? FFBuildPearl::Cyan : FFBuildPearl::Line);
	Style.Hovered = Surface(bPrimary ? FFBuildPearl::Charcoal : FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Cyan);
	Style.Pressed = Surface(FFBuildPearl::Charcoal, 20.0f, FFBuildPearl::Cyan);
	Style.Disabled = Surface(FFBuildPearl::Pearl, 20.0f, FFBuildPearl::Line);
	Style.NormalPadding = Style.PressedPadding = FMargin(9.0f, 5.0f);
	Button->SetStyle(Style);
	UTextBlock* Label = MakeText(Tree, NAME_None, Caption, FontSize,
		bPrimary ? FLinearColor::White : FFBuildPearl::Ink, true);
	UButtonSlot* ButtonSlot = CastChecked<UButtonSlot>(Button->AddChild(Label));
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	return Button;
}
}

void UFF_BuildSelectionRemoveHandler::Initialize(UFF_BuildModeSubsystem* InBuildMode, AFFZSFacilityActor* InFacility)
{
	BuildMode = InBuildMode;
	Facility = InFacility;
}

void UFF_BuildSelectionRemoveHandler::HandleClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
		if (AFFZSFacilityActor* Selected = Facility.Get()) System->RemoveMultiSelectedFacility(Selected);
}

UFF_BuildMultiSelectionWidget::UFF_BuildMultiSelectionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UFF_BuildMultiSelectionWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildNativeTree(); }
	ResolveBindings();
	return Super::RebuildWidget();
}

void UFF_BuildMultiSelectionWidget::ResolveBindings()
{
	if (!WidgetTree) { return; }
	Border_MultiSelection = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_MultiSelection")));
	Text_MultiSelectionCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_MultiSelectionCount")));
	Row_SelectedFacilities = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_SelectedFacilities")));
	Button_InspectSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_InspectSelection")));
	Button_ClearSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ClearSelection")));
	Button_ExitMultiSelection = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ExitMultiSelection")));
	Button_MoveGroup = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_MoveGroup")));
	Button_DuplicateGroup = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_DuplicateGroup")));
	Button_RemoveGroup = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_RemoveGroup")));
	Button_RotateLeft = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_RotateLeft")));
	Button_RotateRight = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_RotateRight")));
	Button_ApplyGroup = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ApplyGroup")));
	Button_CancelGroup = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_CancelGroup")));
	Button_SavePreset = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_SavePreset")));
	Button_LoadPreset = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_LoadPreset")));
	Button_DeletePreset = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_DeletePreset")));
	Text_GroupEditingStatus = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_GroupEditingStatus")));
	Edit_PresetName = Cast<UEditableTextBox>(WidgetTree->FindWidget(TEXT("Edit_PresetName")));
	Combo_Presets = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("Combo_Presets")));
	Text_MultiSelectionDesignerHint = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_MultiSelectionDesignerHint")));
	ApprovedSelectionList = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Column_A5_SelectedFacilities")));
}

// Reflow the existing Designer controls instead of rebuilding their actions or
// losing the group/preset tools. Older/fallback trees keep their authored layout.
void UFF_BuildMultiSelectionWidget::ApplyApprovedPanelLayout()
{
	if (!WidgetTree || ApprovedSelectionList) return;
	auto* Bounds = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("Size_MultiSelection")));
	auto* BoundsSlot = Bounds ? Cast<UCanvasPanelSlot>(Bounds->Slot) : nullptr;
	auto* Header = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_MultiSelectionHeader")));
	auto* Actions = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_GroupActions")));
	auto* Presets = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Row_PresetControls")));
	auto* CardsSize = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("Size_MultiSelectionCards")));
	auto* Scroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("Scroll_SelectedFacilities")));
	if (!Bounds || !BoundsSlot || !Header || !Actions || !Presets || !CardsSize || !Scroll ||
		!Row_SelectedFacilities || Row_SelectedFacilities->GetParent() != Scroll) return;
	const auto Child = [this](UHorizontalBox* Parent, const TCHAR* Name) -> UWidget*
	{
		UWidget* Widget = WidgetTree->FindWidget(Name);
		return Widget && Widget->GetParent() == Parent ? Widget : nullptr;
	};
	UWidget* Title = Child(Header, TEXT("Text_MultiSelectionTitle"));
	UWidget* Count = Child(Header, TEXT("Text_MultiSelectionCount"));
	UWidget* Spacer = Child(Header, TEXT("Spacer_MultiSelectionHeader"));
	UWidget* Inspect = Child(Header, TEXT("Button_InspectSelection"));
	UWidget* Clear = Child(Header, TEXT("Button_ClearSelection"));
	UWidget* Exit = Child(Header, TEXT("Button_ExitMultiSelection"));
	UWidget* Move = Child(Actions, TEXT("Button_MoveGroup"));
	UWidget* Duplicate = Child(Actions, TEXT("Button_DuplicateGroup"));
	UWidget* Remove = Child(Actions, TEXT("Button_RemoveGroup"));
	UWidget* GroupSpacer = Child(Actions, TEXT("Spacer_GroupActions"));
	UWidget* Left = Child(Actions, TEXT("Button_RotateLeft"));
	UWidget* Right = Child(Actions, TEXT("Button_RotateRight"));
	UWidget* Cancel = Child(Actions, TEXT("Button_CancelGroup"));
	UWidget* Apply = Child(Actions, TEXT("Button_ApplyGroup"));
	UWidget* Name = Child(Presets, TEXT("Edit_PresetName"));
	UWidget* Save = Child(Presets, TEXT("Button_SavePreset"));
	UWidget* Combo = Child(Presets, TEXT("Combo_Presets"));
	UWidget* Load = Child(Presets, TEXT("Button_LoadPreset"));
	UWidget* Delete = Child(Presets, TEXT("Button_DeletePreset"));
	for (UWidget* Required : {Title, Count, Spacer, Inspect, Clear, Exit, Move, Duplicate, Remove,
		GroupSpacer, Left, Right, Cancel, Apply, Name, Save, Combo, Load, Delete})
		if (!Required) return;
	// Refuse partial migration if a newer Designer added controls to these rows.
	if (Header->GetChildrenCount() != 6 || Actions->GetChildrenCount() != 8 || Presets->GetChildrenCount() != 5) return;
	const auto MakeStack = [this](UHorizontalBox& Original, const TCHAR* StackName)
	{
		for (UWidget* Widget : Original.GetAllChildren()) Widget->RemoveFromParent();
		auto* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), StackName);
		AddToRow(Original, *Stack, 0.f, true);
		return Stack;
	};
	const auto MakeLine = [this](UVerticalBox& Stack)
	{
		auto* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		Stack.AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
		return Line;
	};
	const auto Fill = [](UHorizontalBox& Line, UWidget& Widget, float Gap)
	{
		AddToRow(Line, Widget, Gap, true);
	};
	auto* HeaderStack = MakeStack(*Header, TEXT("Column_A5_MultiHeader"));
	auto* HeaderTitle = MakeLine(*HeaderStack);
	AddToRow(*HeaderTitle, *Title, 8.f); AddToRow(*HeaderTitle, *Count, 0.f);
	AddToRow(*HeaderTitle, *Spacer, 0.f, true); AddToRow(*HeaderTitle, *Exit);
	auto* HeaderActions = MakeLine(*HeaderStack);
	Fill(*HeaderActions, *Inspect, 5.f); Fill(*HeaderActions, *Clear, 0.f);
	auto* ActionStack = MakeStack(*Actions, TEXT("Column_A5_GroupActions"));
	auto* GroupLine = MakeLine(*ActionStack);
	Fill(*GroupLine, *Move, 5.f); Fill(*GroupLine, *Duplicate, 5.f); Fill(*GroupLine, *Remove, 0.f);
	// The authored spacer is no longer needed, but stays in the live widget tree.
	GroupSpacer->SetVisibility(ESlateVisibility::Collapsed); AddToRow(*GroupLine, *GroupSpacer);
	auto* RotationLine = MakeLine(*ActionStack);
	Fill(*RotationLine, *Left, 5.f); Fill(*RotationLine, *Right, 0.f);
	auto* ConfirmLine = MakeLine(*ActionStack);
	Fill(*ConfirmLine, *Cancel, 5.f); Fill(*ConfirmLine, *Apply, 0.f);
	auto* PresetStack = MakeStack(*Presets, TEXT("Column_A5_PresetControls"));
	auto* SaveLine = MakeLine(*PresetStack); Fill(*SaveLine, *Name, 5.f); Fill(*SaveLine, *Save, 0.f);
	auto* PickLine = MakeLine(*PresetStack); Fill(*PickLine, *Combo, 0.f);
	auto* LoadLine = MakeLine(*PresetStack); Fill(*LoadLine, *Load, 5.f); Fill(*LoadLine, *Delete, 0.f);
	if (auto* PresetHint = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_PresetHint"))))
	{
		PresetHint->SetAutoWrapText(true);
		if (auto* HintSlot = Cast<UHorizontalBoxSlot>(PresetHint->Slot))
			HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ApprovedSelectionList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column_A5_SelectedFacilities"));
	Row_SelectedFacilities->ClearChildren();
	AddToRow(*Row_SelectedFacilities, *ApprovedSelectionList, 0.f, true);
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	CardsSize->SetHeightOverride(156.f);
	Bounds->SetWidthOverride(390.f);
	BoundsSlot->SetAnchors(FAnchors(1.f, 1.f));
	BoundsSlot->SetAlignment(FVector2D(1.f, 1.f));
	BoundsSlot->SetPosition(FVector2D(-24.f, -40.f));
	BoundsSlot->SetAutoSize(true);
}

void UFF_BuildMultiSelectionWidget::AddSelectedFacilityWidget(UWidget& Widget)
{
	if (ApprovedSelectionList)
		ApprovedSelectionList->AddChildToVerticalBox(&Widget)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	else if (Row_SelectedFacilities) AddToRow(*Row_SelectedFacilities, Widget, 8.f);
}

void UFF_BuildMultiSelectionWidget::AvoidBottomHud()
{
	if (!bAvoidBottomHud || !WidgetTree) return;
	UWidget* Bounds = WidgetTree->FindWidget(TEXT("Size_MultiSelection"));
	auto* CanvasSlot = Bounds ? Cast<UCanvasPanelSlot>(Bounds->Slot) : nullptr;
	if (!CanvasSlot || CanvasSlot->GetAnchors().Minimum.Y != 1.f || CanvasSlot->GetAnchors().Maximum.Y != 1.f ||
		CanvasSlot->GetAlignment().Y != 1.f) return;
	TArray<UUserWidget*> Bars;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Bars, UFF_PrepHudBar::StaticClass(), false);
	for (const UUserWidget* Candidate : Bars)
		if (const auto* Bar = Cast<UFF_PrepHudBar>(Candidate))
			if (Bar->GetOwningPlayer() == GetOwningPlayer() && Bar->IsVisible())
			{
				FVector2D Position = CanvasSlot->GetPosition();
				Position.Y = FMath::Min(Position.Y, -static_cast<double>(Bar->GetBottomBarHeight()));
				CanvasSlot->SetPosition(Position);
			}
}

// Used only when no Designer tree exists. Saved WBPs are never rewritten at runtime.
void UFF_BuildMultiSelectionWidget::BuildNativeGroupControls()
{
	if (!Border_MultiSelection || Text_GroupEditingStatus) return;
	UWidget* Existing = Border_MultiSelection->GetContent();
	Border_MultiSelection->ClearChildren();
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Border_MultiSelection->AddChild(Body);
	if (Existing) Body->AddChildToVerticalBox(Existing);
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	const auto AddAction = [&](UHorizontalBox* Row, const TCHAR* Name, const FText& Caption, bool bPrimary = false)
	{
		UButton* Button = MakeAction(*WidgetTree, Name, Caption, bPrimary, 14);
		AddToRow(*Row, *Button, 6.f);
		return Button;
	};
	Button_MoveGroup = AddAction(Actions, TEXT("Button_MoveGroup"), LOCTEXT("MoveGroup", "묶음 이동"));
	Button_DuplicateGroup = AddAction(Actions, TEXT("Button_DuplicateGroup"), LOCTEXT("DuplicateGroup", "복제"));
	Button_RemoveGroup = AddAction(Actions, TEXT("Button_RemoveGroup"), LOCTEXT("RemoveGroup", "묶음 철거"));
	Button_RotateLeft = AddAction(Actions, TEXT("Button_RotateLeft"), LOCTEXT("GroupLeft", "왼쪽 회전 Q"));
	Button_RotateRight = AddAction(Actions, TEXT("Button_RotateRight"), LOCTEXT("GroupRight", "오른쪽 회전 E"));
	Button_ApplyGroup = AddAction(Actions, TEXT("Button_ApplyGroup"), LOCTEXT("ApplyGroup", "확정"), true);
	Button_CancelGroup = AddAction(Actions, TEXT("Button_CancelGroup"), LOCTEXT("CancelGroup", "취소"));
	Button_DuplicateGroup->SetToolTipText(LOCTEXT("DuplicateCost", "기본 단계의 새 시설을 만듭니다. 설치비가 적용되며 간격과 대기열 모양을 유지합니다."));
	Button_RemoveGroup->SetToolTipText(LOCTEXT("RemoveUndo", "선택 전체를 철거합니다. 불가능한 시설이 하나라도 있으면 전체가 취소됩니다. 실행 취소로 복구할 수 있습니다."));
	UHorizontalBox* Presets = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(Presets)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	Edit_PresetName = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Edit_PresetName"));
	Edit_PresetName->SetHintText(LOCTEXT("PresetHint", "먹거리 거리 / 휴게 공간"));
	Edit_PresetName->SetToolTipText(LOCTEXT("PresetOverwrite", "1~40자. 같은 이름으로 저장하면 기존 프리셋을 덮어씁니다."));
	FEditableTextBoxStyle NameStyle = Edit_PresetName->GetWidgetStyle();
	NameStyle.SetFont(FFBuildTypography::Make(FFBuildTypography::ERole::Body, 14));
	Edit_PresetName->SetWidgetStyle(NameStyle);
	AddToRow(*Presets, *Edit_PresetName, 6.f, true);
	Button_SavePreset = AddAction(Presets, TEXT("Button_SavePreset"), LOCTEXT("SavePreset", "이름으로 저장"));
	Combo_Presets = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("Combo_Presets"));
	Combo_Presets->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GeneratePresetOption);
	AddToRow(*Presets, *Combo_Presets, 6.f, true);
	Button_LoadPreset = AddAction(Presets, TEXT("Button_LoadPreset"), LOCTEXT("LoadPreset", "불러와 배치"));
	Button_DeletePreset = AddAction(Presets, TEXT("Button_DeletePreset"), LOCTEXT("DeletePreset", "프리셋 삭제"));
	Text_GroupEditingStatus = MakeText(*WidgetTree, TEXT("Text_GroupEditingStatus"),
		LOCTEXT("GroupHint", "이동·복제 후 Q/E로 묶음을 회전할 수 있습니다. 복제·프리셋은 설치비가 적용됩니다."), 14, FFBuildPearl::Muted);
	Text_GroupEditingStatus->SetAutoWrapText(true);
	Body->AddChildToVerticalBox(Text_GroupEditingStatus)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
}

void UFF_BuildMultiSelectionWidget::RefreshGroupControls()
{
	UFF_BuildModeSubsystem* System = BuildMode.Get();
	if (!System || !Text_GroupEditingStatus) return;
	const bool bPreview = System->IsGroupPlacementActive();
	const bool bSelected = System->GetMultiSelectionCount() > 0;
	for (UButton* Button : {Button_MoveGroup.Get(), Button_DuplicateGroup.Get(), Button_RemoveGroup.Get(), Button_SavePreset.Get()})
		if (Button) Button->SetIsEnabled(bSelected && !bPreview);
	for (UButton* Button : {Button_RotateLeft.Get(), Button_RotateRight.Get(), Button_ApplyGroup.Get(), Button_CancelGroup.Get()})
		if (Button) Button->SetIsEnabled(bPreview);
	if (Button_ClearSelection) Button_ClearSelection->SetIsEnabled(bSelected && !bPreview);
	if (Button_InspectSelection) Button_InspectSelection->SetIsEnabled(!bPreview);
	const TArray<FString> Names = System->GetGroupPresetNames();
	if (Combo_Presets && (!bPresetOptionsInitialized || Names != DisplayedPresetNames))
	{
		bPresetOptionsInitialized = true;
		const FString Selected = Combo_Presets->GetSelectedOption();
		DisplayedPresetNames = Names;
		Combo_Presets->ClearOptions();
		for (const FString& Name : Names) Combo_Presets->AddOption(Name);
		if (Names.Contains(Selected)) Combo_Presets->SetSelectedOption(Selected);
		else if (!Names.IsEmpty()) Combo_Presets->SetSelectedOption(Names[0]);
		else
		{
			const FString Empty = LOCTEXT("NoPresets", "저장된 프리셋 없음").ToString();
			Combo_Presets->AddOption(Empty);
			Combo_Presets->SetSelectedOption(Empty);
		}
	}
	if (Combo_Presets) Combo_Presets->SetIsEnabled(!bPreview && !Names.IsEmpty());
	if (Button_LoadPreset) Button_LoadPreset->SetIsEnabled(!Names.IsEmpty() && !bPreview);
	if (Button_DeletePreset) Button_DeletePreset->SetIsEnabled(!Names.IsEmpty() && !bPreview);
	if (!System->GetGroupEditingStatus().IsEmpty()) Text_GroupEditingStatus->SetText(System->GetGroupEditingStatus());
}

void UFF_BuildMultiSelectionWidget::BuildNativeTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(920.0f);
	Width->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_MultiSelection"));
	Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
	Panel->SetBrushColor(FLinearColor::White);
	Panel->SetPadding(FMargin(13.0f, 9.0f));
	Width->AddChild(Panel);
	UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Width);
	Position->SetAnchors(FAnchors(0.5f, 1.0f));
	Position->SetAlignment(FVector2D(0.5f, 1.0f));
	Position->SetPosition(FVector2D(0.0f, -108.0f));
	Position->SetAutoSize(true);

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Body);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	AddToRow(*Header, *MakeText(*WidgetTree, NAME_None,
		LOCTEXT("Mode", "다중 선택"), 15, FFBuildPearl::Ink, true), 10.0f);
	AddToRow(*Header, *MakeText(*WidgetTree, TEXT("Text_MultiSelectionCount"),
		LOCTEXT("ZeroSelected", "선택 0개"), 12, FFBuildPearl::Muted), 0.0f);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
	AddToRow(*Header, *Spacer, 0.0f, true);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_InspectSelection"),
		LOCTEXT("Inspect", "상세 보기"), false), 7.0f);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_ClearSelection"),
		LOCTEXT("Clear", "전체 해제"), false), 7.0f);
	AddToRow(*Header, *MakeAction(*WidgetTree, TEXT("Button_ExitMultiSelection"),
		LOCTEXT("Exit", "종료 ×"), true), 0.0f);

	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
	ScrollSize->SetHeightOverride(64.0f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetOrientation(Orient_Horizontal);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetAllowOverscroll(false);
	ScrollSize->AddChild(Scroll);
	Body->AddChildToVerticalBox(ScrollSize);
	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("Row_SelectedFacilities"));
	Scroll->AddChild(Cards);
	Border_MultiSelection = Panel;
	BuildNativeGroupControls();
}

void UFF_BuildMultiSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyApprovedPanelLayout();
	BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		System->OnBuildMultiSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectionChanged);
	}
	if (UFF_BuildCatalogSubsystem* Source = Catalog.Get())
	{
		Source->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
		Source->OnCatalogThumbnailsChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
	}
	if (Button_ClearSelection)
	{
		Button_ClearSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClearClicked);
	}
	if (Button_InspectSelection)
	{
		Button_InspectSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInspectClicked);
	}
	if (Button_ExitMultiSelection)
	{
		Button_ExitMultiSelection->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);
	}
	if (Button_MoveGroup) Button_MoveGroup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMoveGroup);
	if (Button_DuplicateGroup) Button_DuplicateGroup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDuplicateGroup);
	if (Button_RemoveGroup) Button_RemoveGroup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveGroup);
	if (Button_RotateLeft) Button_RotateLeft->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateLeft);
	if (Button_RotateRight) Button_RotateRight->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateRight);
	if (Button_ApplyGroup) Button_ApplyGroup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyGroup);
	if (Button_CancelGroup) Button_CancelGroup->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelGroup);
	if (Button_SavePreset) Button_SavePreset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSavePreset);
	if (Button_LoadPreset) Button_LoadPreset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoadPreset);
	if (Button_DeletePreset) Button_DeletePreset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeletePreset);
	if (Combo_Presets) Combo_Presets->OnOpening.AddUniqueDynamic(this, &ThisClass::HandlePresetMenuOpening);
	RefreshSelection();
}

void UFF_BuildMultiSelectionWidget::NativeDestruct()
{
	if (Combo_Presets) Combo_Presets->OnOpening.RemoveDynamic(this, &ThisClass::HandlePresetMenuOpening);
	bPresetMenuWasOpen = false;
	SetMarquee(false, FVector2D::ZeroVector, FVector2D::ZeroVector);
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		System->OnBuildMultiSelectionChanged.RemoveDynamic(this, &ThisClass::HandleSelectionChanged);
	}
	if (UFF_BuildCatalogSubsystem* Source = Catalog.Get())
	{
		Source->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged);
		Source->OnCatalogThumbnailsChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged);
	}
	if (Button_ClearSelection) { Button_ClearSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleClearClicked); }
	if (Button_InspectSelection) { Button_InspectSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleInspectClicked); }
	if (Button_ExitMultiSelection) { Button_ExitMultiSelection->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked); }
	BuildMode.Reset();
	Catalog.Reset();
	Super::NativeDestruct();
}

void UFF_BuildMultiSelectionWidget::RefreshSelection()
{
	AvoidBottomHud();
	if (!Border_MultiSelection || !Text_MultiSelectionCount || !Row_SelectedFacilities)
	{
		TakeWidget();
	}
	UFF_BuildModeSubsystem* System = BuildMode.Get();
	if (!System && GetWorld())
	{
		System = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>();
		BuildMode = System;
	}
	const bool bActive = System && System->IsMultiSelectionActive();
	if (Border_MultiSelection)
	{
		Border_MultiSelection->SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetVisibility(bActive || bMarqueeVisible
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!Row_SelectedFacilities || !Text_MultiSelectionCount) { return; }
	if (!bActive)
	{
		SelectionRemoveHandlers.Reset();
		if (ApprovedSelectionList) ApprovedSelectionList->ClearChildren();
		else Row_SelectedFacilities->ClearChildren();
		if (Button_ClearSelection) { Button_ClearSelection->SetIsEnabled(false); }
		if (Button_InspectSelection) { Button_InspectSelection->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	const TArray<AFFZSFacilityActor*> Facilities = System->GetMultiSelectedFacilities();
	Text_MultiSelectionCount->SetText(FText::Format(LOCTEXT("SelectedCount", "선택 {0}개"),
		FText::AsNumber(System->GetMultiSelectionCount())));
	if (Button_ClearSelection) { Button_ClearSelection->SetIsEnabled(!Facilities.IsEmpty()); }
	if (Button_InspectSelection)
	{
		Button_InspectSelection->SetVisibility(Facilities.Num() == 1
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshGroupControls();
	SelectionRemoveHandlers.Reset();
	if (ApprovedSelectionList) ApprovedSelectionList->ClearChildren();
	else Row_SelectedFacilities->ClearChildren();
	if (Text_MultiSelectionDesignerHint) Text_MultiSelectionDesignerHint->SetVisibility(Facilities.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Facilities.IsEmpty())
	{
		if (!Text_MultiSelectionDesignerHint) AddEmptyHint();
		return;
	}
	int32 DisplayIndex = 0;
	for (const AFFZSFacilityActor* Facility : Facilities)
	{
		if (IsValid(Facility)) { AddFacilityCard(*Facility, ++DisplayIndex); }
	}
}

void UFF_BuildMultiSelectionWidget::AddEmptyHint()
{
	if (!WidgetTree || !Row_SelectedFacilities) { return; }
	AddSelectedFacilityWidget(*MakeText(*WidgetTree, NAME_None,
		LOCTEXT("EmptyHint", "시설을 클릭하거나 드래그하여 선택하세요"),
		13, FFBuildPearl::Muted));
}

void UFF_BuildMultiSelectionWidget::AddFacilityCard(const AFFZSFacilityActor& Facility,
	const int32 DisplayIndex)
{
	if (!WidgetTree || !Row_SelectedFacilities) { return; }
	const FFFZSFacilitySpec& Spec = Facility.GetFacilitySpec();
	FFF_BuildCatalogEntry Entry;
	const bool bHasEntry = Catalog.IsValid() && Catalog->FindEntry(Spec.FacilityDefinitionId, Entry);
	const FText Name = bHasEntry && !Entry.DisplayName.IsEmpty() ? Entry.DisplayName
		: !Spec.DisplayName.IsEmpty() ? Spec.DisplayName : LOCTEXT("UnnamedFacility", "시설");

	if (UClass* CardClass = SelectedFacilityCardClass.LoadSynchronous())
	{
		if (CardClass->IsChildOf(UUserWidget::StaticClass()))
		{
			UUserWidget* Card = CreateWidget<UUserWidget>(GetWorld(), CardClass);
			if (Card)
			{
				Card->TakeWidget();
				if (UTextBlock* Label = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_FacilityName")))) Label->SetText(Name);
				if (UTextBlock* Index = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("Text_FacilityIndex"))))
					Index->SetText(FText::Format(LOCTEXT("CardIndex", "{0}번 시설"), FText::AsNumber(DisplayIndex)));
				if (UImage* Thumbnail = Cast<UImage>(Card->GetWidgetFromName(TEXT("Image_FacilityThumbnail"))))
				{
					if (bHasEntry && Entry.Thumbnail.Get()) Thumbnail->SetBrushFromTexture(Entry.Thumbnail.Get(), true);
					else Thumbnail->SetBrush(FFBuildCategoryIcons::Resolve(false, Spec.Category));
				}
				if (UButton* RemoveButton = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_RemoveSelection"))))
				{
					auto* Handler = NewObject<UFF_BuildSelectionRemoveHandler>(this);
					Handler->Initialize(BuildMode.Get(), const_cast<AFFZSFacilityActor*>(&Facility));
					SelectionRemoveHandlers.Add(Handler);
					RemoveButton->OnClicked.AddDynamic(Handler, &UFF_BuildSelectionRemoveHandler::HandleClicked);
					RemoveButton->SetIsEnabled(!BuildMode.IsValid() || !BuildMode->IsGroupPlacementActive());
					RemoveButton->SetToolTipText(LOCTEXT("RemoveOneSelection", "이 시설 선택 해제"));
				}
				Card->SetToolTipText(Name);
				AddSelectedFacilityWidget(*Card);
				return;
			}
		}
	}
	// Missing optional card asset uses the same data with a minimal native presentation.
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(180.0f);
	Size->SetHeightOverride(52.0f);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrush(Surface(FFBuildPearl::Pearl, 14.0f, FFBuildPearl::Cyan));
	Card->SetBrushColor(FLinearColor::White);
	Card->SetPadding(FMargin(6.0f));
	Size->AddChild(Card);
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
	Card->AddChild(Content);

	UBorder* ImageGround = WidgetTree->ConstructWidget<UBorder>();
	ImageGround->SetBrush(Surface(FFBuildPearl::Pearl, 10.0f, FFBuildPearl::Line));
	ImageGround->SetBrushColor(FLinearColor::White);
	ImageGround->SetPadding(FMargin(4.0f));
	USizeBox* ImageSize = WidgetTree->ConstructWidget<USizeBox>();
	ImageSize->SetWidthOverride(40.0f);
	ImageSize->SetHeightOverride(40.0f);
	ImageSize->AddChild(ImageGround);
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	if (bHasEntry && Entry.Thumbnail.Get())
	{
		Image->SetBrushFromTexture(Entry.Thumbnail.Get(), true);
	}
	else
	{
		Image->SetBrush(FFBuildCategoryIcons::Resolve(false, Spec.Category));
	}
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	ImageGround->AddChild(Image);
	AddToRow(*Content, *ImageSize, 7.0f);

	UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
	AddToRow(*Content, *Copy, 0.0f, true);
	Copy->AddChildToVerticalBox(MakeText(*WidgetTree, NAME_None,
		FText::Format(LOCTEXT("CardIndex", "{0}번 시설"), FText::AsNumber(DisplayIndex)),
		10, FFBuildPearl::Muted));
	Copy->AddChildToVerticalBox(MakeText(*WidgetTree, NAME_None, Name, 13, FFBuildPearl::Ink, true));
	Card->SetToolTipText(Name);
	AddSelectedFacilityWidget(*Size);
}

bool UFF_BuildMultiSelectionWidget::IsPointerOverStrip() const
{
	if (!Border_MultiSelection || !Border_MultiSelection->IsVisible() ||
		GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}
	return Border_MultiSelection->IsHovered() || (FSlateApplication::IsInitialized() &&
		Border_MultiSelection->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos()));
}

void UFF_BuildMultiSelectionWidget::SetMarquee(const bool bVisible,
	const FVector2D StartViewportPx, const FVector2D EndViewportPx)
{
	if (bMarqueeVisible == bVisible && (!bVisible ||
		(MarqueeStartViewportPx == StartViewportPx && MarqueeEndViewportPx == EndViewportPx)))
	{
		return;
	}
	bMarqueeVisible = bVisible;
	MarqueeStartViewportPx = StartViewportPx;
	MarqueeEndViewportPx = EndViewportPx;
	const bool bActive = BuildMode.IsValid() && BuildMode->IsMultiSelectionActive();
	SetVisibility(bActive || bMarqueeVisible
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	InvalidateLayoutAndVolatility();
}

int32 UFF_BuildMultiSelectionWidget::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bMarqueeVisible) { return BaseLayer; }
	const FVector2D ViewportPx = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
	if (ViewportPx.X <= 0.0f || ViewportPx.Y <= 0.0f) { return BaseLayer; }
	const FVector2D Scale(AllottedGeometry.GetLocalSize().X / ViewportPx.X,
		AllottedGeometry.GetLocalSize().Y / ViewportPx.Y);
	const FVector2D A(MarqueeStartViewportPx.X * Scale.X, MarqueeStartViewportPx.Y * Scale.Y);
	const FVector2D B(MarqueeEndViewportPx.X * Scale.X, MarqueeEndViewportPx.Y * Scale.Y);
	const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
	const FVector2D Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
	const FVector2D Size = Max - Min;
	if (Size.X < 1.0f || Size.Y < 1.0f) { return BaseLayer; }
	FLinearColor FillColor = FFBuildPearl::Cyan;
	FillColor.A = 0.17f;
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Min))),
		FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FillColor);
	const TArray<FVector2f> Outline = {
		FVector2f(Min), FVector2f(Max.X, Min.Y), FVector2f(Max),
		FVector2f(Min.X, Max.Y), FVector2f(Min)
	};
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2,
		AllottedGeometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None,
		FFBuildPearl::Cyan, true, 2.0f);
	OutDrawElements.PopClip();
	return BaseLayer + 2;
}

void UFF_BuildMultiSelectionWidget::HandleSelectionChanged() { RefreshSelection(); }
void UFF_BuildMultiSelectionWidget::HandleCatalogChanged() { RefreshSelection(); }
void UFF_BuildMultiSelectionWidget::HandleInspectClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		const TArray<AFFZSFacilityActor*> Selected = System->GetMultiSelectedFacilities();
		if (Selected.Num() == 1 && IsValid(Selected[0]))
		{
			System->InspectMultiSelectedFacility(Selected[0]);
		}
	}
}
void UFF_BuildMultiSelectionWidget::HandleClearClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get())
	{
		System->ClearMultiSelection();
		System->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleExitClicked()
{
	if (UFF_BuildModeSubsystem* System = BuildMode.Get()) { System->ToggleMultiSelection(); }
}

void UFF_BuildMultiSelectionWidget::HandleMoveGroup()
{
	if (BuildMode.IsValid())
	{
		BuildMode->BeginGroupMove();
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleDuplicateGroup()
{
	if (BuildMode.IsValid())
	{
		BuildMode->BeginGroupDuplicate();
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleRemoveGroup()
{
	if (BuildMode.IsValid())
	{
		BuildMode->RemoveSelectedGroup();
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleRotateLeft()
{
	if (BuildMode.IsValid())
	{
		BuildMode->RotateGroupPlacement(-BuildMode->GetPlacementRotationSnapDegrees());
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleRotateRight()
{
	if (BuildMode.IsValid())
	{
		BuildMode->RotateGroupPlacement(BuildMode->GetPlacementRotationSnapDegrees());
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleApplyGroup()
{
	if (BuildMode.IsValid())
	{
		BuildMode->CommitGroupPlacement();
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleCancelGroup()
{
	if (BuildMode.IsValid())
	{
		BuildMode->CancelGroupPlacement();
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleSavePreset()
{
	if (BuildMode.IsValid() && Edit_PresetName)
	{
		BuildMode->SaveSelectedGroupPreset(Edit_PresetName->GetText().ToString());
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleLoadPreset()
{
	if (BuildMode.IsValid() && Combo_Presets)
	{
		BuildMode->BeginGroupPreset(Combo_Presets->GetSelectedOption());
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}
void UFF_BuildMultiSelectionWidget::HandleDeletePreset()
{
	if (BuildMode.IsValid() && Combo_Presets)
	{
		BuildMode->DeleteGroupPreset(Combo_Presets->GetSelectedOption());
		BuildMode->RestoreMultiSelectionViewportFocus();
	}
}

void UFF_BuildMultiSelectionWidget::HandlePresetMenuOpening()
{
	bPresetMenuWasOpen = true;
}

void UFF_BuildMultiSelectionWidget::UpdatePresetMenuFocus()
{
	// Keep the pending close while the app is inactive; returning from Alt+Tab
	// must still release a combo that retained keyboard focus.
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) return;
	if (!bPresetMenuWasOpen || !Combo_Presets || Combo_Presets->IsOpen()) return;
	bPresetMenuWasOpen = false;
	APlayerController* Controller = GetOwningPlayer();
	// SComboBox restores its own focus after closing, even when the same option
	// was chosen. Wait for that close in the existing build-input tick, and only
	// reclaim focus still owned by the combo. Clicking a text field keeps it.
	if (Controller && (Combo_Presets->HasUserFocus(Controller) ||
		Combo_Presets->HasUserFocusedDescendants(Controller)))
	{
		if (BuildMode.IsValid()) BuildMode->RestoreMultiSelectionViewportFocus();
	}
}

UWidget* UFF_BuildMultiSelectionWidget::GeneratePresetOption(FString Name)
{
	return MakeText(*WidgetTree, NAME_None, FText::FromString(Name), 14, FFBuildPearl::Ink);
}

#undef LOCTEXT_NAMESPACE
