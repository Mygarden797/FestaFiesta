// 홍보 구매 전용 확인창. 공용 프런트엔드 확인창과 스타일은 수정하지 않는다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "AssetToolsModule.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Blueprint.h"
#include "Engine/Font.h"
#include "Factories/BlueprintFactory.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Frontend/FF_ConfirmDialog.h"
#include "UI/Widgets/FF_StatCard.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepConfirmSky"

namespace FFPrepConfirmSkyBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_PREP_CONFIRM_SKY");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/PrepHUD");
	const FString StyleFolder = Folder / TEXT("Styles");
	const FString DialogPackage = Folder / TEXT("WBP_FF_PrepConfirmSky");
	const FString RowPackage = Folder / TEXT("WBP_FF_PrepConfirmSkyRow");

	// 기존 에셋의 Designer/Class Defaults 편집은 재실행해도 보존한다.
	UBlueprint* EnsureStyle(const FString& Package, UClass& Parent,
		TFunctionRef<void(UObject&)> Configure)
	{
		if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *MakeObjectPath(Package)))
		{
			if (!Existing->GeneratedClass || !Existing->GeneratedClass->IsChildOf(&Parent))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 부모 클래스가 다르다."), LogPrefix, *Package);
				return nullptr;
			}
			return Existing;
		}
		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		Factory->ParentClass = &Parent;
		IAssetTools& Assets = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UBlueprint* BP = Cast<UBlueprint>(Assets.CreateAsset(FPackageName::GetShortName(Package),
			FPackageName::GetLongPackagePath(Package), UBlueprint::StaticClass(), Factory));
		if (!BP) { return nullptr; }
		FKismetEditorUtilities::CompileBlueprint(BP);
		if (!BP->GeneratedClass) { return nullptr; }
		UObject* Defaults = BP->GeneratedClass->GetDefaultObject();
		Defaults->Modify();
		Configure(*Defaults);
		FKismetEditorUtilities::CompileBlueprint(BP);
		return SaveAssetPackage(LogPrefix, *BP) ? BP : nullptr;
	}

	UBlueprint* EnsureTextStyle(const bool bPrimary)
	{
		return EnsureStyle(StyleFolder / (bPrimary ? TEXT("CTS_FF_PrepConfirmSkyLight") : TEXT("CTS_FF_PrepConfirmSkyInk")),
			*UCommonTextStyle::StaticClass(), [bPrimary](UObject& Object)
		{
			UCommonTextStyle& Style = *CastChecked<UCommonTextStyle>(&Object);
			if (UFont* Font = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage())))
			{
				Style.Font.FontObject = Font;
				Style.Font.TypefaceFontName = TEXT("SemiBold");
			}
			Style.Font.Size = 18;
			Style.Color = bPrimary ? Paper : Ink;
			Style.Margin = FMargin(0.f);
			Style.LineHeightPercentage = 1.f;
			Style.bUsesDropShadow = false;
		});
	}

	UBlueprint* EnsureButtonStyle(UClass* TextStyle, const bool bPrimary)
	{
		return EnsureStyle(StyleFolder / (bPrimary ? TEXT("CBS_FF_PrepConfirmSkyAccept") : TEXT("CBS_FF_PrepConfirmSkyCancel")),
			*UCommonButtonStyle::StaticClass(), [TextStyle, bPrimary](UObject& Object)
		{
			UCommonButtonStyle& Style = *CastChecked<UCommonButtonStyle>(&Object);
			Style.bSingleMaterial = false;
			Style.NormalBase = Brush(bPrimary ? Green : Paper, 7.f, 1.f, bPrimary ? Green : SkyLine);
			Style.NormalHovered = Brush(bPrimary ? Color(21, 122, 80) : SkyWash, 7.f, 2.f, Color(95, 172, 203));
			Style.NormalPressed = Brush(bPrimary ? Ink : Color(221, 239, 246), 7.f, 2.f, Ink);
			Style.SelectedBase = Style.NormalHovered;
			Style.SelectedHovered = Style.NormalHovered;
			Style.SelectedPressed = Style.NormalPressed;
			Style.Disabled = Brush(bPrimary ? Color(138, 164, 153) : SkyWash, 7.f, 1.f, SkyLine);
			Style.NormalTextStyle = TextStyle;
			Style.NormalHoveredTextStyle = TextStyle;
			Style.SelectedTextStyle = TextStyle;
			Style.SelectedHoveredTextStyle = TextStyle;
			Style.DisabledTextStyle = TextStyle;
			Style.ButtonPadding = Style.CustomPadding = FMargin(0.f);
			Style.MinWidth = Style.MinHeight = Style.MaxWidth = Style.MaxHeight = 0;
		});
	}

	UWidgetBlueprint* EnsureActionButton(const bool bPrimary)
	{
		const FString Package = Folder / (bPrimary ? TEXT("WBP_FF_PrepConfirmSkyAccept") : TEXT("WBP_FF_PrepConfirmSkyCancel"));
		if (UWidgetBlueprint* Existing = LoadExistingBlueprint(Package)) { return Existing; }
		UBlueprint* TextStyle = EnsureTextStyle(bPrimary);
		if (!TextStyle || !TextStyle->GeneratedClass) { return nullptr; }
		UBlueprint* ButtonStyle = EnsureButtonStyle(TextStyle->GeneratedClass, bPrimary);
		if (!ButtonStyle || !ButtonStyle->GeneratedClass) { return nullptr; }
		UWidgetBlueprint* BP = EnsureWidgetBlueprint(LogPrefix, Package, Folder, *UFF_CommonButtonBase::StaticClass(),
			[TextStyle, bPrimary](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
		{
			USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"), true);
			Size->SetHeightOverride(46.f);
			Tree.RootWidget = Size;
			UOverlay* Content = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Content"));
			Size->AddChild(Content);
			UCommonTextBlock* Label = MakeWidget<UCommonTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
			Label->SetStyle(TextStyle->GeneratedClass.Get());
			Label->SetText(bPrimary ? LOCTEXT("Purchase", "구매하기") : LOCTEXT("Cancel", "취소"));
			Label->SetJustification(ETextJustify::Center);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			UOverlaySlot* LabelSlot = Content->AddChildToOverlay(Label);
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(16.f, 0.f));
			UImage* Focus = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
			Focus->SetBrush(Brush(FLinearColor::Transparent, 7.f, 2.5f, bPrimary ? Color(148, 216, 230) : Color(77, 160, 190)));
			Focus->SetVisibility(ESlateVisibility::Hidden);
			UOverlaySlot* FocusSlot = Content->AddChildToOverlay(Focus);
			FocusSlot->SetHorizontalAlignment(HAlign_Fill);
			FocusSlot->SetVerticalAlignment(VAlign_Fill);
			return true;
		});
		if (!BP || !BP->GeneratedClass) { return nullptr; }
		UFF_CommonButtonBase* Defaults = CastChecked<UFF_CommonButtonBase>(BP->GeneratedClass->GetDefaultObject());
		Defaults->Modify();
		// 라벨은 전용 CTS에서만 칠해 CommonTextBlock의 재동기화와 경쟁하지 않는다.
		Defaults->ApplyGeneratedButtonDefaults(ButtonStyle->GeneratedClass.Get(), bPrimary ? Paper : Ink,
			bPrimary ? Paper : Ink, Muted, false, true, false);
		Defaults->ApplyGeneratedButtonContent(bPrimary ? LOCTEXT("Purchase", "구매하기") : LOCTEXT("Cancel", "취소"),
			FSlateBrush(), FVector2D::ZeroVector);
		FKismetEditorUtilities::CompileBlueprint(BP);
		return SaveAssetPackage(LogPrefix, *BP) ? BP : nullptr;
	}

	bool BuildRow(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		UBorder* Surface = MakeWidget<UBorder>(BP, Tree, TEXT("Border_RowSurface"));
		Surface->SetBrush(Brush(SkyWash, 0.f));
		Surface->SetPadding(FMargin(16.f, 10.f));
		Tree.RootWidget = Surface;
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Row"));
		Surface->AddChild(Content);
		UTextBlock* Label = Text(BP, Tree, TEXT("Text_Label"), LOCTEXT("RowLabel", "홍보 비용"), 16, Muted, true);
		Label->SetAutoWrapText(true);
		Column(*Content, *Label, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Label->Slot)->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Value = Text(BP, Tree, TEXT("Text_Value"), LOCTEXT("RowValue", "2,600 Gold"), 22, Ink, true);
		Value->SetJustification(ETextJustify::Right);
		Column(*Content, *Value);
		CastChecked<UHorizontalBoxSlot>(Value->Slot)->SetVerticalAlignment(VAlign_Center);
		// Text_Caption은 의도적으로 만들지 않는다. 공용 카드의 '보조 문구'가 표시되지 않는다.
		return true;
	}

	bool BuildDialog(UWidgetBlueprint& BP, UWidgetTree& Tree, UClass* CancelClass, UClass* AcceptClass)
	{
		UOverlay* Root = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Root"));
		Tree.RootWidget = Root;
		UBorder* Scrim = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Scrim"));
		FLinearColor ScrimColor = Color(17, 37, 45); ScrimColor.A = .32f;
		Scrim->SetBrush(Brush(ScrimColor)); Scrim->SetPadding(FMargin(0.f));
		UOverlaySlot* ScrimSlot = Root->AddChildToOverlay(Scrim);
		ScrimSlot->SetHorizontalAlignment(HAlign_Fill); ScrimSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Dialog"));
		Size->SetWidthOverride(640.f);
		UOverlaySlot* DialogSlot = Root->AddChildToOverlay(Size);
		DialogSlot->SetHorizontalAlignment(HAlign_Center); DialogSlot->SetVerticalAlignment(VAlign_Center);
		UBorder* Dialog = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Dialog"));
		Dialog->SetBrush(Brush(Paper, 10.f, 1.5f, SkyLine)); Dialog->SetPadding(FMargin(28.f, 24.f));
		Size->AddChild(Dialog);
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Dialog"));
		Dialog->AddChild(Content);
		UTextBlock* Title = Text(BP, Tree, TEXT("Text_Title"), LOCTEXT("Title", "홍보를 구매할까요?"), 26, Ink, true);
		Title->SetAutoWrapText(true); Row(*Content, *Title);
		Row(*Content, *Rule(BP, Tree, TEXT("Border_TitleRule")), FMargin(0.f, 16.f, 0.f, 16.f));
		UTextBlock* Body = Text(BP, Tree, TEXT("Text_Body"),
			LOCTEXT("Body", "축제당 한 번 구매하며, 3일 전체에 적용됩니다. 구매 후에는 변경할 수 없습니다."), 16, Muted, true);
		Body->SetAutoWrapText(true); Row(*Content, *Body);
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Rows"), true);
		Row(*Content, *Rows, FMargin(0.f, 20.f, 0.f, 24.f));
		UHorizontalBox* Actions = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Actions"));
		UFF_CommonButtonBase* Cancel = Cast<UFF_CommonButtonBase>(MakeWidgetOfClass(BP, Tree, TEXT("Button_Cancel"), CancelClass));
		UFF_CommonButtonBase* Accept = Cast<UFF_CommonButtonBase>(MakeWidgetOfClass(BP, Tree, TEXT("Button_Confirm"), AcceptClass));
		if (!Cancel || !Accept) { return false; }
		Cancel->ApplyGeneratedButtonContent(LOCTEXT("Cancel", "취소"), FSlateBrush(), FVector2D::ZeroVector);
		Accept->ApplyGeneratedButtonContent(LOCTEXT("Purchase", "구매하기"), FSlateBrush(), FVector2D::ZeroVector);
		Column(*Actions, *Cancel, FMargin(0.f, 0.f, 12.f, 0.f), true);
		Column(*Actions, *Accept, FMargin(0.f), true);
		Row(*Content, *Actions);
		return true;
	}

	void Apply()
	{
		if (LoadExistingBlueprint(DialogPackage))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_SKIP: 이미 생성된 홍보 확인창의 Designer 편집을 보존했다."), LogPrefix);
			return;
		}
		UWidgetBlueprint* Cancel = EnsureActionButton(false);
		UWidgetBlueprint* Accept = EnsureActionButton(true);
		UWidgetBlueprint* StatRow = EnsureWidgetBlueprint(LogPrefix, RowPackage, Folder, *UFF_StatCard::StaticClass(), BuildRow);
		if (!Cancel || !Accept || !StatRow || !Cancel->GeneratedClass || !Accept->GeneratedClass || !StatRow->GeneratedClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 확인창의 선행 에셋 생성 실패."), LogPrefix);
			return;
		}
		UWidgetBlueprint* Dialog = EnsureWidgetBlueprint(LogPrefix, DialogPackage, Folder, *UFF_ConfirmDialog::StaticClass(),
			[Cancel, Accept](UWidgetBlueprint& BP, UWidgetTree& Tree)
			{ return BuildDialog(BP, Tree, Cancel->GeneratedClass, Accept->GeneratedClass); });
		if (!Dialog || !Dialog->GeneratedClass) { return; }
		UObject* Defaults = Dialog->GeneratedClass->GetDefaultObject();
		FClassProperty* RowClass = FindFProperty<FClassProperty>(Defaults->GetClass(), TEXT("RowWidgetClass"));
		if (!RowClass)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: RowWidgetClass 속성이 없다."), LogPrefix);
			return;
		}
		Defaults->Modify();
		RowClass->SetObjectPropertyValue_InContainer(Defaults, StatRow->GeneratedClass);
		FKismetEditorUtilities::CompileBlueprint(Dialog);
		if (SaveAssetPackage(LogPrefix, *Dialog))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: 홍보 전용 밝은 확인창 생성 완료."), LogPrefix);
		}
	}

	FAutoConsoleCommand ApplyCommand(TEXT("FF.ApplyPrepConfirmSkyDesign"),
		TEXT("홍보 전용 밝은 구매 확인창을 최초 생성한다. 기존 Designer 편집은 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
