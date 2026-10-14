// 라이브 스태프만 간소화한다. 공용 셋업 원본은 복제에만 사용하고 저장하지 않는다.
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"
#include "UI/Staff/FF_StaffManagementWidget.h"
#include "UI/Staff/FF_StaffPromotionRowWidget.h"
#include "UI/Staff/FF_StaffTrainingWidget.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace FFLiveStaffCompactBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_LIVE_STAFF_COMPACT");
constexpr const TCHAR* Version = TEXT("FF.LiveStaffCompact.Version");
const FString Folder = TEXT("/Game/FestaFiesta/UI/Staff");
const FString OriginalManagement = Folder / TEXT("WBP_FF_StaffManagement");
const FString OriginalTraining = Folder / TEXT("WBP_FF_StaffTraining");
const FString OriginalPromotion = Folder / TEXT("WBP_FF_StaffPromotionRow");
const FString ManagementPackage = Folder / TEXT("WBP_FF_LiveStaffManagement");
const FString TrainingPackage = Folder / TEXT("WBP_FF_LiveStaffTraining");
const FString PromotionPackage = Folder / TEXT("WBP_FF_LiveStaffPromotionRow");
const FString LivePackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff");

template<typename T> T* Get(UWidgetBlueprint& BP, const TCHAR* Name)
{
    return Cast<T>(BP.WidgetTree->FindWidget(Name));
}
bool Applied(UWidgetBlueprint& BP)
{
    return BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1");
}
bool Valid(UWidgetBlueprint* BP, UClass* Parent)
{
    return BP && BP->WidgetTree && BP->GeneratedClass && BP->ParentClass
        && BP->ParentClass->IsChildOf(Parent) && !BP->GetOutermost()->IsDirty();
}
template<typename T> bool Require(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names)
{
    for (const TCHAR* Name : Names)
    {
        if (!Get<T>(BP, Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_MISSING %s.%s"), Prefix, *BP.GetName(), Name);
            return false;
        }
    }
    return true;
}
bool CheckManagement(UWidgetBlueprint& BP)
{
    return Require<UVerticalBox>(BP, {TEXT("VerticalBox_Management"), TEXT("Box_Facilities")})
        && Require<UHorizontalBox>(BP, {TEXT("HorizontalBox_ManagementIntro"), TEXT("HorizontalBox_ManagementMetrics"),
            TEXT("HorizontalBox_ManagementColumns"), TEXT("HorizontalBox_FacilitiesHeader"), TEXT("HorizontalBox_Incident")})
        && Require<UBorder>(BP, {TEXT("Border_Phase"), TEXT("Management_Total_Card"), TEXT("Management_Second_Card"),
            TEXT("Management_Third_Card"), TEXT("Management_Fourth_Card"), TEXT("Group_Incident"), TEXT("Border_AssignmentGuide")})
        && Require<UTextBlock>(BP, {TEXT("Text_Phase"), TEXT("Text_Subtitle"), TEXT("Text_TotalStaff"), TEXT("Text_StaffBreakdown"),
            TEXT("Text_SecondLabel"), TEXT("Text_SecondValue"), TEXT("Text_SecondNote"), TEXT("Text_ThirdLabel"),
            TEXT("Text_ThirdValue"), TEXT("Text_ThirdNote"), TEXT("Text_FourthLabel"), TEXT("Text_FourthValue"),
            TEXT("Text_FourthNote"), TEXT("Text_FacilitiesTitle"), TEXT("Text_FacilitiesEmpty"), TEXT("Text_AssignmentGuide"),
            TEXT("Text_AssignmentResult"), TEXT("Text_IncidentTitle"), TEXT("Text_IncidentSummary"), TEXT("Text_ManagementNote")})
        && Require<UButton>(BP, {TEXT("Button_FillVacancies"), TEXT("Button_ViewIncident")})
        && Require<UFF_StaffTrainingWidget>(BP, {TEXT("Widget_StaffTraining")});
}
bool CheckTraining(UWidgetBlueprint& BP)
{
    return Require<UVerticalBox>(BP, {TEXT("Management_TrainingRoot"), TEXT("Box_StaffRoster")})
        && Require<UHorizontalBox>(BP, {TEXT("Management_TrainingTop"), TEXT("HorizontalBox_RosterHeader")})
        && Require<USizeBox>(BP, {TEXT("Management_Size_LevelHeader"), TEXT("Management_Size_PromotionHeader"), TEXT("SizeBox_StaffRoster")})
        && Require<UBorder>(BP, {TEXT("Border_RosterHeader"), TEXT("Border_PromotionResult")})
        && Require<UButton>(BP, {TEXT("Button_AllStaff"), TEXT("Button_UnassignedStaff")})
        && Require<UTextBlock>(BP, {TEXT("Text_TrainingTitle"), TEXT("Text_RosterNote"), TEXT("Text_RosterEmpty"),
            TEXT("Text_PromotionResult"), TEXT("Text_NameHeader"), TEXT("Text_KindHeader"), TEXT("Text_LevelHeader"),
            TEXT("Text_PromotionHeader"), TEXT("Management_Text_AssignmentHeader"), TEXT("Text_AllStaffFilter"), TEXT("Text_UnassignedStaffFilter")});
}
bool CheckPromotion(UWidgetBlueprint& BP)
{
    return Require<UHorizontalBox>(BP, {TEXT("HorizontalBox_StaffRow")})
        && Require<UVerticalBox>(BP, {TEXT("Management_StaffIdentity"), TEXT("Management_StaffAssignment"), TEXT("VerticalBox_PromotionAction")})
        && Require<USizeBox>(BP, {TEXT("Management_Size_Level"), TEXT("Management_Size_Promotion"), TEXT("SizeBox_StaffRow"), TEXT("SizeBox_Promote")})
        && Require<UButton>(BP, {TEXT("Button_Promote")})
        && Require<UTextBlock>(BP, {TEXT("Text_StaffName"), TEXT("Text_StaffKind"), TEXT("Text_StaffLevel"),
            TEXT("Text_AssignmentFacility"), TEXT("Text_AssignmentStatus"), TEXT("Text_Promote"), TEXT("Text_PromotionStatus")});
}
UTextBlock* LabelLike(UWidgetBlueprint& BP, const TCHAR* Name, const TCHAR* Caption, UTextBlock& Source, bool Variable = false)
{
    auto* Result = MakeWidget<UTextBlock>(BP, *BP.WidgetTree, Name, Variable);
    Result->SetFont(Source.GetFont());
    Result->SetColorAndOpacity(Source.GetColorAndOpacity());
    Result->SetText(FText::FromString(Caption));
    Result->SetAutoWrapText(false);
    Result->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Result->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Result;
}
void H(UHorizontalBox& Parent, UWidget& Child, int32 Index, float Weight, FMargin Padding)
{
    Child.RemoveFromParent();
    auto* Slot = Parent.AddChildToHorizontalBox(&Child);
    FSlateChildSize Width(Weight > 0.f ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
    Width.Value = Weight;
    Slot->SetSize(Width); Slot->SetPadding(Padding); Slot->SetVerticalAlignment(VAlign_Center);
    Parent.ShiftChild(Index, &Child);
}
USizeBox* Wrap(UWidgetBlueprint& BP, const TCHAR* Name, UWidget& Child, float Width)
{
    auto* Size = MakeWidget<USizeBox>(BP, *BP.WidgetTree, Name);
    Size->SetWidthOverride(Width); Child.RemoveFromParent(); Size->AddChild(&Child); return Size;
}
void VPadding(UWidget* Widget, FMargin Padding)
{
    if (auto* Slot = Cast<UVerticalBoxSlot>(Widget->Slot)) Slot->SetPadding(Padding);
}
// UE 5.7 PanelWidget.cpp의 ReplaceChild는 기존 슬롯을 유지한다.
// 이름은 그대로 남겨 BindWidget 및 애니메이션 참조를 유지하고, 교체 전 객체만 transient로 옮긴다.
bool ReplaceEmbedded(UWidgetBlueprint& BP, const TCHAR* Name, UClass* NewClass)
{
    UWidget* Old = BP.WidgetTree->FindWidget(Name);
    UPanelWidget* Parent = Old ? Old->GetParent() : nullptr;
    if (!Parent || !NewClass) return false;
    const FName Temporary = MakeUniqueObjectName(BP.WidgetTree, NewClass, TEXT("Compact_Replacement"));
    auto* Replacement = BP.WidgetTree->ConstructWidget<UUserWidget>(NewClass, Temporary);
    if (!Replacement) return false;
    Replacement->bIsVariable = Old->bIsVariable;
    Replacement->SetVisibility(Old->GetVisibility());
    Replacement->SetIsEnabled(Old->GetIsEnabled());
    Replacement->SetRenderOpacity(Old->GetRenderOpacity());
    if (!Parent->ReplaceChild(Old, Replacement)) return false;
    const FName Trash = MakeUniqueObjectName(GetTransientPackage(), Old->GetClass(), TEXT("TRASH_LiveStaffCompact"));
    if (!Old->Rename(*Trash.ToString(), GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional)) return false;
    return Replacement->Rename(Name, BP.WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
}
void MigratePromotion(UWidgetBlueprint& BP)
{
    auto* Row = Get<UHorizontalBox>(BP, TEXT("HorizontalBox_StaffRow"));
    Get<UTextBlock>(BP, TEXT("Text_StaffKind"))->SetVisibility(ESlateVisibility::Collapsed);
    Get<USizeBox>(BP, TEXT("SizeBox_StaffRow"))->ClearMinDesiredHeight();
    Get<USizeBox>(BP, TEXT("SizeBox_StaffRow"))->SetHeightOverride(56.f);
    Get<USizeBox>(BP, TEXT("SizeBox_Promote"))->ClearMinDesiredHeight();
    Get<USizeBox>(BP, TEXT("SizeBox_Promote"))->SetHeightOverride(32.f);
    auto* Level = Get<USizeBox>(BP, TEXT("Management_Size_Level")); Level->SetWidthOverride(60.f);
    auto* Promotion = Get<USizeBox>(BP, TEXT("Management_Size_Promotion")); Promotion->SetWidthOverride(110.f);
    auto* Status = Get<UTextBlock>(BP, TEXT("Text_AssignmentStatus"));
    Status->SetAutoWrapText(false); Status->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    auto* StatusSize = Wrap(BP, TEXT("Compact_Size_Status"), *Status, 82.f);
    H(*Row, *Get<UVerticalBox>(BP, TEXT("Management_StaffIdentity")), 0, .42f, FMargin(10, 6, 8, 6));
    H(*Row, *Level, 1, 0, FMargin(0, 6, 8, 6));
    H(*Row, *Get<UVerticalBox>(BP, TEXT("Management_StaffAssignment")), 2, .58f, FMargin(0, 6, 8, 6));
    H(*Row, *StatusSize, 3, 0, FMargin(0, 6, 8, 6));
    H(*Row, *Promotion, 4, 0, FMargin(0, 6, 10, 6));
}
void MigrateTraining(UWidgetBlueprint& BP)
{
    auto* Top = Get<UHorizontalBox>(BP, TEXT("Management_TrainingTop"));
    auto* Title = Get<UTextBlock>(BP, TEXT("Text_TrainingTitle"));
    H(*Top, *Title, 0, 0, FMargin(0, 0, 8, 0));
    auto* Help = LabelLike(BP, TEXT("Text_TrainingHelp"), TEXT("ⓘ"), *Get<UTextBlock>(BP, TEXT("Text_LevelHeader")), true);
    Help->SetVisibility(ESlateVisibility::Visible);
    Help->SetToolTipText(FText::FromString(TEXT("숙련도가 높을수록 현장 처리가 빨라집니다. 승급 효과는 다음 출동부터 적용됩니다.")));
    H(*Top, *Help, 1, 1.f, FMargin(0, 0, 8, 0));
    VPadding(Top, FMargin(0, 0, 0, 10));
    auto* Row = Get<UHorizontalBox>(BP, TEXT("HorizontalBox_RosterHeader"));
    auto* Assignment = Get<UTextBlock>(BP, TEXT("Management_Text_AssignmentHeader"));
    Assignment->SetText(FText::FromString(TEXT("배정 시설")));
    auto* Status = LabelLike(BP, TEXT("Compact_Text_StatusHeader"), TEXT("상태"), *Assignment);
    auto* StatusSize = Wrap(BP, TEXT("Compact_Size_StatusHeader"), *Status, 82.f);
    auto* Level = Get<USizeBox>(BP, TEXT("Management_Size_LevelHeader")); Level->SetWidthOverride(60.f);
    auto* Promotion = Get<USizeBox>(BP, TEXT("Management_Size_PromotionHeader")); Promotion->SetWidthOverride(110.f);
    H(*Row, *Get<UTextBlock>(BP, TEXT("Text_NameHeader")), 0, .42f, FMargin(10, 7, 8, 7));
    H(*Row, *Level, 1, 0, FMargin(0, 7, 8, 7));
    H(*Row, *Assignment, 2, .58f, FMargin(0, 7, 8, 7));
    H(*Row, *StatusSize, 3, 0, FMargin(0, 7, 8, 7));
    H(*Row, *Promotion, 4, 0, FMargin(0, 7, 10, 7));
    Get<UTextBlock>(BP, TEXT("Text_RosterNote"))->SetVisibility(ESlateVisibility::Collapsed);
    auto* Roster = Get<USizeBox>(BP, TEXT("SizeBox_StaffRoster"));
    Roster->ClearHeightOverride(); Roster->ClearMaxDesiredHeight(); Roster->SetMinDesiredHeight(310.f);
    VPadding(Get<UBorder>(BP, TEXT("Border_RosterHeader")), FMargin(0, 0, 0, 6));
}
bool MigrateManagement(UWidgetBlueprint& BP, UClass* TrainingClass)
{
    for (const TCHAR* Name : {TEXT("Text_Subtitle"), TEXT("Text_StaffBreakdown"), TEXT("Text_SecondNote"),
        TEXT("Text_ThirdNote"), TEXT("Text_FourthNote"), TEXT("Text_ManagementNote")})
    {
        Get<UTextBlock>(BP, Name)->SetVisibility(ESlateVisibility::Collapsed);
    }
    VPadding(Get<UHorizontalBox>(BP, TEXT("HorizontalBox_ManagementIntro")), FMargin(0, 0, 0, 8));
    VPadding(Get<UHorizontalBox>(BP, TEXT("HorizontalBox_ManagementMetrics")), FMargin(0, 0, 0, 14));
    for (const TCHAR* Name : {TEXT("Management_Total_Card"), TEXT("Management_Second_Card"), TEXT("Management_Third_Card"), TEXT("Management_Fourth_Card")})
        Get<UBorder>(BP, Name)->SetPadding(FMargin(12, 8));
    auto* Header = Get<UHorizontalBox>(BP, TEXT("HorizontalBox_FacilitiesHeader"));
    auto* Title = Get<UTextBlock>(BP, TEXT("Text_FacilitiesTitle"));
    H(*Header, *Title, 0, 0, FMargin(0, 0, 10, 0));
    auto* Vacancies = LabelLike(BP, TEXT("Text_Vacancies"), TEXT("빈자리 —명"), *Get<UTextBlock>(BP, TEXT("Text_AssignmentGuide")), true);
    H(*Header, *Vacancies, 1, 1.f, FMargin(0, 0, 10, 0));
    Get<UBorder>(BP, TEXT("Border_AssignmentGuide"))->bIsVariable = true;
    Get<UBorder>(BP, TEXT("Border_AssignmentGuide"))->SetVisibility(ESlateVisibility::Collapsed);
    Get<UTextBlock>(BP, TEXT("Text_IncidentTitle"))->bIsVariable = true;
    Get<UBorder>(BP, TEXT("Group_Incident"))->SetPadding(FMargin(14, 8));
    VPadding(Get<UBorder>(BP, TEXT("Group_Incident")), FMargin(0));
    if (!ReplaceEmbedded(BP, TEXT("Widget_StaffTraining"), TrainingClass)) return false;
    // 복제 클래스의 기본값을 명시해 이전 공용 위젯 인스턴스의 행 참조가 섞이지 않게 한다.
    auto* Training = Get<UFF_StaffTrainingWidget>(BP, TEXT("Widget_StaffTraining"));
    Training->bCompactLiveLayout = true;
    Training->StaffRowClass = LoadGeneratedClass(PromotionPackage);
    Training->StaffRowSpacing = 6.f;
    return true;
}
bool Backup(const FString& Package, const FString& Directory)
{
    if (!FPackageName::DoesPackageExist(Package)) return true;
    const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
    return IFileManager::Get().Copy(*(Directory / FPaths::GetCleanFilename(File)), *File, false, false) == COPY_OK;
}
UWidgetBlueprint* Duplicate(UWidgetBlueprint& Source, const FString& Package)
{
    auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    return Cast<UWidgetBlueprint>(AssetTools.DuplicateAsset(FPackageName::GetLongPackageAssetName(Package), Folder, &Source));
}
bool Finish(UWidgetBlueprint& BP)
{
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (SaveAssetPackage(Prefix, BP)) return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version); return false;
}
bool CheckBound(UUserWidget& Instance, const TCHAR* Name, UClass* ExpectedClass, int32& Count)
{
    const auto* Property = FindFProperty<FObjectPropertyBase>(Instance.GetClass(), Name);
    UObject* Bound = Property ? Property->GetObjectPropertyValue_InContainer(&Instance) : nullptr;
    if (!Bound || !Bound->IsA(ExpectedClass) || Bound != Instance.GetWidgetFromName(Name))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_BINDING_FAIL %s.%s"), Prefix, *Instance.GetClass()->GetName(), Name);
        return false;
    }
    ++Count;
    return true;
}
bool ValidateBindings(UWidgetBlueprint& Live, UWidgetBlueprint& Management, UWidgetBlueprint& Training, UWidgetBlueprint& Promotion)
{
    // Initialize는 컴파일된 트리를 복제하면서 중첩 UserWidget도 초기화한다(UE 5.7 UserWidget.cpp).
    // 화면에 붙이지 않아 NativeConstruct·게임 구독을 시작하지 않고 실제 native 포인터를 검사한다.
    TStrongObjectPtr<UUserWidget> Host(NewObject<UUserWidget>(GetTransientPackage(), Live.GeneratedClass.Get()));
    TStrongObjectPtr<UUserWidget> Row(NewObject<UUserWidget>(GetTransientPackage(), Promotion.GeneratedClass.Get()));
    if (!Host.IsValid() || !Row.IsValid() || !Host->Initialize() || !Row->Initialize())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_INITIALIZE_FAIL"), Prefix);
        return false;
    }
    int32 Count = 0;
    if (!CheckBound(*Host.Get(), TEXT("Widget_StaffManagement"), Management.GeneratedClass.Get(), Count)) return false;
    auto* ManagementInstance = CastChecked<UFF_StaffManagementWidget>(Host->GetWidgetFromName(TEXT("Widget_StaffManagement")));
    if (!CheckBound(*ManagementInstance, TEXT("Widget_StaffTraining"), Training.GeneratedClass.Get(), Count)) return false;
    auto* TrainingInstance = CastChecked<UFF_StaffTrainingWidget>(ManagementInstance->GetWidgetFromName(TEXT("Widget_StaffTraining")));
    if (!ManagementInstance->bUseCompactLiveLayout || !TrainingInstance->bCompactLiveLayout
        || TrainingInstance->StaffRowClass != Promotion.GeneratedClass.Get()
        || Training.GeneratedClass->GetDefaultObject<UFF_StaffTrainingWidget>()->StaffRowClass != Promotion.GeneratedClass.Get())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_DEFAULTS_FAIL: 라이브 인스턴스·CDO의 행 클래스와 간소화 옵션을 확인하세요."), Prefix);
        return false;
    }
    for (const TCHAR* Name : {TEXT("Text_Vacancies"), TEXT("Text_IncidentTitle"), TEXT("Text_IncidentSummary")})
        if (!CheckBound(*ManagementInstance, Name, UTextBlock::StaticClass(), Count)) return false;
    if (!CheckBound(*ManagementInstance, TEXT("Border_AssignmentGuide"), UBorder::StaticClass(), Count)
        || !CheckBound(*TrainingInstance, TEXT("Text_TrainingHelp"), UTextBlock::StaticClass(), Count)) return false;
    for (const TCHAR* Name : {TEXT("Text_StaffName"), TEXT("Text_StaffLevel"), TEXT("Text_AssignmentFacility"), TEXT("Text_AssignmentStatus")})
        if (!CheckBound(*Row.Get(), Name, UTextBlock::StaticClass(), Count)) return false;
    if (!CheckBound(*Row.Get(), TEXT("Button_Promote"), UButton::StaticClass(), Count)) return false;
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("LiveStaffCompact");
    const FString Report = FString::Printf(TEXT("{\"status\":\"passed\",\"token\":\"%s\",\"checked_bindings\":%d,\"initialized_widgets\":4}"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits), Count);
    if (!IFileManager::Get().MakeDirectory(*Directory, true)
        || !FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("binding-validation.json")))) return false;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BINDINGS_OK: 초기화된 실제 위젯의 native 바인딩 %d개 확인."), Prefix, Count);
    return true;
}
void Apply()
{
    if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PIE_RUNNING"), Prefix); return; }
    auto* OriginalM = LoadExistingBlueprint(OriginalManagement);
    auto* OriginalT = LoadExistingBlueprint(OriginalTraining);
    auto* OriginalP = LoadExistingBlueprint(OriginalPromotion);
    auto* Live = LoadExistingBlueprint(LivePackage);
    if (!Valid(OriginalM, UFF_StaffManagementWidget::StaticClass()) || !Valid(OriginalT, UFF_StaffTrainingWidget::StaticClass())
        || !Valid(OriginalP, UFF_StaffPromotionRowWidget::StaticClass()) || !Valid(Live, UFF_LivePanelStaff::StaticClass())
        || !CheckManagement(*OriginalM) || !CheckTraining(*OriginalT) || !CheckPromotion(*OriginalP)
        || !Require<UFF_StaffManagementWidget>(*Live, {TEXT("Widget_StaffManagement")})
        || !Require<UButton>(*Live, {TEXT("Button_Close")}) || !Require<UTextBlock>(*Live, {TEXT("Text_PanelTitle")}))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_CONTRACT_FAIL: 부모·위젯·미저장 상태를 확인하세요."), Prefix); return; }
    UWidgetBlueprint* ExistingM = LoadExistingBlueprint(ManagementPackage);
    UWidgetBlueprint* ExistingT = LoadExistingBlueprint(TrainingPackage);
    UWidgetBlueprint* ExistingP = LoadExistingBlueprint(PromotionPackage);
    if (ExistingM || ExistingT || ExistingP || Applied(*Live))
    {
        if (Valid(ExistingM, UFF_StaffManagementWidget::StaticClass()) && Valid(ExistingT, UFF_StaffTrainingWidget::StaticClass())
            && Valid(ExistingP, UFF_StaffPromotionRowWidget::StaticClass()) && Applied(*ExistingM) && Applied(*ExistingT)
            && Applied(*ExistingP) && Applied(*Live)
            && Get<UFF_StaffManagementWidget>(*Live, TEXT("Widget_StaffManagement"))->GetClass() == ExistingM->GeneratedClass
            && Get<UFF_StaffTrainingWidget>(*ExistingM, TEXT("Widget_StaffTraining"))
            && Get<UFF_StaffTrainingWidget>(*ExistingM, TEXT("Widget_StaffTraining"))->GetClass() == ExistingT->GeneratedClass)
        {
            if (!ValidateBindings(*Live, *ExistingM, *ExistingT, *ExistingP)) return;
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 저장된 라이브 전용 배치를 보존했습니다."), Prefix);
        }
        else
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_CONFLICT: 기존 또는 부분 적용 에셋은 덮어쓰지 않습니다."), Prefix);
        }
        return;
    }
    const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveStaffCompact") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (!IFileManager::Get().MakeDirectory(*BackupDir, true)) return;
    for (const FString& Package : {OriginalManagement, OriginalTraining, OriginalPromotion, LivePackage})
        if (!Backup(Package, BackupDir)) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_BACKUP_FAIL %s"), Prefix, *Package); return; }
    auto* Promotion = Duplicate(*OriginalP, PromotionPackage);
    if (!Promotion) return;
    MigratePromotion(*Promotion);
    if (!SaveNewBlueprint(Prefix, *Promotion)) return;
    auto* PromotionCDO = Promotion->GeneratedClass->GetDefaultObject<UFF_StaffPromotionRowWidget>();
    PromotionCDO->bCompactLiveLayout = true;
    PromotionCDO->bCompactManagementRow = true;
    if (!Finish(*Promotion)) return;
    auto* Training = Duplicate(*OriginalT, TrainingPackage);
    if (!Training) return;
    MigrateTraining(*Training);
    if (!SaveNewBlueprint(Prefix, *Training)) return;
    auto* TrainingCDO = Training->GeneratedClass->GetDefaultObject<UFF_StaffTrainingWidget>();
    TrainingCDO->bCompactLiveLayout = true;
    TrainingCDO->StaffRowSpacing = 6.f;
    // 복제에는 원본 행 클래스가 이미 있으므로 '비었을 때만' 설정하는 Configure 함수를 쓰지 않는다.
    TrainingCDO->StaffRowClass = Promotion->GeneratedClass.Get();
    if (!Finish(*Training)) return;
    auto* Management = Duplicate(*OriginalM, ManagementPackage);
    if (!Management || !MigrateManagement(*Management, Training->GeneratedClass.Get())) return;
    if (!SaveNewBlueprint(Prefix, *Management)) return;
    Management->GeneratedClass->GetDefaultObject<UFF_StaffManagementWidget>()->bUseCompactLiveLayout = true;
    if (!Finish(*Management)) return;
    Live->Modify(); Live->WidgetTree->Modify();
    if (!ReplaceEmbedded(*Live, TEXT("Widget_StaffManagement"), Management->GeneratedClass.Get())) return;
    Get<UFF_StaffManagementWidget>(*Live, TEXT("Widget_StaffManagement"))->bUseCompactLiveLayout = true;
    if (!SaveNewBlueprint(Prefix, *Live) || !Finish(*Live)) return;
    if (!ValidateBindings(*Live, *Management, *Training, *Promotion)) return;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 라이브 3개 복제 및 본문 교체 완료. 백업=%s"), Prefix, *BackupDir);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyLiveStaffCompactDesign"),
    TEXT("라이브 스태프 간소화: 공용 원본을 복제하고 라이브 본문만 교체한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}
