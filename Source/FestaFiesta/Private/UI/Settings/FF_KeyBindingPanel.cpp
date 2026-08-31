// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UI/Settings/FF_KeyBindingDefinitions.h"
#include "UI/Settings/FF_KeyBindingLayoutMetrics.h"
#include "UI/Settings/FF_KeyBindingRuntimeSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "Input/Reply.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#define LOCTEXT_NAMESPACE "FFKeyBindingPanel"

namespace
{
	constexpr TCHAR PretendardFontPath[] =
		TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard");
	const FName FixedBuildModeBindingName(TEXT("FF.Fixed.BuildMode"));

	struct FAnsiKeyDefinition
	{
		FKey Key;
		const TCHAR* Label = TEXT("");
		float WidthUnits = 1.0f;
		float HeightRows = 1.0f;
		float StartX = -1.0f;
	};

	FAnsiKeyDefinition Key(
		const FKey& InKey,
		const TCHAR* InLabel,
		const float WidthUnits = 1.0f,
		const float HeightRows = 1.0f)
	{
		return { InKey, InLabel, WidthUnits, HeightRows, -1.0f };
	}

	FAnsiKeyDefinition KeyAt(
		const FKey& InKey,
		const TCHAR* InLabel,
		const float StartX,
		const float WidthUnits = 1.0f,
		const float HeightRows = 1.0f)
	{
		return { InKey, InLabel, WidthUnits, HeightRows, StartX };
	}

	const TArray<TArray<FAnsiKeyDefinition>>& GetAnsiKeyboardRows()
	{
		// UE does not expose Print Screen as an EKey. The remaining standard full-size ANSI keys
		// are laid out in their physical rows, including the navigation cluster and number pad.
		static const TArray<TArray<FAnsiKeyDefinition>> Rows =
		{
			{
				KeyAt(EKeys::Escape, TEXT("Esc"), 0.f, FFF_KeyBindingLayoutMetrics::EscapeWidthUnits),
				KeyAt(EKeys::F1, TEXT("F1"), FFF_KeyBindingLayoutMetrics::FunctionF1X), Key(EKeys::F2, TEXT("F2")),
				Key(EKeys::F3, TEXT("F3")), Key(EKeys::F4, TEXT("F4")),
				KeyAt(EKeys::F5, TEXT("F5"), FFF_KeyBindingLayoutMetrics::FunctionF5X), Key(EKeys::F6, TEXT("F6")),
				Key(EKeys::F7, TEXT("F7")), Key(EKeys::F8, TEXT("F8")),
				KeyAt(EKeys::F9, TEXT("F9"), FFF_KeyBindingLayoutMetrics::FunctionF9X), Key(EKeys::F10, TEXT("F10")),
				Key(EKeys::F11, TEXT("F11")), Key(EKeys::F12, TEXT("F12")),
				// UE has no Print Screen EKey. Preserve its physical slot before Scroll Lock and Pause.
				KeyAt(EKeys::ScrollLock, TEXT("ScrLk"),
					FFF_KeyBindingLayoutMetrics::NavigationClusterX + FFF_KeyBindingLayoutMetrics::KeyPitch),
				Key(EKeys::Pause, TEXT("Pause"))
			},
			{
				KeyAt(EKeys::Tilde, TEXT("`"), 0.f), Key(EKeys::One, TEXT("1")),
				Key(EKeys::Two, TEXT("2")), Key(EKeys::Three, TEXT("3")),
				Key(EKeys::Four, TEXT("4")), Key(EKeys::Five, TEXT("5")),
				Key(EKeys::Six, TEXT("6")), Key(EKeys::Seven, TEXT("7")),
				Key(EKeys::Eight, TEXT("8")), Key(EKeys::Nine, TEXT("9")),
				Key(EKeys::Zero, TEXT("0")), Key(EKeys::Hyphen, TEXT("-")),
				Key(EKeys::Equals, TEXT("=")), Key(EKeys::BackSpace, TEXT("Back"), 2.0f),
				KeyAt(EKeys::Insert, TEXT("INS"), FFF_KeyBindingLayoutMetrics::NavigationClusterX),
				Key(EKeys::Home, TEXT("HOM")), Key(EKeys::PageUp, TEXT("PGU")),
				KeyAt(EKeys::NumLock, TEXT("NUM"), FFF_KeyBindingLayoutMetrics::NumberPadClusterX),
				Key(EKeys::Divide, TEXT("/")),
				Key(EKeys::Multiply, TEXT("*")), Key(EKeys::Subtract, TEXT("-"))
			},
			{
				KeyAt(EKeys::Tab, TEXT("Tab"), 0.f, 1.5f), Key(EKeys::Q, TEXT("Q")),
				Key(EKeys::W, TEXT("W")), Key(EKeys::E, TEXT("E")), Key(EKeys::R, TEXT("R")),
				Key(EKeys::T, TEXT("T")), Key(EKeys::Y, TEXT("Y")), Key(EKeys::U, TEXT("U")),
				Key(EKeys::I, TEXT("I")), Key(EKeys::O, TEXT("O")), Key(EKeys::P, TEXT("P")),
				Key(EKeys::LeftBracket, TEXT("[")), Key(EKeys::RightBracket, TEXT("]")),
				Key(EKeys::Backslash, TEXT("\\"), 1.5f),
				KeyAt(EKeys::Delete, TEXT("DEL"), FFF_KeyBindingLayoutMetrics::NavigationClusterX),
				Key(EKeys::End, TEXT("END")), Key(EKeys::PageDown, TEXT("PGD")),
				KeyAt(EKeys::NumPadSeven, TEXT("7"), FFF_KeyBindingLayoutMetrics::NumberPadClusterX),
				Key(EKeys::NumPadEight, TEXT("8")), Key(EKeys::NumPadNine, TEXT("9")),
				Key(EKeys::Add, TEXT("+"), 1.0f, 2.0f)
			},
			{
				KeyAt(EKeys::CapsLock, TEXT("Caps"), 0.f, 1.75f), Key(EKeys::A, TEXT("A")),
				Key(EKeys::S, TEXT("S")), Key(EKeys::D, TEXT("D")), Key(EKeys::F, TEXT("F")),
				Key(EKeys::G, TEXT("G")), Key(EKeys::H, TEXT("H")), Key(EKeys::J, TEXT("J")),
				Key(EKeys::K, TEXT("K")), Key(EKeys::L, TEXT("L")),
				Key(EKeys::Semicolon, TEXT(";")), Key(EKeys::Apostrophe, TEXT("'")),
				Key(EKeys::Enter, TEXT("Enter"), 2.25f),
				KeyAt(EKeys::NumPadFour, TEXT("4"), FFF_KeyBindingLayoutMetrics::NumberPadClusterX),
				Key(EKeys::NumPadFive, TEXT("5")),
				Key(EKeys::NumPadSix, TEXT("6"))
			},
			{
				KeyAt(EKeys::LeftShift, TEXT("Shift"), 0.f, 2.25f), Key(EKeys::Z, TEXT("Z")),
				Key(EKeys::X, TEXT("X")), Key(EKeys::C, TEXT("C")), Key(EKeys::V, TEXT("V")),
				Key(EKeys::B, TEXT("B")), Key(EKeys::N, TEXT("N")), Key(EKeys::M, TEXT("M")),
				Key(EKeys::Comma, TEXT(",")), Key(EKeys::Period, TEXT(".")), Key(EKeys::Slash, TEXT("/")),
				Key(EKeys::RightShift, TEXT("Shift"), 2.75f),
				KeyAt(EKeys::Up, TEXT("↑"),
					FFF_KeyBindingLayoutMetrics::NavigationClusterX + FFF_KeyBindingLayoutMetrics::KeyPitch),
				KeyAt(EKeys::NumPadOne, TEXT("1"), FFF_KeyBindingLayoutMetrics::NumberPadClusterX),
				Key(EKeys::NumPadTwo, TEXT("2")), Key(EKeys::NumPadThree, TEXT("3")),
				Key(EKeys::Enter, TEXT("Enter"), 1.0f, 2.0f)
			},
			{
				KeyAt(EKeys::LeftControl, TEXT("Ctrl"), 0.f, 1.25f),
				Key(EKeys::LeftCommand, TEXT("Win"), 1.25f),
				Key(EKeys::LeftAlt, TEXT("Alt"), 1.25f), Key(EKeys::SpaceBar, TEXT("Space"), 6.25f),
				Key(EKeys::RightAlt, TEXT("Alt"), 1.25f), Key(EKeys::RightCommand, TEXT("Win"), 1.25f),
				Key(EKeys::RightControl, TEXT("Ctrl"), 1.25f),
				KeyAt(EKeys::Left, TEXT("←"), FFF_KeyBindingLayoutMetrics::NavigationClusterX),
				Key(EKeys::Down, TEXT("↓")),
				Key(EKeys::Right, TEXT("→")),
				KeyAt(EKeys::NumPadZero, TEXT("0"), FFF_KeyBindingLayoutMetrics::NumberPadClusterX, 2.0f),
				Key(EKeys::Decimal, TEXT("."))
			}
		};
		return Rows;
	}

