// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/UnrealType.h"

namespace FFBuildPearlCatalog
{
using namespace FFUIWbpBuilder;
using namespace FFBuildPearl;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_PEARL_CATALOG");

void Hide(UWidgetTree& T, const TCHAR* Name)
{ if (auto* W = T.FindWidget(Name)) W->SetVisibility(ESlateVisibility::Collapsed); }
void Height(UWidgetTree& T, const TCHAR* Name, float Value)
{ if (auto* W = Cast<USizeBox>(T.FindWidget(Name))) W->SetHeightOverride(Value); }
void Text(UWidgetTree& T, const TCHAR* Name, int32 Size, FLinearColor Tint = Ink)
{
	if (auto* W = Cast<UTextBlock>(T.FindWidget(Name)))
	{
		StyleText(*W, Size, Tint, TEXT("SemiBold"));
		W->SetAutoWrapText(false);
		W->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	}
}
void Button(UButton& W, bool Selected = false)
{
	auto S = W.GetStyle();
	S.Normal = Rounded(Selected ? Charcoal : Color(250,250,255,.65f), 24.f);
	S.Hovered = Rounded(Color(221,236,251,.95f), 24.f, Cyan, 1.f);
	S.Pressed = Rounded(Charcoal,24.f,Cyan,1.f);
	S.Disabled = Rounded(Color(226,228,238,.5f),24.f);
	S.NormalPadding = S.PressedPadding = FMargin(14,4);
	W.SetStyle(S);
}
void Pad(UWidget* W, FMargin P)
{ if (auto* S = W ? Cast<UVerticalBoxSlot>(W->Slot) : nullptr) S->SetPadding(P); }

bool Panel(UWidgetBlueprint& BP)
{
	auto& T = *BP.WidgetTree;
	auto* Tiles = Cast<UFF_PrepBuildTileView>(T.FindWidget(TEXT("TileView_Facilities")));
	auto* Tray = Cast<UBorder>(T.FindWidget(TEXT("Border_PanelRoot")));
	auto* Row = Cast<UHorizontalBox>(T.FindWidget(TEXT("HorizontalBox_SubcategoryHeader")));
	if (!Tiles || !Tray || !Row) return false;
	BP.Modify(); T.Modify();
	Tray->SetBrush(MaterialBrush(TEXT("Panel")));
	Tray->SetBrushColor(FLinearColor::White);
	Tray->SetPadding(FMargin(8,6,8,6));
	if (auto* Outer = Cast<UBorder>(T.FindWidget(TEXT("Border_ApprovedCatalogV9"))))
	{
		Outer->SetBrush(Rounded(FLinearColor::Transparent)); Outer->SetPadding(FMargin(0));
	}
	for (const TCHAR* N : {TEXT("Size_CatalogHeader"),TEXT("Size_HeaderDivider"),TEXT("Border_CatalogFooter"),
		TEXT("Border_AuroraSelection"),TEXT("HorizontalBox_Tools"),TEXT("Size_CategoryDivider")}) Hide(T,N);
	Height(T,TEXT("Size_ClarityCatalogDock"),324.f);
	Height(T,TEXT("Size_CategoryRow"),44.f);
	Height(T,TEXT("Size_SubcategoryHeader"),38.f);
	Height(T,TEXT("Size_SubcategoryViewport"),36.f);
	Pad(T.FindWidget(TEXT("Size_CategoryRow")),FMargin(0,0,0,5));
	Pad(T.FindWidget(TEXT("Size_SubcategoryHeader")),FMargin(0,0,0,5));
	Pad(T.FindWidget(TEXT("VerticalBox_CatalogBody")),FMargin(10,0,10,0));
	Tiles->ConfigureHorizontalRibbon();
	Tiles->SetScrollbarVisibility(ESlateVisibility::Collapsed);
	if (auto* Search = Cast<UEditableTextBox>(T.FindWidget(TEXT("Search_Facilities"))))
	{
		Search->RemoveFromParent();
		USizeBox* Bounds = Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlSearch")));
		if (!Bounds) Bounds = MakeWidget<USizeBox>(BP,T,TEXT("Size_PearlSearch"));
		Bounds->SetWidthOverride(192); Bounds->SetHeightOverride(34); Bounds->AddChild(Search);
		if (Bounds->GetParent()!=Row) Row->AddChildToHorizontalBox(Bounds)->SetPadding(FMargin(8,0,0,0));
		Search->SetMinDesiredWidth(160);
		Search->SetHintText(NSLOCTEXT("BuildPearl","Search","시설 검색"));
		auto S=Search->GetWidgetStyle();
		S.BackgroundImageNormal=Rounded(Color(245,247,253,.55f),18);
		S.BackgroundImageHovered=Rounded(Pearl,18);
		S.BackgroundImageFocused=Rounded(Pearl,18,Cyan,1);
		S.ForegroundColor=Ink; S.TextStyle.Font.Size=13; S.Padding=FMargin(12,4);
		Search->SetWidgetStyle(S);
	}
	if (auto* Ranking = Cast<UButton>(T.FindWidget(TEXT("Button_RevenueRanking"))))
	{
		Ranking->RemoveFromParent();
		auto* Bounds = Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlRevenueRanking")));
		if (!Bounds) Bounds = MakeWidget<USizeBox>(BP,T,TEXT("Size_PearlRevenueRanking"));
		Bounds->SetWidthOverride(138.f); Bounds->SetHeightOverride(34.f);
		Bounds->AddChild(Ranking);
		if (Bounds->GetParent()!=Row) Row->AddChildToHorizontalBox(Bounds)->SetPadding(FMargin(8,0,0,0));
		Ranking->SetVisibility(ESlateVisibility::Visible);
		Button(*Ranking);
		Text(T,TEXT("Text_RevenueRanking"),16);
	}
	for (const TCHAR* N : {TEXT("Button_FilterAll"),TEXT("Button_FilterBooth"),TEXT("Button_FilterPerformance"),
		TEXT("Button_FavoritesOnly"),TEXT("Button_ResetFilters")})
		if(auto* W=Cast<UButton>(T.FindWidget(N))) Button(*W,FString(N)==TEXT("Button_FilterAll"));
	for (const TCHAR* N : {TEXT("Text_FavoritesOnly"),TEXT("Text_FavoriteCount"),TEXT("Text_MinorCategory"),TEXT("Text_ItemCount")}) Text(T,N,14);
	if(auto* W=Cast<UBorder>(T.FindWidget(TEXT("Border_FavoriteCount")))) W->SetBrush(Rounded(Color(229,232,247,.7f),12));
	if(auto* W=Cast<UScrollBox>(T.FindWidget(TEXT("ScrollBox_Categories")))) W->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	if (!SaveNewBlueprint(Prefix,BP)) return false;
	auto* CDO=BP.GeneratedClass->GetDefaultObject();
	FindFProperty<FBoolProperty>(BP.GeneratedClass,TEXT("bUsePagination"))->SetPropertyValue_InContainer(CDO,false);
	FindFProperty<FFloatProperty>(BP.GeneratedClass,TEXT("CatalogHeight"))->SetPropertyValue_InContainer(CDO,324.f);
	return SaveAssetPackage(Prefix,BP);
}

bool Tile(UWidgetBlueprint& BP)
{
	auto& T=*BP.WidgetTree;
	auto* Card=Cast<UBorder>(T.FindWidget(TEXT("Border_Card")));
	auto* Thumb=Cast<UBorder>(T.FindWidget(TEXT("Border_ThumbnailBackdrop")));
	if(!Card||!Thumb) return false;
	BP.Modify(); T.Modify();
	Height(T,TEXT("SizeBox_Card"),210);
	Height(T,TEXT("SizeBox_Thumbnail"),104);
	if (auto* ThumbnailSize = Cast<USizeBox>(T.FindWidget(TEXT("SizeBox_Thumbnail"))))
		ThumbnailSize->SetWidthOverride(162.f);
	Card->SetBrush(Rounded(Color(247,248,255,.96f),18,Color(255,255,255,.75f),1));
	Card->SetPadding(FMargin(6,5,6,4));
	Card->SetBrushColor(FLinearColor::White);
	Thumb->SetBrush(MaterialBrush(TEXT("Card"))); Thumb->SetBrushColor(FLinearColor::White);
	Hide(T,TEXT("Text_CategoryPath")); Hide(T,TEXT("Text_BuildLimit"));
	Text(T,TEXT("Text_Name"),15); Text(T,TEXT("Text_Cost"),15);
	if (auto* Cost = Cast<UTextBlock>(T.FindWidget(TEXT("Text_Cost"))))
	{
		Cost->SetLineHeightPercentage(1.f);
		Cost->SetJustification(ETextJustify::Left);
		if (auto* Slot = Cast<UBorderSlot>(Cost->Slot)) Slot->SetPadding(FMargin(0));
	}
	if (auto* Price = Cast<UBorder>(T.FindWidget(TEXT("Border_AuroraCardGold"))))
	{
		Price->SetPadding(FMargin(0));
		Price->SetHorizontalAlignment(HAlign_Left);
		if (auto* Slot = Cast<UHorizontalBoxSlot>(Price->Slot))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0));
		}
	}
	Text(T,TEXT("Text_State"),10,Muted); Text(T,TEXT("Text_Favorite"),17,Muted);
	for (const TPair<const TCHAR*, const TCHAR*>& Badge : {
		TPair<const TCHAR*, const TCHAR*>(TEXT("Text_RequiredMissing"), TEXT("필수")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("Text_RequiredSatisfied"), TEXT("완료")),
		TPair<const TCHAR*, const TCHAR*>(TEXT("Text_ProgramBadge"), TEXT("연계"))})
	{
		if (auto* Label = Cast<UTextBlock>(T.FindWidget(Badge.Key)))
		{
			Label->SetText(FText::FromString(Badge.Value));
			StyleText(*Label, 10, FString(Badge.Key) == TEXT("Text_RequiredSatisfied") ? Ink : FLinearColor::White,
				TEXT("SemiBold"));
		}
	}
	for (const TCHAR* BadgeName : {TEXT("Border_RequiredMissing"), TEXT("Border_RequiredSatisfied"),
		TEXT("Border_ProgramBadge")})
		if (auto* Badge = Cast<UBorder>(T.FindWidget(BadgeName))) Badge->SetPadding(FMargin(4.f, 2.f));
	Pad(T.FindWidget(TEXT("Text_Name")),FMargin(0,3,0,0));
	Pad(T.FindWidget(TEXT("HorizontalBox_Meta")),FMargin(0));
	if(auto* W=Cast<UTextBlock>(T.FindWidget(TEXT("Text_Name"))))
	{
		W->SetJustification(ETextJustify::Center);
		W->SetAutoWrapText(true);
		W->SetLineHeightPercentage(1.f);
		W->SetWrapTextAt(158.f);
		W->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
		W->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
		auto* NameBounds=Cast<USizeBox>(T.FindWidget(TEXT("Size_PearlFacilityName")));
		if(!NameBounds)
		{
			auto* Column=Cast<UVerticalBox>(W->GetParent());
			if(!Column) return false;
			const int32 Index=Column->GetChildIndex(W);
			W->RemoveFromParent();
			NameBounds=MakeWidget<USizeBox>(BP,T,TEXT("Size_PearlFacilityName"));
			NameBounds->AddChild(W);Column->InsertChildAt(Index,NameBounds);
		}
		NameBounds->ClearHeightOverride();
		NameBounds->SetMinDesiredHeight(56.f);
		if(auto* S=Cast<USizeBoxSlot>(W->Slot))
		{S->SetHorizontalAlignment(HAlign_Fill);S->SetVerticalAlignment(VAlign_Center);}
	}
	if(auto* State=T.FindWidget(TEXT("Text_State")))
	{
		State->RemoveFromParent();
		auto* O=Cast<UOverlay>(T.FindWidget(TEXT("Overlay_Thumbnail")));
		auto* S=O->AddChildToOverlay(State); S->SetVerticalAlignment(VAlign_Bottom); S->SetHorizontalAlignment(HAlign_Center);
	}
	if(!SaveNewBlueprint(Prefix,BP)) return false;
	auto* CDO=BP.GeneratedClass->GetDefaultObject();
	for(const TPair<FName,FLinearColor>& Value : {TPair<FName,FLinearColor>(TEXT("HoverTint"),Color(232,244,255)),{TEXT("SelectedTint"),Color(210,238,255)}})
		*FindFProperty<FStructProperty>(BP.GeneratedClass,Value.Key)->ContainerPtrToValuePtr<FLinearColor>(CDO)=Value.Value;
	return SaveAssetPackage(Prefix,BP);
}

