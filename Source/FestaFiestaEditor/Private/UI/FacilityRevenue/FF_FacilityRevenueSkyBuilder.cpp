// 승인된 중앙형 매출 순위 시안을 기존 패널·행에 한 번 적용한다.
// 기존 위젯 객체, 데이터 바인딩, 목록 선택과 처음 사용 안내는 유지한다.
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFFacilityRevenueSky
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_REVENUE_SKY");
constexpr const TCHAR* Version = TEXT("FF.FacilityRevenue.Sky20261010");
FLinearColor C(uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R,G,B)); }
const FLinearColor Ink = C(25,69,57), Muted = C(105,134,124), Paper = C(252,253,247),
    Pale = C(242,248,250), Blue = C(227,244,251), Line = C(169,216,233), Teal = C(24,140,94);
FSlateBrush Brush(FLinearColor Fill, float Radius = 5.f, float Stroke = 1.f)
{
    FSlateBrush B; B.DrawAs = ESlateBrushDrawType::RoundedBox; B.TintColor = Fill;
    B.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Stroke);
    return B;
}
template<typename T> T* Get(UWidgetTree& Tree, const TCHAR* Name) { return Cast<T>(Tree.FindWidget(Name)); }
void Font(UTextBlock* Text, int Size, FLinearColor Color, bool Bold = false)
{
    auto F = Text->GetFont(); F.Size = Size; F.TypefaceFontName = Bold ? TEXT("Bold") : TEXT("Medium");
    Text->SetFont(F); Text->SetColorAndOpacity(Color); Text->SetAutoWrapText(false);
    Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
}
void Surface(UBorder* Border, FLinearColor Color, FMargin Padding)
{
    Border->SetBrush(Brush(Color)); Border->SetBrushColor(FLinearColor::White); Border->SetPadding(Padding);
}
UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Value,
    int Size = 16, bool Bold = false, bool Variable = false)
{
    auto* W = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
    StyleText(*W, Size, Ink, Bold ? TEXT("Bold") : TEXT("Medium"));
    W->SetText(FText::FromString(Value)); W->SetAutoWrapText(false);
    W->SetVisibility(ESlateVisibility::HitTestInvisible);
    return W;
}
void V(UVerticalBox* Parent, UWidget* W, FMargin Padding = FMargin(0), bool Fill = false)
{
    W->RemoveFromParent(); auto* S = Parent->AddChildToVerticalBox(W); S->SetPadding(Padding);
    S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
}
void H(UHorizontalBox* Parent, UWidget* W, FMargin Padding = FMargin(0), bool Fill = false)
{
    W->RemoveFromParent(); auto* S = Parent->AddChildToHorizontalBox(W); S->SetPadding(Padding);
    S->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic)); S->SetVerticalAlignment(VAlign_Center);
}
USizeBox* Width(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget* Child, float Size)
{
    auto* W = MakeWidget<USizeBox>(BP, Tree, Name); W->SetWidthOverride(Size);
    Child->RemoveFromParent(); W->AddChild(Child); return W;
}
void Button(UButton* W, bool Primary = false, bool Period = false)
{
    auto Style = W->GetStyle();
    Style.Normal = Brush(Primary ? Teal : Paper); Style.Hovered = Brush(Primary ? C(29,157,108) : Blue);
    Style.Pressed = Brush(Primary ? Ink : Pale);
    Style.Disabled = Brush(Period ? Ink : C(156,183,173));
    Style.NormalPadding = Style.PressedPadding = FMargin(12,6);
    W->SetStyle(Style); W->SetBackgroundColor(FLinearColor::White);
    if (auto* Label = Cast<UTextBlock>(W->GetContent())) Font(Label,16,Primary ? FLinearColor::White : Ink,true);
}
bool Applied(UWidgetBlueprint& BP) { return BP.GetOutermost()->GetMetaData().GetValue(&BP,Version) == TEXT("1"); }
bool Require(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names)
{
    if (!BP.WidgetTree) return false;
    for (const TCHAR* Name : Names) if (!BP.WidgetTree->FindWidget(Name))
    { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_MISSING %s.%s"),Prefix,*BP.GetName(),Name); return false; }
    return true;
}
void ApplyRow(UWidgetBlueprint& BP)
{
    UWidgetTree& T = *BP.WidgetTree;
    Get<USizeBox>(T,TEXT("Size_Row"))->SetHeightOverride(86);
    Get<USizeBox>(T,TEXT("Size_Rank"))->SetWidthOverride(48);
    Get<USizeBox>(T,TEXT("Size_Thumbnail"))->SetWidthOverride(60);
    Get<USizeBox>(T,TEXT("Size_Thumbnail"))->SetHeightOverride(60);
    Get<USizeBox>(T,TEXT("Size_AuroraRevenueAmount"))->SetWidthOverride(140);
    Surface(Get<UBorder>(T,TEXT("Border_Row")),Paper,FMargin(12,8));
    Surface(Get<UBorder>(T,TEXT("Border_Thumbnail")),Pale,FMargin(3));
    Font(Get<UTextBlock>(T,TEXT("Text_Rank")),20,Ink,true);
    Get<UTextBlock>(T,TEXT("Text_Rank"))->SetJustification(ETextJustify::Center);
    Font(Get<UTextBlock>(T,TEXT("Text_Name")),19,Ink,true);
    Font(Get<UTextBlock>(T,TEXT("Text_Amount")),21,Ink,true);
    Font(Get<UTextBlock>(T,TEXT("Text_Share")),19,Ink,true);
    Get<UTextBlock>(T,TEXT("Text_Location"))->SetVisibility(ESlateVisibility::Collapsed);
    auto* Shares = MakeWidget<UVerticalBox>(BP,T,TEXT("Sky_Column_Share"));
    V(Shares,Get<UTextBlock>(T,TEXT("Text_Share")));
    auto* Mark = Text(BP,T,TEXT("Text_SelectionMark"),TEXT("✓ 선택됨"),12,false,true);
    Mark->SetJustification(ETextJustify::Right); Mark->SetColorAndOpacity(Teal);
    Mark->SetVisibility(ESlateVisibility::Hidden); V(Shares,Mark,FMargin(0,2,0,0));
    H(Get<UHorizontalBox>(T,TEXT("Row_Contents")),Width(BP,T,TEXT("Sky_Size_Share"),Shares,104),FMargin(24,0,0,0));
    if (auto* S = Cast<UHorizontalBoxSlot>(Get<USizeBox>(T,TEXT("Size_AuroraRevenueAmount"))->Slot)) S->SetPadding(FMargin(24,0,0,0));
    auto* Bar = Get<UProgressBar>(T,TEXT("Bar_Revenue"));
    auto Style = Bar->GetWidgetStyle(); Style.BackgroundImage = Brush(C(221,235,237),3,0);
    Style.FillImage = Brush(FLinearColor::White,3,0); Bar->SetWidgetStyle(Style); Bar->SetFillColorAndOpacity(C(69,157,135));
    Get<USizeBox>(T,TEXT("Size_RevenueBar"))->SetHeightOverride(6);
}
void ApplyPanel(UWidgetBlueprint& BP)
{
    UWidgetTree& T = *BP.WidgetTree;
    auto* Frame = Get<USizeBox>(T,TEXT("Size_ClarityFrame"));
    Frame->SetWidthOverride(1060); Frame->SetHeightOverride(690);
    auto* Slot = CastChecked<UCanvasPanelSlot>(Frame->Slot);
    Slot->SetAnchors(FAnchors(.5f,.5f)); Slot->SetAlignment(FVector2D(.5f,.5f));
    Slot->SetOffsets(FMargin(0,-29,0,0)); Slot->SetAutoSize(true);
    Surface(Get<UBorder>(T,TEXT("Border_Panel")),Paper,FMargin(24,20));
    auto* Contents = Get<UVerticalBox>(T,TEXT("Column_Panel"));
    auto* Header = Get<UHorizontalBox>(T,TEXT("Row_Header"));
    auto* Title = Get<UTextBlock>(T,TEXT("Text_Title"));
    Title->SetText(FText::FromString(TEXT("매출 순위"))); Font(Title,28,Ink,true);
    auto* Titles = MakeWidget<UVerticalBox>(BP,T,TEXT("Sky_Column_Title"));
    V(Titles,Title);
    auto* Subtitle = Get<UTextBlock>(T,TEXT("Text_Order"));
    Subtitle->SetText(FText::FromString(TEXT("시설별 매출과 기여도를 확인하세요."))); Font(Subtitle,16,Muted);
    Subtitle->SetVisibility(ESlateVisibility::HitTestInvisible); V(Titles,Subtitle,FMargin(0,3,0,0));
    H(Header,Titles,FMargin(0),true); Header->ShiftChild(0,Titles);
    for (const TCHAR* Name : {TEXT("Button_Close"),TEXT("Button_Today"),TEXT("Button_Total"),TEXT("Button_Inspect")})
        Button(Get<UButton>(T,Name),FString(Name)==TEXT("Button_Inspect"),FString(Name)==TEXT("Button_Today")||FString(Name)==TEXT("Button_Total"));
    auto* Close = Get<UTextBlock>(T,TEXT("Button_Close_Label")); Close->SetText(FText::FromString(TEXT("×"))); Font(Close,22,Ink);
    auto* CloseBounds = Get<USizeBox>(T,TEXT("Size_Clarity_Button_Close")); CloseBounds->SetWidthOverride(36); CloseBounds->SetHeightOverride(36);
    // 기간 버튼은 왼쪽 절반에, 금화 단위는 화면에 한 번만 표시한다.
    auto* Period = Get<UHorizontalBox>(T,TEXT("Row_Period"));
    for (const TCHAR* Name : {TEXT("Size_Clarity_Button_Today"),TEXT("Size_Clarity_Button_Total")})
    {
        auto* Bounds = Get<USizeBox>(T,Name); Bounds->SetWidthOverride(240);
        auto* S = CastChecked<UHorizontalBoxSlot>(Bounds->Slot); S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetPadding(FMargin(0,0,8,0));
    }
    auto* Unit = Text(BP,T,TEXT("Sky_Text_Unit"),TEXT("금액 단위: 금화"),14); Unit->SetColorAndOpacity(Muted); Unit->SetJustification(ETextJustify::Right);
    H(Period,Unit,FMargin(8,0,0,0),true);
    auto* HeaderLine = MakeWidget<UBorder>(BP,T,TEXT("Sky_Border_HeaderLine")); HeaderLine->SetBrush(Brush(Line,0,0)); HeaderLine->SetPadding(FMargin(0));
    auto* HeaderLineSize = MakeWidget<USizeBox>(BP,T,TEXT("Sky_Size_HeaderLine")); HeaderLineSize->SetHeightOverride(1); HeaderLineSize->AddChild(HeaderLine);
    const int32 PeriodIndex=Contents->GetChildIndex(Period); V(Contents,HeaderLineSize,FMargin(0,0,0,14)); Contents->ShiftChild(PeriodIndex,HeaderLineSize);
    // 합계와 집계 시설 수를 각각 분리하되 기존 Text_Total/Count 바인딩은 그대로 재사용한다.
    Surface(Get<UBorder>(T,TEXT("Border_Total")),Blue,FMargin(20,12));
    auto* TotalColumn = Get<UVerticalBox>(T,TEXT("Column_Total"));
    auto* Totals = MakeWidget<UHorizontalBox>(BP,T,TEXT("Sky_Row_Summary"));
    auto* AmountColumn = MakeWidget<UVerticalBox>(BP,T,TEXT("Sky_Column_Amount"));
    auto* CountColumn = MakeWidget<UVerticalBox>(BP,T,TEXT("Sky_Column_Count"));
    auto* AmountLabel = Get<UTextBlock>(T,TEXT("Text_TotalLabel")); Font(AmountLabel,16,Muted,true);
    auto* Amount = Get<UTextBlock>(T,TEXT("Text_Total")); Font(Amount,34,Ink,true);
    for (UTextBlock* L : {AmountLabel,Amount}) L->SetJustification(ETextJustify::Center);
    V(AmountColumn,AmountLabel); V(AmountColumn,Amount,FMargin(0,5,0,0));
    auto* CountLabel = Text(BP,T,TEXT("Sky_Text_CountLabel"),TEXT("매출 집계 시설"),16,true); CountLabel->SetJustification(ETextJustify::Center); CountLabel->SetColorAndOpacity(Muted);
    auto* Count = Get<UTextBlock>(T,TEXT("Text_Count")); Font(Count,28,Ink,true); Count->SetJustification(ETextJustify::Center);
    auto* Scope = Text(BP,T,TEXT("Sky_Text_CountScope"),TEXT("현재 설치된 시설 기준"),12); Scope->SetColorAndOpacity(Muted); Scope->SetJustification(ETextJustify::Center);
    V(CountColumn,CountLabel); V(CountColumn,Count,FMargin(0,3,0,0)); V(CountColumn,Scope,FMargin(0,2,0,0));
    H(Totals,AmountColumn,FMargin(0),true); H(Totals,CountColumn,FMargin(0),true); V(TotalColumn,Totals);
    // 표 머리글은 각 행의 금액·비중 열과 같은 폭을 사용한다.
    auto* TableBorder = MakeWidget<UBorder>(BP,T,TEXT("Sky_Border_TableHeader")); Surface(TableBorder,Pale,FMargin(12,7));
    auto* TableHeader = MakeWidget<UHorizontalBox>(BP,T,TEXT("Sky_Row_TableHeader")); TableBorder->AddChild(TableHeader);
    H(TableHeader,Width(BP,T,TEXT("Sky_Size_RankHeader"),Text(BP,T,TEXT("Sky_Text_RankHeader"),TEXT("순위"),15,true),48),FMargin(0,0,8,0));
    H(TableHeader,Text(BP,T,TEXT("Sky_Text_NameHeader"),TEXT("시설명"),15,true),FMargin(0),true);
    auto* AmountHead=Text(BP,T,TEXT("Sky_Text_AmountHeader"),TEXT("매출"),15,true); AmountHead->SetJustification(ETextJustify::Right);
    auto* ShareHead=Text(BP,T,TEXT("Sky_Text_ShareHeader"),TEXT("비중"),15,true); ShareHead->SetJustification(ETextJustify::Right);
    H(TableHeader,Width(BP,T,TEXT("Sky_Size_AmountHeader"),AmountHead,140),FMargin(24,0,0,0));
    H(TableHeader,Width(BP,T,TEXT("Sky_Size_ShareHeader"),ShareHead,104),FMargin(24,0,0,0));
    auto* ListArea=T.FindWidget(TEXT("Overlay_AuroraRevenueList"));
    const int32 ListIndex=Contents->GetChildIndex(ListArea); V(Contents,TableBorder,FMargin(0,0,0,8)); Contents->ShiftChild(ListIndex,TableBorder);
    auto* List = Get<UFF_FacilityRevenueListView>(T,TEXT("List_Ranking")); List->SetScrollbarVisibility(ESlateVisibility::Visible);
    if (auto* Property=FindFProperty<FStructProperty>(List->GetClass(),TEXT("ScrollBarStyle")))
    {
        auto& Style=*Property->ContainerPtrToValuePtr<FScrollBarStyle>(List);
        Style.Thickness=6.f;
        Style.NormalThumbImage=Brush(C(130,185,200),3,0); Style.HoveredThumbImage=Brush(C(94,161,180),3,0); Style.DraggedThumbImage=Brush(Ink,3,0);
        Style.VerticalBackgroundImage=Brush(Pale,3,0); Style.VerticalTopSlotImage=Style.VerticalBottomSlotImage=Brush(FLinearColor::Transparent,0,0);
        Style.NormalThumbImage.ImageSize=Style.HoveredThumbImage.ImageSize=Style.DraggedThumbImage.ImageSize=FVector2D(6,20);
    }
    Font(Get<UTextBlock>(T,TEXT("Text_Empty")),17,Muted);
    Surface(Get<UBorder>(T,TEXT("Border_FirstRevenue")),Pale,FMargin(10,8));
    Font(Get<UTextBlock>(T,TEXT("Text_FirstRevenueDescription")),14,Muted);
    auto* Footer=MakeWidget<UHorizontalBox>(BP,T,TEXT("Sky_Row_Footer"));
    auto* Hint=Get<UTextBlock>(T,TEXT("Text_Footnote")); Hint->SetText(FText::FromString(TEXT("시설을 선택하면 자세한 정보를 확인할 수 있습니다.")));
    Font(Hint,14,Muted); Hint->SetVisibility(ESlateVisibility::HitTestInvisible); H(Footer,Hint,FMargin(0,0,16,0),true);
    auto* Inspect=Get<USizeBox>(T,TEXT("Size_Clarity_Button_Inspect")); Inspect->SetWidthOverride(266); H(Footer,Inspect);
    auto* FooterLine = MakeWidget<UBorder>(BP,T,TEXT("Sky_Border_FooterLine")); FooterLine->SetBrush(Brush(Line,0,0)); FooterLine->SetPadding(FMargin(0));
    auto* FooterLineSize=MakeWidget<USizeBox>(BP,T,TEXT("Sky_Size_FooterLine")); FooterLineSize->SetHeightOverride(1); FooterLineSize->AddChild(FooterLine);
    V(Contents,FooterLineSize,FMargin(0,12,0,0));
    V(Contents,Footer,FMargin(0,14,0,0));
}
void SetColor(UWidgetBlueprint& BP,const TCHAR* Name,FLinearColor Value)
{
    if (UObject* CDO=BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr)
        if (auto* Prop=FindFProperty<FStructProperty>(CDO->GetClass(),Name)) *Prop->ContainerPtrToValuePtr<FLinearColor>(CDO)=Value;
}
void Apply()
{
    if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_PIE_RUNNING"),Prefix); return; }
    auto* Row=LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityRevenueRankRowPackage);
    auto* Panel=LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityRevenueRankingPackage);
    if (!Row || !Panel || !Row->ParentClass || !Panel->ParentClass ||
        !Row->ParentClass->IsChildOf(UFF_FacilityRevenueRankRow::StaticClass()) || !Panel->ParentClass->IsChildOf(UFF_FacilityRevenueRankingPanel::StaticClass())) return;
    if (!Require(*Row,{TEXT("Size_Row"),TEXT("Size_Rank"),TEXT("Size_Thumbnail"),TEXT("Border_Row"),TEXT("Border_Thumbnail"),TEXT("Row_Contents"),TEXT("Text_Rank"),TEXT("Text_Name"),TEXT("Text_Location"),TEXT("Text_Amount"),TEXT("Text_Share"),TEXT("Size_AuroraRevenueAmount"),TEXT("Bar_Revenue"),TEXT("Size_RevenueBar")}) ||
        !Require(*Panel,{TEXT("Size_ClarityFrame"),TEXT("Border_Panel"),TEXT("Column_Panel"),TEXT("Row_Header"),TEXT("Text_Title"),TEXT("Text_Order"),TEXT("Row_Period"),TEXT("Button_Close"),TEXT("Button_Close_Label"),TEXT("Button_Today"),TEXT("Button_Total"),TEXT("Button_Inspect"),TEXT("Size_Clarity_Button_Close"),TEXT("Size_Clarity_Button_Today"),TEXT("Size_Clarity_Button_Total"),TEXT("Size_Clarity_Button_Inspect"),TEXT("Border_Total"),TEXT("Column_Total"),TEXT("Text_TotalLabel"),TEXT("Text_Total"),TEXT("Text_Count"),TEXT("List_Ranking"),TEXT("Overlay_AuroraRevenueList"),TEXT("Text_Empty"),TEXT("Border_FirstRevenue"),TEXT("Text_FirstRevenueDescription"),TEXT("Text_Footnote")})) return;
    if (!Cast<UCanvasPanelSlot>(Get<USizeBox>(*Panel->WidgetTree,TEXT("Size_ClarityFrame"))->Slot)) return;
    const FString Backup=FPaths::ProjectSavedDir()/TEXT("UIBackups/RevenueSky")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
    for (auto* BP : {Row,Panel})
    {
        if (Applied(*BP)) continue;
        if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("%s_DIRTY %s"),Prefix,*BP->GetName()); return; }
        const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if (!IFileManager::Get().MakeDirectory(*Backup,true) || IFileManager::Get().Copy(*(Backup/FPaths::GetCleanFilename(File)),*File,false,false)!=COPY_OK) return;
    }
    for (auto* BP : {Row,Panel})
    {
        if (Applied(*BP)) continue;
        BP->Modify(); BP->WidgetTree->Modify();
        if (BP==Row) ApplyRow(*BP); else ApplyPanel(*BP);
        // 새 구조를 먼저 컴파일한 뒤 CDO 스타일을 저장한다.
        if (!SaveNewBlueprint(Prefix,*BP)) return;
        if (BP==Row) SetColor(*BP,TEXT("SelectedTint"),C(226,246,252));
        else { SetColor(*BP,TEXT("InactivePeriodTextColor"),Ink); SetColor(*BP,TEXT("SelectedPeriodTextColor"),FLinearColor::White); }
        BP->GetOutermost()->GetMetaData().SetValue(BP,Version,TEXT("1"));
        if (!SaveAssetPackage(Prefix,*BP)) return;
    }
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("%s_APPLIED: 중앙 패널·표형 순위, 원본 백업 %s"),Prefix,*Backup);
}
FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityRevenueSky"),TEXT("기존 매출 순위 패널/행을 중앙 HUD 스타일로 한 번 변경한다. 백업 후 적용하며 재실행은 디자이너 편집을 보존한다."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