	FSlateBrush MakeRoundedBrush(
		const FLinearColor& Fill,
		const float Radius,
		const FLinearColor& Outline,
		const float OutlineWidth = 1.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius,
			FSlateColor(Outline),
			OutlineWidth);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(
		const bool bPrimary,
		const bool bReserved = false,
		const bool bFixedAction = false)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		const FFPauseOptionsLightTeal::FTokens Tokens = Palette
			? FFPauseOptionsLightTeal::Get(*Palette)
			: FFPauseOptionsLightTeal::FTokens {
				FLinearColor::Transparent, FLinearColor::White, FLinearColor::White,
				FLinearColor(0.92f, 0.94f, 0.93f), FLinearColor(0.90f, 0.92f, 0.91f),
				FLinearColor(0.91f, 0.94f, 0.92f), FLinearColor(0.85f, 0.85f, 0.85f),
				FLinearColor::Gray, FLinearColor::Gray, FLinearColor::Black,
				FLinearColor::Gray, FLinearColor::Gray, FLinearColor(0.32f, 0.49f, 0.42f),
				FLinearColor(0.19f, 0.34f, 0.28f), FLinearColor(0.70f, 0.78f, 0.73f),
				FLinearColor::White, FLinearColor::Red };
		const FLinearColor Edge = bReserved ? Tokens.TealDeep : (bFixedAction ? Tokens.TealDeep : Tokens.Line);
		const FLinearColor AccentEdge = Tokens.TealDeep;
		const FLinearColor Normal = bFixedAction
			? Tokens.PaperSunken
			: (bReserved
			? Tokens.TealDeep
			: (bPrimary ? Tokens.Teal : Tokens.PaperStrong));
		const FLinearColor Hovered = bFixedAction
			? Tokens.Hover
			: (bReserved
			? Tokens.Teal
			: (bPrimary
			? Tokens.TealDeep
			: Tokens.Hover));
		const FLinearColor Pressed = bFixedAction
			? Tokens.TealSoft
			: Tokens.TealDeep;
		const FLinearColor Disabled = Tokens.Disabled;

		FButtonStyle Style;
		Style.Normal = MakeRoundedBrush(Normal, 4.0f, bFixedAction ? AccentEdge : Edge);
		Style.Hovered = MakeRoundedBrush(Hovered, 4.0f, AccentEdge, 1.5f);
		Style.Pressed = MakeRoundedBrush(Pressed, 4.0f, AccentEdge, 1.5f);
		Style.Disabled = MakeRoundedBrush(Disabled, 4.0f, Edge);
		Style.NormalPadding = bReserved ? FMargin(4.0f, 4.0f) : FMargin(7.0f, 5.0f);
		Style.PressedPadding = bReserved
			? FMargin(4.0f, 5.0f, 4.0f, 3.0f)
			: FMargin(7.0f, 6.0f, 7.0f, 4.0f);
		return Style;
	}

	void StyleKeyBindingText(
		UTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const FName Typeface = TEXT("Medium"))
	{
		FSlateFontInfo Font = Text.GetFont();
		if (UFont* Pretendard = LoadObject<UFont>(nullptr, PretendardFontPath))
		{
			Font.FontObject = Pretendard;
			Font.TypefaceFontName = Typeface;
		}
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
	}

	FText KeyDisplayText(const FKey& KeyToDisplay)
	{
		return KeyToDisplay.IsValid()
			? KeyToDisplay.GetDisplayName(false)
			: LOCTEXT("UnassignedKey", "미지정");
	}

}

