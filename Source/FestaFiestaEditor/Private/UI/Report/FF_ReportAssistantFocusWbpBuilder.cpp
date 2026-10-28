// 일일 결산의 비서 카드만 Footer 바로 위로 옮긴다. 본문·버튼·HUD 배치는 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace FFReportAssistantFocusBuilder
{
using namespace FFUIWbpBuilder;
using namespace FFPrepPanelSkyBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_REPORT_ASSISTANT_FOCUS");
constexpr const TCHAR* Version = TEXT("FF.ReportAssistantFocus.Version");
const FString Package = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelReport");
const FString IconPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_SNS_White_256");

template<typename T> T* Get(UWidgetBlueprint& BP, const TCHAR* Name)
{
    return Cast<T>(BP.WidgetTree->FindWidget(Name));
}
UWidget* DirectSectionChild(UVerticalBox* Sections, UWidget* Descendant)
{
    if (!Sections) return nullptr;
    // 구분선은 두께를 지정하는 SizeBox 안에 있을 수 있다. 기존 래퍼·슬롯을 함께 보존한다.
    for (UWidget* Current = Descendant; Current; Current = Current->GetParent())
    {
        if (Current->GetParent() == Sections) return Current;
    }
    return nullptr;
}
UTextBlock* Label(UWidgetBlueprint& BP, const TCHAR* Name, const FText& Value, const FSlateFontInfo& BaseFont,
    int32 Size, bool Bold, bool Variable)
{
    auto* Result = MakeWidget<UTextBlock>(BP, *BP.WidgetTree, Name, Variable);
    auto Font = BaseFont; Font.Size = Size; Font.TypefaceFontName = Bold ? TEXT("SemiBold") : TEXT("Medium");
    Result->SetFont(Font); Result->SetColorAndOpacity(Ink); Result->SetText(Value);
    Result->SetAutoWrapText(true); Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return Result;
}
bool Validate(UWidgetBlueprint& BP)
{
    auto* Sections = Get<UVerticalBox>(BP, TEXT("Report_Sections"));
    auto* Scroll = Get<UScrollBox>(BP, TEXT("Report_ScrollBody"));
    auto* Group = Get<UBorder>(BP, TEXT("Group_AssistantComment"));
    auto* FooterRule = BP.WidgetTree->FindWidget(TEXT("Report_FooterRule"));
    UWidget* FooterSection = DirectSectionChild(Sections, FooterRule);
    if (!Sections || !Scroll || !Group || !FooterSection || Group->GetParent() != Sections || Scroll->GetParent() != Sections
        || Sections->GetChildIndex(Group) <= Sections->GetChildIndex(Scroll)
        || Sections->GetChildIndex(Group) + 1 != Sections->GetChildIndex(FooterSection)) return false;
    TStrongObjectPtr<UUserWidget> Instance(NewObject<UUserWidget>(GetTransientPackage(), BP.GeneratedClass.Get()));
    if (!Instance.IsValid() || !Instance->Initialize()) return false;
    int32 Count = 0;
    for (const TCHAR* Name : {TEXT("Text_AssistantAdvice"), TEXT("Text_AssistantComment"), TEXT("Group_AssistantComment")})
    {
        const auto* Property = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), Name);
        UObject* Bound = Property ? Property->GetObjectPropertyValue_InContainer(Instance.Get()) : nullptr;
        if (!Bound || Bound != Instance->GetWidgetFromName(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_BINDING_FAIL %s"), Prefix, Name); return false;
        }
        ++Count;
    }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("ReportAssistantFocus");
    const FString Report = FString::Printf(TEXT("{\"status\":\"passed\",\"token\":\"%s\",\"checked_bindings\":%d,\"outside_scroll\":true}"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits), Count);
    if (!IFileManager::Get().MakeDirectory(*Directory, true)
        || !FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("binding-validation.json")))) return false;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BINDINGS_OK"), Prefix);
    return true;
}
void Apply()
{
    if (GEditor && GEditor->PlayWorld)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PIE_RUNNING"), Prefix); return;
    }
    auto* BP = LoadExistingBlueprint(Package);
    if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !BP->ParentClass
        || !BP->ParentClass->IsChildOf(UFF_LivePanelReport::StaticClass()) || BP->GetOutermost()->IsDirty())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_CONTRACT_FAIL: 결산 부모·미저장 상태를 확인하세요."), Prefix); return;
    }
    if (BP->GetOutermost()->GetMetaData().GetValue(BP, Version) == TEXT("1"))
    {
        if (Validate(*BP))
        {
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 저장된 결산 디자인을 보존했습니다."), Prefix);
        }
        else
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_REUSE_VALIDATION_FAIL"), Prefix);
        }
        return;
    }
    auto* Sections = Get<UVerticalBox>(*BP, TEXT("Report_Sections"));
    auto* Scroll = Get<UScrollBox>(*BP, TEXT("Report_ScrollBody"));
    auto* Body = Get<UVerticalBox>(*BP, TEXT("Report_Body"));
    auto* Footer = Get<UVerticalBox>(*BP, TEXT("Report_Footer"));
    auto* FooterRule = BP->WidgetTree->FindWidget(TEXT("Report_FooterRule"));
    UWidget* FooterSection = DirectSectionChild(Sections, FooterRule);
    auto* StyleSource = Get<UTextBlock>(*BP, TEXT("Text_MissingNote"));
    auto* ExistingComment = Get<UTextBlock>(*BP, TEXT("Text_AssistantComment"));
    auto* IconTexture = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconPackage));
    if (!Sections || !Scroll || !Body || !Footer || !FooterSection || !StyleSource || !IconTexture
        || Scroll->GetParent() != Sections || Footer->GetParent() != Sections
        || Sections->GetChildIndex(Scroll) >= Sections->GetChildIndex(FooterSection)
        || Sections->GetChildIndex(FooterSection) >= Sections->GetChildIndex(Footer))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_STRUCTURE_FAIL: Sections=%s Scroll=%s(parent=%s) Body=%s Footer=%s(parent=%s) Rule=%s(parent=%s) RuleSection=%s Style=%s Icon=%s"),
            Prefix, *GetNameSafe(Sections), *GetNameSafe(Scroll), *GetNameSafe(Scroll ? Scroll->GetParent() : nullptr),
            *GetNameSafe(Body), *GetNameSafe(Footer), *GetNameSafe(Footer ? Footer->GetParent() : nullptr),
            *GetNameSafe(FooterRule), *GetNameSafe(FooterRule ? FooterRule->GetParent() : nullptr),
            *GetNameSafe(FooterSection), *GetNameSafe(StyleSource), *GetNameSafe(IconTexture));
        return;
    }
    if (BP->WidgetTree->FindWidget(TEXT("Text_AssistantComment")) && !ExistingComment) return;
    for (const TCHAR* Name : {TEXT("Group_AssistantComment"), TEXT("Text_AssistantAdvice"), TEXT("AssistantFocus_Columns"),
        TEXT("AssistantFocus_Body"), TEXT("AssistantFocus_Title"), TEXT("AssistantFocus_Icon"),
        TEXT("AssistantFocus_IconCircle"), TEXT("AssistantFocus_IconSize")})
    {
        if (BP->WidgetTree->FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_PARTIAL_CONFLICT %s"), Prefix, Name); return;
        }
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
    const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/ReportAssistantFocus") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if (IFileManager::Get().IsReadOnly(*Filename) || !IFileManager::Get().MakeDirectory(*Backup, true)
        || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(Filename)), *Filename, false, false) != COPY_OK) return;
    BP->Modify(); BP->WidgetTree->Modify(); Sections->Modify();
    const FSlateFontInfo Font = StyleSource->GetFont();
    auto* Card = MakeWidget<UBorder>(*BP, *BP->WidgetTree, TEXT("Group_AssistantComment"), true);
    Card->SetBrush(Brush(Color(232, 248, 241), 7.f, 1.f, Color(166, 212, 197)));
    Card->SetPadding(FMargin(18.f, 14.f));
    auto* Columns = MakeWidget<UHorizontalBox>(*BP, *BP->WidgetTree, TEXT("AssistantFocus_Columns")); Card->AddChild(Columns);
    auto* Image = MakeWidget<UImage>(*BP, *BP->WidgetTree, TEXT("AssistantFocus_Icon"));
    Image->SetBrushFromTexture(IconTexture); Image->SetColorAndOpacity(FLinearColor::White);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Circle = MakeWidget<UBorder>(*BP, *BP->WidgetTree, TEXT("AssistantFocus_IconCircle"));
    Circle->SetBrush(Brush(Ink, 25.f)); Circle->SetPadding(FMargin(12.f)); Circle->AddChild(Image);
    auto* IconSize = MakeWidget<USizeBox>(*BP, *BP->WidgetTree, TEXT("AssistantFocus_IconSize"));
    IconSize->SetWidthOverride(50.f); IconSize->SetHeightOverride(50.f); IconSize->AddChild(Circle);
    auto* IconSlot = Columns->AddChildToHorizontalBox(IconSize); IconSlot->SetPadding(FMargin(0.f, 4.f, 18.f, 0.f));
    IconSlot->SetVerticalAlignment(VAlign_Top);
    auto* CardBody = MakeWidget<UVerticalBox>(*BP, *BP->WidgetTree, TEXT("AssistantFocus_Body"));
    auto* BodySlot = Columns->AddChildToHorizontalBox(CardBody); BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BodySlot->SetVerticalAlignment(VAlign_Center);
    Row(*CardBody, *Label(*BP, TEXT("AssistantFocus_Title"),
        NSLOCTEXT("FestaFiestaReportAssistantFocus", "Title", "비서 한마디"), Font, 16, true, false), FMargin(0.f, 0.f, 0.f, 6.f));
    Row(*CardBody, *Label(*BP, TEXT("Text_AssistantAdvice"), FText::GetEmpty(), Font, 24, true, true));
    if (ExistingComment)
    {
        ExistingComment->Modify(); ExistingComment->RemoveFromParent();
        auto DetailFont = ExistingComment->GetFont(); DetailFont.Size = 15; ExistingComment->SetFont(DetailFont);
        ExistingComment->SetAutoWrapText(true);
    }
    else
    {
        ExistingComment = Label(*BP, TEXT("Text_AssistantComment"), FText::GetEmpty(), Font, 15, false, true);
    }
    ExistingComment->SetColorAndOpacity(Muted);
    Row(*CardBody, *ExistingComment, FMargin(0.f, 7.f, 0.f, 0.f));
    // 본문 스크롤과 Footer 슬롯에는 손대지 않고 두 영역 사이에 Auto 높이 카드만 넣는다.
    const int32 FooterIndex = Sections->GetChildIndex(FooterSection);
    auto* CardSlot = CastChecked<UVerticalBoxSlot>(Sections->InsertChildAt(FooterIndex, Card));
    CardSlot->SetHorizontalAlignment(HAlign_Fill); CardSlot->SetPadding(FMargin(22.f, 0.f, 22.f, 12.f));
    BP->GetOutermost()->GetMetaData().SetValue(BP, Version, TEXT("1"));
    if (!SaveNewBlueprint(Prefix, *BP) || !Validate(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_APPLY_FAIL: 백업=%s"), Prefix, *Backup); return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 비서 카드만 Footer 위 고정. 백업=%s"), Prefix, *Backup);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyReportAssistantFocusDesign"),
    TEXT("일일 결산의 비서 한마디를 스크롤 밖 고정 카드로 강조한다."), FConsoleCommandDelegate::CreateStatic(&Apply));
}
