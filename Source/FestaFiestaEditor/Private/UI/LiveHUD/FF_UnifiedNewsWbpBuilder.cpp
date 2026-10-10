// 기존 SNS 도크의 트리와 읽기 동작을 보존하면서 운영 알림을 같은 소식 목록에 연결한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUnifiedNews"
namespace FFUnifiedNewsBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_UNIFIED_NEWS");
	constexpr const TCHAR* VersionKey = TEXT("FF_UnifiedNewsVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD");
	bool HasVersion(UWidgetBlueprint& BP) { return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1"); }
	bool Finish(UWidgetBlueprint& BP)
	{
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return SaveAssetPackage(Prefix, BP);
	}
	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return; }
		auto* Panel = LoadExistingBlueprint(Folder / TEXT("Panels/WBP_FF_LiveSnsFlyout"));
		auto* Row = LoadExistingBlueprint(Folder / TEXT("Panels/WBP_FF_LiveSnsFlyoutPostRow"));
		auto* Dock = LoadExistingBlueprint(Folder / TEXT("WBP_FF_LiveUtilityDock"));
		if (!Panel || !Row || !Dock || !Panel->WidgetTree || !Row->WidgetTree || !Dock->WidgetTree)
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 소식 도크 원본이 없습니다."), Prefix); return; }
		for (const FName Name : { FName(TEXT("Text_PanelTitle")), FName(TEXT("Text_Subtitle")), FName(TEXT("Text_ReadPositionNote")), FName(TEXT("Text_FilterAll")), FName(TEXT("Text_FilterPositive")), FName(TEXT("Text_FilterNegative")) })
		{
			if (!Cast<UTextBlock>(Panel->WidgetTree->FindWidget(Name)))
			{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 글자 바인딩 누락: %s"), Prefix, *Name.ToString()); return; }
		}
		for (const FName Name : { FName(TEXT("Button_FilterAll")), FName(TEXT("Button_FilterPositive")), FName(TEXT("Button_FilterNegative")), FName(TEXT("Button_Close")) })
		{
			if (!Cast<UButton>(Panel->WidgetTree->FindWidget(Name)))
			{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 버튼 누락: %s"), Prefix, *Name.ToString()); return; }
		}
		auto* Mood = Cast<UTextBlock>(Row->WidgetTree->FindWidget(TEXT("Text_Sentiment")));
		auto* Avatar = Cast<UImage>(Row->WidgetTree->FindWidget(TEXT("Image_Avatar")));
		auto* AvatarBorder = Cast<UBorder>(Row->WidgetTree->FindWidget(TEXT("Border_Avatar")));
		if (!Mood || !Avatar || !AvatarBorder || (!HasVersion(*Row) && !Cast<UHorizontalBox>(Mood->GetParent())))
		{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 글 행 구조가 다릅니다. 교체하지 않습니다."), Prefix); return; }
		auto* SocialIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/AudienceGroups/T_FF_LH_Icon_GroupSolo_White_256.T_FF_LH_Icon_GroupSolo_White_256"));
		auto* WarningIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Status/T_FF_LH_Icon_StatusWarning_White_256.T_FF_LH_Icon_StatusWarning_White_256"));
		if (!SocialIcon || !WarningIcon) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 소식 아이콘이 없습니다."), Prefix); return; }
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/UnifiedNews") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		for (UWidgetBlueprint* BP : { Panel, Row, Dock })
		{
			if (HasVersion(*BP)) continue;
			if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 편집을 먼저 저장하세요: %s"), Prefix, *BP->GetName()); return; }
			const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
			{ UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 백업 실패: %s"), Prefix, *File); return; }
		}
		if (!HasVersion(*Row))
		{
			for (UWidget* Variable : { static_cast<UWidget*>(Avatar), static_cast<UWidget*>(AvatarBorder) })
			{ Variable->bIsVariable = true; Row->OnVariableAdded(Variable->GetFName()); }
			// 기존 반응 문구와 슬롯은 살리고 둥근 유형 배지만 감싼다.
			auto* Meta = CastChecked<UHorizontalBox>(Mood->GetParent());
			const int32 Index = Meta->GetChildIndex(Mood);
			const auto* OldSlot = CastChecked<UHorizontalBoxSlot>(Mood->Slot);
			const FMargin Padding = OldSlot->GetPadding();
			const FSlateChildSize Size = OldSlot->GetSize();
			Mood->RemoveFromParent();
			auto* Badge = MakeWidget<UBorder>(*Row, *Row->WidgetTree, TEXT("Border_Sentiment"), true);
			Badge->SetBrush(Brush(FLinearColor::White, 10.f));
			Badge->SetPadding(FMargin(7.f, 3.f));
			Badge->SetContent(Mood);
			Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* NewSlot = Meta->AddChildToHorizontalBox(Badge);
			NewSlot->SetPadding(Padding); NewSlot->SetSize(Size); NewSlot->SetVerticalAlignment(VAlign_Center);
			Meta->ShiftChild(Index, Badge);
			if (!SaveNewBlueprint(Prefix, *Row)) return;
			CastChecked<UFF_LiveSnsPostEntry>(Row->GeneratedClass->GetDefaultObject())->ConfigureNewsRowDefaults(SocialIcon, WarningIcon);
			if (!Finish(*Row)) return;
		}
		if (!HasVersion(*Panel))
		{
			const auto Set = [Panel](FName Name, FText Text) { CastChecked<UTextBlock>(Panel->WidgetTree->FindWidget(Name))->SetText(Text); };
			Set(TEXT("Text_PanelTitle"), LOCTEXT("Title", "소식"));
			Set(TEXT("Text_Subtitle"), LOCTEXT("Subtitle", "관객 이야기와 운영 알림"));
			Set(TEXT("Text_ReadPositionNote"), LOCTEXT("ReadPosition", "새 소식이 와도 읽던 위치는 유지됩니다."));
			Set(TEXT("Text_FilterPositive"), LOCTEXT("Social", "SNS"));
			Set(TEXT("Text_FilterNegative"), LOCTEXT("Operation", "운영 알림"));
			for (const FName Name : { FName(TEXT("Button_FilterAll")), FName(TEXT("Button_FilterPositive")), FName(TEXT("Button_FilterNegative")) })
			{
				auto* Button = CastChecked<UButton>(Panel->WidgetTree->FindWidget(Name));
				FButtonStyle Style = Button->GetStyle();
				Style.Normal = Brush(FLinearColor::White, 5.f, 1.f, Color(174, 214, 232));
				Style.Hovered = Brush(Color(234, 245, 250), 5.f, 1.f, Color(110, 166, 184));
				Style.Pressed = Brush(Color(214, 237, 245), 5.f, 1.f, Color(65, 139, 163));
				Button->SetStyle(Style);
			}
			CastChecked<UButton>(Panel->WidgetTree->FindWidget(TEXT("Button_Close")))->SetToolTipText(LOCTEXT("Close", "소식 닫기"));
			if (!SaveNewBlueprint(Prefix, *Panel)) return;
			CastChecked<UFF_LivePanelSNS>(Panel->GeneratedClass->GetDefaultObject())->ConfigureUnifiedNewsDefaults();
			if (!Finish(*Panel)) return;
		}
		if (!HasVersion(*Dock))
		{
			if (auto* Label = Cast<UTextBlock>(Dock->WidgetTree->FindWidget(TEXT("Text_SNSLabel")))) { Label->SetText(LOCTEXT("Title", "소식")); }
			if (auto* Button = Cast<UButton>(Dock->WidgetTree->FindWidget(TEXT("Button_SNS")))) { Button->SetToolTipText(LOCTEXT("NewsTooltip", "소식 · 관객 이야기와 운영 알림")); }
			if (!SaveNewBlueprint(Prefix, *Dock) || !Finish(*Dock)) return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 기존 SNS 도크에 전체/SNS/운영 알림 필터와 유형 배지를 적용했습니다."), Prefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyUnifiedNews"), TEXT("기존 SNS 도크의 사용자 편집을 보존하며 통합 소식 디자인을 한 번 적용한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