void UFF_KeyBindingKeyButton::Configure(
	const FKey InKey,
	const FText InLabel,
	const bool bInReserved,
	const bool bInFixedAction)
{
	ConfiguredKey = InKey;
	bReservedKey = bInReserved;
	bFixedActionKey = bInFixedAction;
	FButtonStyle KeyStyle = MakeButtonStyle(false, bReservedKey, bFixedActionKey);
	// Keycaps need more usable text area than the full-width action buttons that share the base style.
	KeyStyle.NormalPadding = FMargin(3.0f);
	KeyStyle.PressedPadding = FMargin(3.0f, 4.0f, 3.0f, 2.0f);
	SetStyle(KeyStyle);
	SetToolTipText(bFixedActionKey
		? LOCTEXT(
			"FixedBuildKeyTooltip",
			"건축 모드 / 시설 목록 전환에 고정된 키입니다. 클릭하면 현재 설정을 확인합니다.")
		: (bReservedKey
			? LOCTEXT("SystemOptionKeyTooltip", "현재 옵션 열기 / 닫기 키입니다. 클릭하면 연결된 동작을 확인합니다.")
			: FText::Format(LOCTEXT("InspectableKeyTooltip", "{0} 키의 현재 설정 확인 / 키 지정"), InLabel)));

	UScaleBox* LabelScale = Cast<UScaleBox>(GetContent());
	UTextBlock* Label = LabelScale ? Cast<UTextBlock>(LabelScale->GetContent()) : nullptr;
	if (!LabelScale)
	{
		LabelScale = NewObject<UScaleBox>(this);
		LabelScale->SetStretch(EStretch::ScaleToFit);
		LabelScale->SetStretchDirection(EStretchDirection::DownOnly);
		LabelScale->SetClipping(EWidgetClipping::ClipToBounds);
		SetContent(LabelScale);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(LabelScale->Slot))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	if (!Label)
	{
		Label = NewObject<UTextBlock>(LabelScale);
		LabelScale->SetContent(Label);
		if (UScaleBoxSlot* LabelSlot = Cast<UScaleBoxSlot>(Label->Slot))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Label->SetText(InLabel);
	Label->SetJustification(ETextJustify::Center);
	Label->SetAutoWrapText(false);
	Label->SetClipping(EWidgetClipping::ClipToBounds);
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
	Label->SetShadowOffset(bReservedKey ? FVector2D(0.0f, 1.0f) : FVector2D::ZeroVector);
	Label->SetShadowColorAndOpacity(
		bReservedKey ? FLinearColor(0.0f, 0.0f, 0.0f, 0.35f) : FLinearColor::Transparent);
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	StyleKeyBindingText(
		*Label,
		bReservedKey ? 14 : 12,
		bFixedActionKey
			? (Palette ? Palette->InGame.TealDeep : FLinearColor(0.2f, 0.4f, 0.3f))
			: (bReservedKey
			? (Palette ? Palette->InGame.White : FLinearColor::White)
			: (Palette ? Palette->InGame.Ink : FLinearColor::Black)),
		bReservedKey ? TEXT("Bold") : TEXT("SemiBold"));

	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UFF_KeyBindingKeyButton::HandleClicked()
{
	OnKeyChosen.Broadcast(ConfiguredKey);
}

void UFF_KeyBindingActionButton::Configure(
	const FName InMappingName,
	const FText InActionLabel,
	const FKey InCurrentKey,
	const bool bInReadOnly)
{
	MappingName = InMappingName;
	CurrentKey = InCurrentKey;
	bReadOnlyBinding = bInReadOnly;
	SetStyle(MakeButtonStyle(false, false, bReadOnlyBinding));
	SetToolTipText(bReadOnlyBinding
		? LOCTEXT("InspectFixedMappingTooltip", "고정 단축키입니다. 클릭하면 현재 설정을 확인합니다.")
		: FText::Format(
			LOCTEXT("ChooseMappingTooltip", "{0} 키 변경"),
			InActionLabel));

	UTextBlock* Label = Cast<UTextBlock>(GetContent());
	if (!Label)
	{
		Label = NewObject<UTextBlock>(this);
		SetContent(Label);
	}
	Label->SetText(bReadOnlyBinding
		? FText::Format(
			LOCTEXT("FixedActionAndKeyFormat", "{0}    [ {1} ] · 고정"),
			InActionLabel,
			KeyDisplayText(CurrentKey))
		: FText::Format(
			LOCTEXT("ActionAndKeyFormat", "{0}    [ {1} ]"),
			InActionLabel,
			KeyDisplayText(CurrentKey)));
	Label->SetJustification(ETextJustify::Left);
	Label->SetAutoWrapText(true);
	Label->SetClipping(EWidgetClipping::ClipToBounds);
	SetClipping(EWidgetClipping::ClipToBounds);
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	StyleKeyBindingText(
		*Label,
		15,
		bReadOnlyBinding
			? (Palette ? Palette->InGame.TealDeep : FLinearColor(0.2f, 0.4f, 0.3f))
			: (Palette ? Palette->InGame.Ink : FLinearColor::Black),
		TEXT("SemiBold"));

	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UFF_KeyBindingActionButton::HandleClicked()
{
	OnMappingChosen.Broadcast(MappingName);
}

UFF_KeyBindingPanel::UFF_KeyBindingPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

FKey UFF_KeyBindingPanel::GetFixedBuildModeKey()
{
	return EKeys::B;
}

bool UFF_KeyBindingPanel::IsFixedActionKey(const FKey& Key)
{
	return Key == GetFixedBuildModeKey();
}

FKey UFF_KeyBindingPanel::GetDefaultKeyForMapping(const FName MappingName)
{
	if (const FFF_KeyBindingDefinition* Definition = FFKeyBindingDefinitions::Find(MappingName))
	{
		return Definition->DefaultKey;
	}
	return EKeys::Invalid;
}

TSharedRef<SWidget> UFF_KeyBindingPanel::RebuildWidget()
{
	BuildFallbackWidgetTree();
	ResolveWidgetBindings();
	return Super::RebuildWidget();
}

void UFF_KeyBindingPanel::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveWidgetBindings();

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (WidgetTree)
	{
		// 전체 화면 옵션의 중앙 열 안에서 풀사이즈 키보드가 잘리지 않도록 한 화면 밀도로 맞춘다.
		// WBP 이름 계약을 사용하므로 디자이너 레이아웃과 런타임 입력 데이터는 계속 분리된다.
		if (USizeBox* KeyboardArtworkBox = Cast<USizeBox>(
			WidgetTree->FindWidget(TEXT("SizeBox_KeyboardArtwork"))))
		{
			KeyboardArtworkBox->SetHeightOverride(FFF_KeyBindingLayoutMetrics::StageHeight);
		}
		if (UTextBlock* Description = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Description"))))
		{
			Description->SetText(LOCTEXT(
				"PanelDescriptionRuntime",
				"키를 누르거나 키캡을 클릭하면 현재 동작을 확인합니다. 변경하려면 아래 조작 항목을 먼저 선택하세요."));
		}
	}
	if (Text_ResetAll)
	{
		Text_ResetAll->SetText(LOCTEXT("ResetAll", "전체 기본값 복원"));
		Text_ResetAll->SetJustification(ETextJustify::Center);
		StyleKeyBindingText(*Text_ResetAll, 14, Palette ? Palette->InGame.White : FLinearColor::White, TEXT("SemiBold"));
	}
	if (Button_ResetAll)
	{
		Button_ResetAll->SetStyle(MakeButtonStyle(true));
		Button_ResetAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetAllClicked);
	}
	if (Text_CaptureStatus)
	{
		StyleKeyBindingText(*Text_CaptureStatus, 14, Palette ? Palette->InGame.InkMuted : FLinearColor::Gray);
		Text_CaptureStatus->SetAutoWrapText(true);
	}

	InitializeUserSettings();
	RefreshBindings();

	if (Button_ResetAll)
	{
		Button_ResetAll->SetIsEnabled(UserSettings != nullptr);
	}
	if (UserSettings)
	{
		SetStatus(LOCTEXT(
			"InitialCaptureStatus",
			"키를 누르거나 키캡을 클릭하면 현재 설정을 확인할 수 있습니다. 변경하려면 아래 조작 항목을 선택하세요."));
	}
	else
	{
		SetStatus(LOCTEXT(
			"MissingUserSettings",
			"키 설정을 불러오지 못했습니다. Enhanced Input User Settings 활성화를 확인해 주세요."), true);
	}
}

void UFF_KeyBindingPanel::NativeDestruct()
{
	if (Button_ResetAll)
	{
		Button_ResetAll->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetAllClicked);
	}
	SetCaptureInProgress(false);
	SelectedMappingName = NAME_None;
	UserSettings = nullptr;
	Super::NativeDestruct();
}

FReply UFF_KeyBindingPanel::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && HandlePhysicalKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

bool UFF_KeyBindingPanel::HandlePhysicalKey(const FKey Key)
{
	if (!IsSupportedKeyboardKey(Key))
	{
		return false;
	}

	if (!SelectedMappingName.IsNone())
	{
		TryAssignKey(Key);
	}
	else
	{
		// When no row is capturing, the current system keys belong to the owning
		// Pause/Settings screen. Keycap clicks can still inspect them without closing.
		if (IsSystemOptionKey(Key))
		{
			return false;
		}
		InspectKey(Key);
	}
	SetKeyboardFocus();
	return true;
}

void UFF_KeyBindingPanel::CancelKeyCapture()
{
	if (SelectedMappingName.IsNone())
	{
		return;
	}

	SetCaptureInProgress(false);
	SelectedMappingName = NAME_None;
	RefreshBindings();
}

bool UFF_KeyBindingPanel::BuildFallbackWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}

	ResolveWidgetBindings();
	if (Image_KeyboardArtwork && VerticalBox_KeyboardRows && VerticalBox_BindingRows
		&& Text_CaptureStatus && Button_ResetAll && Text_ResetAll)
	{
		return true;
	}

	UVerticalBox* FallbackHost = nullptr;
	if (!WidgetTree->RootWidget)
	{
		FallbackHost = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("VerticalBox_KeyBindingRuntimeRoot"));
		WidgetTree->RootWidget = FallbackHost;
	}
	else if (UVerticalBox* ExistingRoot = Cast<UVerticalBox>(WidgetTree->RootWidget);
		ExistingRoot && ExistingRoot != VerticalBox_KeyboardRows && ExistingRoot != VerticalBox_BindingRows)
	{
		FallbackHost = ExistingRoot;
	}
	else
	{
		UWidget* ExistingRootWidget = WidgetTree->RootWidget;
		UVerticalBox* Wrapper = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("VerticalBox_KeyBindingRuntimeWrapper"));
		FallbackHost = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("VerticalBox_KeyBindingRuntimeFallback"));
		if (!Wrapper || !FallbackHost)
		{
			return false;
		}
		WidgetTree->RootWidget = Wrapper;
		Wrapper->AddChildToVerticalBox(ExistingRootWidget);
		Wrapper->AddChildToVerticalBox(FallbackHost);
	}

	if (!FallbackHost)
	{
		return false;
	}

	if (!Image_KeyboardArtwork)
	{
		Image_KeyboardArtwork = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, Image_KeyboardArtwork));
		if (Image_KeyboardArtwork)
		{
			Image_KeyboardArtwork->SetVisibility(ESlateVisibility::Collapsed);
			FallbackHost->AddChildToVerticalBox(Image_KeyboardArtwork);
		}
	}
	if (!VerticalBox_KeyboardRows)
	{
		VerticalBox_KeyboardRows = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, VerticalBox_KeyboardRows));
		if (VerticalBox_KeyboardRows)
		{
			FallbackHost->AddChildToVerticalBox(VerticalBox_KeyboardRows);
		}
	}
	if (!VerticalBox_BindingRows)
	{
		VerticalBox_BindingRows = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, VerticalBox_BindingRows));
		if (VerticalBox_BindingRows)
		{
			FallbackHost->AddChildToVerticalBox(VerticalBox_BindingRows);
		}
	}
	if (!Text_CaptureStatus)
	{
		Text_CaptureStatus = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, Text_CaptureStatus));
		if (Text_CaptureStatus)
		{
			if (UVerticalBoxSlot* StatusSlot = FallbackHost->AddChildToVerticalBox(Text_CaptureStatus))
			{
				StatusSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 8.0f));
			}
		}
	}
	if (!Button_ResetAll)
	{
		Button_ResetAll = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, Button_ResetAll));
		if (Button_ResetAll)
		{
			FallbackHost->AddChildToVerticalBox(Button_ResetAll);
		}
	}
	if (!Text_ResetAll)
	{
		Text_ResetAll = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			GET_MEMBER_NAME_CHECKED(ThisClass, Text_ResetAll));
		if (Button_ResetAll && Text_ResetAll)
		{
			Button_ResetAll->SetContent(Text_ResetAll);
		}
	}

	return VerticalBox_KeyboardRows && VerticalBox_BindingRows
		&& Text_CaptureStatus && Button_ResetAll && Text_ResetAll;
}

