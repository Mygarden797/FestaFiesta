// 사용자 편집을 보존하고 기존 소식 행 본문 아래에 공감 버튼만 덧붙인다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaSnsReactionBuilder"
namespace FFSnsReactionBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_SNS_REACTION");
	constexpr const TCHAR* VersionKey = TEXT("FF_SnsReactionVersion");
	constexpr const TCHAR* Package = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow");

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return; }
		UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
		if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(UFF_LiveSnsPostEntry::StaticClass()))
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 소식 행이 없습니다."), Prefix); return; }
		UWidgetTree& Tree = *BP->WidgetTree;
		auto* Texts = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Text")));
		auto* Body = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Body")));
		if (!Texts || !Body || Body->GetParent() != Texts)
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 본문 구조가 다릅니다. 기존 편집을 교체하지 않습니다."), Prefix); return; }
		if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			if (!Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Box_Reaction")))
				|| !Cast<UButton>(Tree.FindWidget(TEXT("Button_React")))
				|| !Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ReactLabel")))
				|| !Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ReactStatus"))))
			{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 적용 이력은 있지만 공감 바인딩이 누락됐습니다."), Prefix); return; }
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_ALREADY_APPLIED: 저장된 공감 행 편집을 유지합니다."), Prefix);
			return;
		}
		for (const FName Name : { FName(TEXT("Box_Reaction")), FName(TEXT("Button_React")),
			FName(TEXT("Text_ReactLabel")), FName(TEXT("Text_ReactStatus")) })
		{
			if (Tree.FindWidget(Name))
			{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 같은 이름의 위젯이 이미 있습니다: %s"), Prefix, *Name.ToString()); return; }
		}
		if (BP->GetOutermost()->IsDirty())
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 소식 행의 미저장 편집을 먼저 저장하세요."), Prefix); return; }
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/SnsReaction") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*Backup, true)
			|| IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 소식 행 백업 실패."), Prefix); return; }

		BP->Modify(); Tree.Modify(); Texts->Modify();
		auto* Reaction = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("Box_Reaction"), true);
		Reaction->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Label = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_ReactLabel"), true);
		FSlateFontInfo Font = Body->GetFont(); Font.Size = 12;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(Ink));
		Label->SetText(LOCTEXT("React", "♡ 공감"));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* ReactButton = MakeWidget<UButton>(*BP, Tree, TEXT("Button_React"), true);
		FButtonStyle Style = ReactButton->GetStyle();
		Style.Normal = Brush(FLinearColor::Transparent, 5.f, 1.f, SkyLine);
		Style.Hovered = Brush(SkyWash, 5.f, 1.f, Green);
		Style.Pressed = Brush(Color(226, 243, 233), 5.f, 1.f, Green);
		Style.Disabled = Brush(Color(235, 245, 239), 5.f, 1.f, Color(178, 216, 196));
		Style.NormalPadding = Style.PressedPadding = FMargin(8.f, 4.f);
		ReactButton->SetStyle(Style);
		ReactButton->SetClickMethod(EButtonClickMethod::PreciseClick);
		ReactButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		ReactButton->SetPressMethod(EButtonPressMethod::DownAndUp);
		auto* ButtonSlot = CastChecked<UButtonSlot>(ReactButton->AddChild(Label));
		ButtonSlot->SetPadding(FMargin(0.f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center); ButtonSlot->SetVerticalAlignment(VAlign_Center);
		auto* ReactSlot = Reaction->AddChildToHorizontalBox(ReactButton);
		ReactSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ReactSlot->SetVerticalAlignment(VAlign_Center);

		auto* Status = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_ReactStatus"), true);
		Font.Size = 11;
		Status->SetFont(Font); Status->SetColorAndOpacity(FSlateColor(Muted));
		Status->SetText(FText::GetEmpty());
		Status->SetAutoWrapText(true);
		Status->SetVisibility(ESlateVisibility::Hidden);
		auto* StatusSlot = Reaction->AddChildToHorizontalBox(Status);
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetPadding(FMargin(9.f, 0.f, 0.f, 0.f));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		auto* Slot = Texts->AddChildToVerticalBox(Reaction);
		Slot->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		BP->GetOutermost()->GetMetaData().SetValue(BP, VersionKey, TEXT("1"));
		if (!SaveNewBlueprint(Prefix, *BP)) { return; }
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 기존 본문 아래에 공감 행을 추가했습니다. 백업: %s"), Prefix, *Backup);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplySnsReactionRow"),
		TEXT("기존 소식 행에 공감 버튼과 실제 결과 표시만 추가합니다. 기존 위젯 편집은 보존합니다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