void Apply()
{
	auto* P=LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
	auto* T=LoadExistingBlueprint(FFBuildUIAssetPaths::BuildTilePackage);
	auto* C=LoadExistingBlueprint(FFBuildUIAssetPaths::CategoryChoicePackage);
	if(!P||!T||!C||!Panel(*P)||!Tile(*T)) { UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_BUILD_PEARL_CATALOG_FAIL"));return; }
	C->Modify(); C->WidgetTree->Modify();
	if(auto* B=Cast<UButton>(C->WidgetTree->FindWidget(TEXT("Button_Root")))) Button(*B);
	Text(*C->WidgetTree,TEXT("Text_Label"),16);
	Height(*C->WidgetTree,TEXT("Size_Choice"),38);
	if(SaveNewBlueprint(Prefix,*C))
	{
		auto* CDO=C->GeneratedClass->GetDefaultObject();
		auto* Normal=FindFProperty<FStructProperty>(C->GeneratedClass,TEXT("NormalContentColor"));
		auto* Selected=FindFProperty<FStructProperty>(C->GeneratedClass,TEXT("SelectedContentColor"));
		if(Normal&&Selected)
		{
			*Normal->ContainerPtrToValuePtr<FLinearColor>(CDO)=Ink;
			*Selected->ContainerPtrToValuePtr<FLinearColor>(CDO)=FLinearColor::White;
		}
		if(Normal&&Selected&&SaveAssetPackage(Prefix,*C))
		{
			UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_PEARL_CATALOG_PASS"));
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_BUILD_PEARL_CATALOG_CHOICE_COLORS_FAIL"));
		}
	}
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildPearlCatalog"),TEXT("Apply the horizontal pearl construction catalog."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