void UFF_KeyBindingPanel::ResolveWidgetBindings()
{
	if (!WidgetTree)
	{
		return;
	}
	if (UImage* Found = Cast<UImage>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, Image_KeyboardArtwork))))
	{
		Image_KeyboardArtwork = Found;
	}
	if (UVerticalBox* Found = Cast<UVerticalBox>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, VerticalBox_KeyboardRows))))
	{
		VerticalBox_KeyboardRows = Found;
	}
	if (UVerticalBox* Found = Cast<UVerticalBox>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, VerticalBox_BindingRows))))
	{
		VerticalBox_BindingRows = Found;
	}
	if (UTextBlock* Found = Cast<UTextBlock>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, Text_CaptureStatus))))
	{
		Text_CaptureStatus = Found;
	}
	if (UButton* Found = Cast<UButton>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, Button_ResetAll))))
	{
		Button_ResetAll = Found;
	}
	if (UTextBlock* Found = Cast<UTextBlock>(WidgetTree->FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, Text_ResetAll))))
	{
		Text_ResetAll = Found;
	}
}

void UFF_KeyBindingPanel::InitializeUserSettings()
{
	UserSettings = nullptr;
	if (UFF_KeyBindingRuntimeSubsystem* RuntimeSubsystem = ResolveRuntimeSubsystem())
	{
		RuntimeSubsystem->EnsureReady();
		UserSettings = RuntimeSubsystem->GetUserSettings();
		RuntimeSubsystem->BeginKeyBindingEdit();
	}
}

