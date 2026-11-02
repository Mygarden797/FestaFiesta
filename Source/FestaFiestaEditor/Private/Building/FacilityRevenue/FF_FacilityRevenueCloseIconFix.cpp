// 매출 순위의 글자 X만 공통 닫기 아이콘으로 교체한다. 사용자 패널 치수는 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "WidgetBlueprintEditorUtils.h"

namespace FFFacilityRevenueCloseIconFix
{
void Apply()
{
    using namespace FFUIWbpBuilder;
    constexpr const TCHAR* Prefix = TEXT("FF_REVENUE_CLOSE_ICON");
    if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PIE_RUNNING"), Prefix); return; }
    UWidgetBlueprint* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityRevenueRankingPackage);
    if (!BP || !BP->WidgetTree) { return; }
    UWidgetTree& Tree = *BP->WidgetTree;
    UButton* Close = Cast<UButton>(Tree.FindWidget(TEXT("Button_Close")));
    if (!Close) { return; }
    if (Tree.FindWidget(TEXT("Image_Close")))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE"), Prefix);
        return;
    }
    UTextBlock* Old = Cast<UTextBlock>(Close->GetContent());
    if (!Old || Old->GetFName() != TEXT("Button_Close_Label") || BP->GetOutermost()->IsDirty())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_UNEXPECTED_OR_DIRTY"), Prefix);
        return;
    }
    if (!LoadObject<UTexture2D>(nullptr, TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Close_White_256.T_FF_LH_Icon_Close_White_256")))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_MISSING_TEXTURE"), Prefix);
        return;
    }
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/RevenueCloseIcon") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (!IFileManager::Get().MakeDirectory(*Backup, true) ||
        IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK) { return; }
    BP->Modify(); Tree.Modify(); Close->Modify();
    FWidgetBlueprintEditorUtils::DeleteWidgets(BP, TSet<UWidget*>{Old},
        FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
    USizeBox* Icon = FFPrepPanelSkyBuilder::CloseIcon(*BP, Tree);
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    UButtonSlot* Slot = CastChecked<UButtonSlot>(Close->AddChild(Icon));
    Slot->SetPadding(FMargin(0.f));
    Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
    FButtonStyle Style = Close->GetStyle();
    Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
    Close->SetStyle(Style);
    Close->SetToolTipText(NSLOCTEXT("FestaFiestaPrepSky", "Close", "닫기"));
    if (SaveNewBlueprint(Prefix, *BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED backup=%s"), Prefix, *Backup);
    }
}
FAutoConsoleCommand Command(TEXT("FF.FixFacilityRevenueCloseIcon"),
    TEXT("매출 순위 닫기 X를 중앙 공통 아이콘으로 변경한다. 높이·폭·목록·나머지 버튼은 보존한다."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