UFF_KeyBindingRuntimeSubsystem* UFF_KeyBindingPanel::ResolveRuntimeSubsystem() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer
		? LocalPlayer->GetSubsystem<UFF_KeyBindingRuntimeSubsystem>()
		: nullptr;
}

void UFF_KeyBindingPanel::SetCaptureInProgress(const bool bInCaptureInProgress)
{
	if (UFF_KeyBindingRuntimeSubsystem* RuntimeSubsystem = ResolveRuntimeSubsystem())
	{
		RuntimeSubsystem->SetCaptureInProgress(bInCaptureInProgress);
	}
}

bool UFF_KeyBindingPanel::IsSystemOptionKey(const FKey& Key) const
{
	if (!Key.IsValid())
	{
		return false;
	}
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		if (Definition.Context == EFF_KeyBindingContext::System
			&& FindCurrentKey(Definition.MappingName) == Key)
		{
			return true;
		}
	}
	return false;
}

void UFF_KeyBindingPanel::BuildKeyboardRows()
{
	if (!WidgetTree || !VerticalBox_KeyboardRows)
	{
		return;
	}
	VerticalBox_KeyboardRows->ClearChildren();

	USizeBox* KeyboardSurface = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SizeBox_KeyboardSurface"));
	UCanvasPanel* KeyboardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("CanvasPanel_KeyboardSurface"));
	if (!KeyboardSurface || !KeyboardCanvas)
	{
		return;
	}
	KeyboardSurface->SetWidthOverride(FFF_KeyBindingLayoutMetrics::KeyboardSurfaceWidth);
	KeyboardSurface->SetHeightOverride(FFF_KeyBindingLayoutMetrics::KeyboardSurfaceHeight);
	KeyboardSurface->SetClipping(EWidgetClipping::ClipToBounds);
	KeyboardCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	KeyboardSurface->SetContent(KeyboardCanvas);
	if (UVerticalBoxSlot* SurfaceSlot = VerticalBox_KeyboardRows->AddChildToVerticalBox(KeyboardSurface))
	{
		SurfaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		SurfaceSlot->SetHorizontalAlignment(HAlign_Center);
		SurfaceSlot->SetVerticalAlignment(VAlign_Center);
	}

	int32 KeyIndex = 0;
	const TArray<TArray<FAnsiKeyDefinition>>& KeyboardRows = GetAnsiKeyboardRows();
	for (int32 RowIndex = 0; RowIndex < KeyboardRows.Num(); ++RowIndex)
	{
		const TArray<FAnsiKeyDefinition>& RowDefinition = KeyboardRows[RowIndex];
		float CursorX = 0.0f;
		const float RowY = FFF_KeyBindingLayoutMetrics::GetRowY(RowIndex);
		for (const FAnsiKeyDefinition& Definition : RowDefinition)
		{
			if (Definition.StartX >= 0.0f)
			{
				CursorX = Definition.StartX;
			}

			USizeBox* KeySize = WidgetTree->ConstructWidget<USizeBox>();
			UFF_KeyBindingKeyButton* KeyButton =
				WidgetTree->ConstructWidget<UFF_KeyBindingKeyButton>(
					UFF_KeyBindingKeyButton::StaticClass(),
					FName(*FString::Printf(TEXT("KeyBindingKey_%03d"), KeyIndex++)));
			if (!KeySize || !KeyButton)
			{
				continue;
			}

			const float KeyWidth = FFF_KeyBindingLayoutMetrics::GetKeyWidth(Definition.WidthUnits);
			const float KeyHeight = FFF_KeyBindingLayoutMetrics::GetKeyHeight(Definition.HeightRows);
			KeySize->SetWidthOverride(KeyWidth);
			KeySize->SetHeightOverride(KeyHeight);
			KeySize->SetClipping(EWidgetClipping::ClipToBounds);
			KeyButton->Configure(
				Definition.Key,
				FText::FromString(Definition.Label),
				IsSystemOptionKey(Definition.Key),
				IsFixedActionKey(Definition.Key));
			KeyButton->OnKeyChosen.AddUniqueDynamic(this, &ThisClass::HandleKeyChosen);
			KeySize->SetContent(KeyButton);
			if (UCanvasPanelSlot* KeySlot = KeyboardCanvas->AddChildToCanvas(KeySize))
			{
				KeySlot->SetAnchors(FAnchors(0.0f, 0.0f));
				KeySlot->SetAlignment(FVector2D::ZeroVector);
				KeySlot->SetPosition(FVector2D(CursorX, RowY));
				KeySlot->SetSize(FVector2D(KeyWidth, KeyHeight));
				KeySlot->SetAutoSize(false);
			}

			CursorX += KeyWidth + FFF_KeyBindingLayoutMetrics::KeyGap;
		}
	}
}

void UFF_KeyBindingPanel::RefreshBindings()
{
	// System option keys are represented both in the action list and on the
	// keyboard diagram. Rebuild both so remaps/reset never leave stale keycaps.
	BuildKeyboardRows();
	BuildBindingRows();
}

void UFF_KeyBindingPanel::BuildBindingRows()
{
	if (!WidgetTree || !VerticalBox_BindingRows)
	{
		return;
	}
	VerticalBox_BindingRows->ClearChildren();
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const TArray<FFF_KeyBindingDefinition>& Definitions = FFKeyBindingDefinitions::GetAll();
	TOptional<EFF_KeyBindingContext> PreviousContext;
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FFF_KeyBindingDefinition& Definition = Definitions[Index];
		if (!PreviousContext.IsSet() || PreviousContext.GetValue() != Definition.Context)
		{
			PreviousContext = Definition.Context;
			UTextBlock* GroupTitle = WidgetTree->ConstructWidget<UTextBlock>();
			if (GroupTitle)
			{
				GroupTitle->SetText(FFKeyBindingDefinitions::GetContextLabel(Definition.Context));
				GroupTitle->SetAutoWrapText(true);
				GroupTitle->SetClipping(EWidgetClipping::ClipToBounds);
				StyleKeyBindingText(
					*GroupTitle,
					16,
					Palette ? Palette->InGame.TealDeep : FLinearColor::Black,
					TEXT("SemiBold"));
				if (UVerticalBoxSlot* TitleSlot = VerticalBox_BindingRows->AddChildToVerticalBox(GroupTitle))
				{
					TitleSlot->SetPadding(FMargin(2.0f, 14.0f, 2.0f, 5.0f));
				}
			}
		}

		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>();
		UFF_KeyBindingActionButton* ActionButton =
			WidgetTree->ConstructWidget<UFF_KeyBindingActionButton>(
				UFF_KeyBindingActionButton::StaticClass(),
				FName(*FString::Printf(TEXT("KeyBindingAction_%02d"), Index)));
		if (!RowSize || !ActionButton)
		{
			continue;
		}
		RowSize->SetMinDesiredHeight(44.0f);
		ActionButton->Configure(
			Definition.MappingName,
			Definition.ActionLabel,
			FindCurrentKey(Definition.MappingName));
		ActionButton->OnMappingChosen.AddUniqueDynamic(this, &ThisClass::HandleMappingChosen);
		ActionButton->SetStyle(MakeButtonStyle(SelectedMappingName == Definition.MappingName));
		ActionButton->SetBackgroundColor(FLinearColor::White);
		RowSize->SetContent(ActionButton);
		if (UVerticalBoxSlot* RowSlot = VerticalBox_BindingRows->AddChildToVerticalBox(RowSize))
		{
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	BuildFixedBindingRows();
}

void UFF_KeyBindingPanel::BuildFixedBindingRows()
{
	if (!WidgetTree || !VerticalBox_BindingRows)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	UTextBlock* GroupTitle = WidgetTree->ConstructWidget<UTextBlock>();
	if (GroupTitle)
	{
		GroupTitle->SetText(LOCTEXT("FixedInputGroup", "고정 단축키"));
		StyleKeyBindingText(
			*GroupTitle,
			16,
			Palette ? Palette->InGame.TealDeep : FLinearColor::Black,
			TEXT("SemiBold"));
		if (UVerticalBoxSlot* TitleSlot = VerticalBox_BindingRows->AddChildToVerticalBox(GroupTitle))
		{
			TitleSlot->SetPadding(FMargin(2.0f, 4.0f, 2.0f, 5.0f));
		}
	}

	USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>();
	UFF_KeyBindingActionButton* ActionButton =
		WidgetTree->ConstructWidget<UFF_KeyBindingActionButton>(
			UFF_KeyBindingActionButton::StaticClass(),
			TEXT("KeyBindingFixedAction_BuildMode"));
	if (!RowSize || !ActionButton)
	{
		return;
	}

	RowSize->SetMinDesiredHeight(44.0f);
	ActionButton->Configure(
		FixedBuildModeBindingName,
		LOCTEXT("BuildModeFixedAction", "건축 모드 / 시설 목록 전환"),
		GetFixedBuildModeKey(),
		true);
	ActionButton->OnMappingChosen.AddUniqueDynamic(this, &ThisClass::HandleMappingChosen);
	RowSize->SetContent(ActionButton);
	if (UVerticalBoxSlot* RowSlot = VerticalBox_BindingRows->AddChildToVerticalBox(RowSize))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UFF_KeyBindingPanel::HandleKeyChosen(const FKey Key)
{
	if (!SelectedMappingName.IsNone())
	{
		TryAssignKey(Key);
	}
	else
	{
		InspectKey(Key);
	}
	SetKeyboardFocus();
}

void UFF_KeyBindingPanel::HandleMappingChosen(const FName MappingName)
{
	if (MappingName == FixedBuildModeBindingName)
	{
		SetCaptureInProgress(false);
		SelectedMappingName = NAME_None;
		RefreshBindings();
		InspectKey(GetFixedBuildModeKey());
		SetKeyboardFocus();
		return;
	}
	if (SelectedMappingName == MappingName)
	{
		SetCaptureInProgress(false);
		SelectedMappingName = NAME_None;
		RefreshBindings();
		SetStatus(LOCTEXT("CaptureCancelled", "키 변경을 취소했습니다."));
		SetKeyboardFocus();
		return;
	}

	if (UFF_KeyBindingRuntimeSubsystem* RuntimeSubsystem = ResolveRuntimeSubsystem())
	{
		if (!RuntimeSubsystem->BeginKeyBindingEdit())
		{
			SetStatus(LOCTEXT(
				"EditTransactionUnavailable",
				"키 설정 편집을 시작할 수 없습니다. Enhanced Input User Settings 상태를 확인해 주세요."), true);
			return;
		}
	}

	SelectedMappingName = MappingName;
	SetCaptureInProgress(true);
	RefreshBindings();
	SetStatus(FText::Format(
		LOCTEXT("WaitingForKey", "‘{0}’에 지정할 키를 클릭하거나 실제 키보드에서 누르세요."),
		FindActionLabel(MappingName)));
	SetKeyboardFocus();
}

FText UFF_KeyBindingPanel::GetCurrentBindingSummary(const FKey Key) const
{
	if (!Key.IsValid())
	{
		return LOCTEXT("InvalidKeyInspection", "유효한 키를 선택해 주세요.");
	}

	TArray<FString> ActionLines;
	const bool bHasFixedBuildAction = IsFixedActionKey(Key);
	if (bHasFixedBuildAction)
	{
		ActionLines.Add(LOCTEXT(
			"FixedBuildActionSummary",
			"• 건축 모드 / 시설 목록 전환 (고정 · 변경 불가)").ToString());
	}

	int32 PlayerMappableActionCount = 0;
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		if (FindCurrentKey(Definition.MappingName) == Key)
		{
			ActionLines.Add(FString::Printf(TEXT("• %s"), *Definition.ActionLabel.ToString()));
			++PlayerMappableActionCount;
		}
	}

	if (ActionLines.IsEmpty())
	{
		return FText::Format(
			LOCTEXT("UnboundKeySummary", "{0} 키 · 현재 연결된 동작이 없습니다."),
			KeyDisplayText(Key));
	}

	FString Summary = FText::Format(
		LOCTEXT("KeySummaryHeader", "{0} 키 · 현재 설정"),
		KeyDisplayText(Key)).ToString();
	Summary += TEXT("\n");
	Summary += FString::Join(ActionLines, TEXT("\n"));
	if (bHasFixedBuildAction && PlayerMappableActionCount > 0)
	{
		Summary += TEXT("\n");
		Summary += LOCTEXT(
			"FixedBuildConflictWarning",
			"주의: B의 고정 건축 입력과 사용자 동작이 함께 실행될 수 있습니다. 사용자 동작을 다른 키로 변경하거나 기본값을 복원하세요.").ToString();
	}
	return FText::FromString(MoveTemp(Summary));
}

void UFF_KeyBindingPanel::InspectKey(const FKey& Key)
{
	SetStatus(GetCurrentBindingSummary(Key));
}

void UFF_KeyBindingPanel::TryAssignKey(const FKey& NewKey)
{
	if (SelectedMappingName.IsNone())
	{
		SetStatus(LOCTEXT("ChooseActionFirst", "먼저 변경할 동작을 선택해 주세요."), true);
		return;
	}
	if (!UserSettings)
	{
		SetStatus(LOCTEXT("SettingsUnavailable", "키 설정 서비스를 사용할 수 없습니다."), true);
		return;
	}
	if (IsFixedActionKey(NewKey))
	{
		SetStatus(LOCTEXT(
			"FixedBuildKeyRejected",
			"B 키는 건축 모드 / 시설 목록 전환에 고정되어 다른 동작에 지정할 수 없습니다."), true);
		return;
	}
	if (!IsSupportedKeyboardKey(NewKey))
	{
		SetStatus(LOCTEXT(
			"UnsupportedKeyRejected",
			"키보드의 디지털 키만 지정할 수 있습니다. 게임패드·마우스·축 입력은 지원하지 않습니다."), true);
		return;
	}

	const FPlayerKeyMapping* ExistingMapping = UserSettings->FindCurrentMappingForSlot(
		SelectedMappingName, EPlayerMappableKeySlot::First);
	if (!ExistingMapping)
	{
		SetStatus(FText::Format(
			LOCTEXT("MissingMappingRow", "‘{0}’의 Player Mappable 설정을 찾을 수 없습니다."),
			FindActionLabel(SelectedMappingName)), true);
		return;
	}
	const FKey ExistingKey = FindCurrentKey(SelectedMappingName);
	if (ExistingKey == NewKey)
	{
		const FText ExistingActionLabel = FindActionLabel(SelectedMappingName);
		SetCaptureInProgress(false);
		SelectedMappingName = NAME_None;
		RefreshBindings();
		SetStatus(FText::Format(
			LOCTEXT("AlreadyAssigned", "‘{0}’은(는) 이미 {1} 키를 사용합니다."),
			ExistingActionLabel,
			KeyDisplayText(NewKey)));
		return;
	}

	FName ConflictingMapping = NAME_None;
	if (FindConflictingMapping(NewKey, ConflictingMapping))
	{
		SetStatus(FText::Format(
			LOCTEXT("DuplicateKeyRejected", "{0} 키는 이미 ‘{1}’ 동작에 사용 중입니다."),
			KeyDisplayText(NewKey),
			FindActionLabel(ConflictingMapping)), true);
		return;
	}

	const FName AssignedMapping = SelectedMappingName;
	const FText AssignedActionLabel = FindActionLabel(AssignedMapping);
	FMapPlayerKeyArgs Args;
	Args.MappingName = AssignedMapping;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;
	Args.bCreateMatchingSlotIfNeeded = true;
	FGameplayTagContainer FailureReasons;
	UserSettings->MapPlayerKey(Args, FailureReasons);
	if (!FailureReasons.IsEmpty() || FindCurrentKey(AssignedMapping) != NewKey)
	{
		SetStatus(LOCTEXT(
			"MapPlayerKeyFailed",
			"키 변경에 실패했습니다. Input Action과 Mapping Context의 Player Mappable 이름을 확인해 주세요."), true);
		return;
	}

	PersistSettings();
	SetCaptureInProgress(false);
	SelectedMappingName = NAME_None;
	RefreshBindings();
	SetStatus(FText::Format(
		LOCTEXT("KeyAssigned", "‘{0}’ 키를 {1}(으)로 미리 변경했습니다. 하단 저장을 눌러 확정하세요."),
		AssignedActionLabel,
		KeyDisplayText(NewKey)));
}

bool UFF_KeyBindingPanel::IsSupportedKeyboardKey(const FKey& KeyToTest) const
{
	return KeyToTest.IsValid()
		&& KeyToTest != EKeys::AnyKey
		&& !KeyToTest.IsGamepadKey()
		&& !KeyToTest.IsMouseButton()
		&& !KeyToTest.IsAxis1D()
		&& !KeyToTest.IsAxis2D()
		&& !KeyToTest.IsAxis3D()
		&& !KeyToTest.IsAnalog()
		&& KeyToTest.IsBindableInBlueprints();
}

bool UFF_KeyBindingPanel::FindConflictingMapping(
	const FKey& KeyToTest,
	FName& OutMappingName) const
{
	OutMappingName = NAME_None;
	if (!UserSettings)
	{
		return false;
	}
	const FFF_KeyBindingDefinition* SelectedDefinition =
		FFKeyBindingDefinitions::Find(SelectedMappingName);
	if (!SelectedDefinition)
	{
		return false;
	}
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		const bool bConflictsByContext =
			SelectedDefinition->Context == EFF_KeyBindingContext::System
			|| Definition.Context == EFF_KeyBindingContext::System
			|| Definition.Context == SelectedDefinition->Context;
		if (Definition.MappingName != SelectedMappingName
			&& bConflictsByContext
			&& FindCurrentKey(Definition.MappingName) == KeyToTest)
		{
			OutMappingName = Definition.MappingName;
			return true;
		}
	}
	return false;
}

FKey UFF_KeyBindingPanel::FindCurrentKey(const FName MappingName) const
{
	if (UserSettings)
	{
		if (const FPlayerKeyMapping* Mapping = UserSettings->FindCurrentMappingForSlot(
			MappingName,
			EPlayerMappableKeySlot::First))
		{
			const FKey UserKey = Mapping->GetCurrentKey();
			if (UserKey.IsValid())
			{
				return UserKey;
			}
		}
	}

	// A missing/empty profile slot means "no override", not "unassigned".  The UI only
	// says 미지정 when neither a valid user key nor a project-authored default exists.
	return GetDefaultKeyForMapping(MappingName);
}

FText UFF_KeyBindingPanel::FindActionLabel(const FName MappingName) const
{
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		if (Definition.MappingName == MappingName)
		{
			return Definition.ActionLabel;
		}
	}
	return FText::FromName(MappingName);
}

void UFF_KeyBindingPanel::PersistSettings()
{
	if (UFF_KeyBindingRuntimeSubsystem* RuntimeSubsystem = ResolveRuntimeSubsystem())
	{
		RuntimeSubsystem->ApplyKeyBindingDraft();
	}
}

void UFF_KeyBindingPanel::HandleResetAllClicked()
{
	if (!UserSettings)
	{
		SetStatus(LOCTEXT("ResetUnavailable", "키 설정 서비스를 사용할 수 없어 초기화하지 못했습니다."), true);
		return;
	}
	if (UFF_KeyBindingRuntimeSubsystem* RuntimeSubsystem = ResolveRuntimeSubsystem())
	{
		if (!RuntimeSubsystem->BeginKeyBindingEdit())
		{
			SetStatus(LOCTEXT(
				"ResetTransactionUnavailable",
				"키 설정 편집을 시작할 수 없어 기본값을 복원하지 못했습니다."), true);
			return;
		}
	}

	bool bHadFailure = false;
	for (const FFF_KeyBindingDefinition& Definition : FFKeyBindingDefinitions::GetAll())
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Definition.MappingName;
		Args.Slot = EPlayerMappableKeySlot::First;
		FGameplayTagContainer FailureReasons;
		UserSettings->ResetAllPlayerKeysInRow(Args, FailureReasons);
		bHadFailure |= !FailureReasons.IsEmpty();
	}

	PersistSettings();
	SetCaptureInProgress(false);
	SelectedMappingName = NAME_None;
	RefreshBindings();
	SetStatus(
		bHadFailure
			? LOCTEXT("ResetPartiallyFailed", "일부 키를 초기화하지 못했습니다. Player Mappable 설정을 확인해 주세요.")
			: LOCTEXT("ResetSucceeded", "모든 키를 기본값으로 미리 복원했습니다. 하단 저장을 눌러 확정하세요."),
		bHadFailure);
}

void UFF_KeyBindingPanel::SetStatus(const FText& Message, const bool bIsError)
{
	if (!Text_CaptureStatus)
	{
		return;
	}
	Text_CaptureStatus->SetText(Message);
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	Text_CaptureStatus->SetColorAndOpacity(FSlateColor(
		bIsError
			? (Palette ? Palette->InGame.Danger : FLinearColor::Red)
			: (Palette ? Palette->InGame.InkMuted : FLinearColor::Gray)));
}

#undef LOCTEXT_NAMESPACE
