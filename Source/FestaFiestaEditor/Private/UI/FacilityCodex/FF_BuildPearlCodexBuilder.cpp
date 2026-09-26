// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UObject/UnrealType.h"

namespace FFBuildPearlCodex
{
using namespace FFUIWbpBuilder;
using namespace FFBuildPearl;
const FString Folder=TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex");
constexpr const TCHAR* Prefix=TEXT("FF_BUILD_PEARL_CODEX");
constexpr const TCHAR* GoldTexturePath=TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Badges/T_FF_LH_Badge_Gold_256.T_FF_LH_Badge_Gold_256");
UTexture2D* GoldTexture(){return LoadObject<UTexture2D>(nullptr,GoldTexturePath);}
void PersistIconSize(UImage& Icon, float Size)
{
	// Builder widgets have no Slate SImage yet, so SetDesiredSizeOverride is a
	// no-op here. Persist the intended dimensions in the authored brush.
	FSlateBrush Brush=Icon.GetBrush();
	Brush.ImageSize=FVector2D(Size,Size);
	Icon.SetBrush(Brush);
}
void StyleGoldIcon(UImage& Icon, UTexture2D& Texture, float Size)
{
	Icon.SetBrushFromTexture(&Texture,false);
	PersistIconSize(Icon,Size);
}
template<class T> T* Get(UWidgetTree& Tree,const TCHAR* Name){return Cast<T>(Tree.FindWidget(Name));}
template<class T> T* Ensure(UWidgetBlueprint& BP,const TCHAR* Name,bool Variable=false)
{if(auto* W=Get<T>(*BP.WidgetTree,Name))return W;return MakeWidget<T>(BP,*BP.WidgetTree,Name,Variable);}
void Hide(UWidgetTree& T,const TCHAR* N){if(auto* W=T.FindWidget(N))W->SetVisibility(ESlateVisibility::Collapsed);}
void Label(UWidgetTree& T,const TCHAR* N,int32 Size,FLinearColor Tint=Ink)
{if(auto* W=Get<UTextBlock>(T,N))StyleText(*W,Size,Tint,TEXT("SemiBold"));}
void StyleButton(UButton& B,bool Primary=false)
{
	auto S=B.GetStyle();
	S.Normal=Rounded(Primary?Charcoal:Color(250,251,255,.70f),24);
	S.Hovered=Rounded(Color(221,239,255,.95f),24,Cyan,1);
	S.Pressed=Rounded(Charcoal,24,Cyan,1);
	S.Disabled=Rounded(Charcoal,24,Cyan,1);
	S.NormalPadding=S.PressedPadding=FMargin(14,5);B.SetStyle(S);
}
void StyleMilestoneButton(UButton& B)
{
	auto S=B.GetStyle();
	// The dark reward tiles sit in front of the discovery track.
	S.Normal=Rounded(Color(37,47,62),14,Color(91,111,131),1);
	S.Hovered=Rounded(Color(46,65,81),14,Cyan,1);
	S.Pressed=Rounded(Color(29,78,94),14,Cyan,1);
	S.Disabled=S.Normal;
	S.NormalPadding=S.PressedPadding=FMargin(0);B.SetStyle(S);
}
bool UpgradeMilestone(UWidgetBlueprint& BP,UTexture2D& Gold)
{
	auto& T=*BP.WidgetTree;
	auto* B=Get<UButton>(T,TEXT("Button_Root"));
	auto* Label=Get<UTextBlock>(T,TEXT("Text_Label"));
	auto* Size=Get<USizeBox>(T,TEXT("Size_Choice"));
	if(!B||!Label||!Size)return false;
	// Cache the named children before detaching the old vertical card layout.
	auto* RetiredStack=Get<UVerticalBox>(T,TEXT("Vertical_MilestoneCard"));
	auto* RetiredRow=Get<UHorizontalBox>(T,TEXT("Row_MilestoneGold"));
	auto* Icon=Get<UImage>(T,TEXT("Image_MilestoneGold"));
	auto* IconBox=Get<USizeBox>(T,TEXT("Size_MilestoneGold"));
	auto* Amount=Get<UTextBlock>(T,TEXT("Text_MilestoneAmount"));
	auto* Badge=Get<UBorder>(T,TEXT("Border_MilestoneStatus"));
	auto* Status=Get<UTextBlock>(T,TEXT("Text_MilestoneStatus"));
	if(!Icon)Icon=MakeWidget<UImage>(BP,T,TEXT("Image_MilestoneGold"),true);
	if(!Amount)Amount=MakeWidget<UTextBlock>(BP,T,TEXT("Text_MilestoneAmount"),true);
	if(!Badge)Badge=MakeWidget<UBorder>(BP,T,TEXT("Border_MilestoneStatus"),true);
	if(!Status)Status=MakeWidget<UTextBlock>(BP,T,TEXT("Text_MilestoneStatus"),true);
	Icon->bIsVariable=true;Amount->bIsVariable=true;Badge->bIsVariable=true;Status->bIsVariable=true;
	if(Status->GetParent()!=Badge){Status->RemoveFromParent();Badge->AddChild(Status);}

	auto* Root=Get<UVerticalBox>(T,TEXT("Vertical_MilestoneRoot"));
	if(!Root)Root=MakeWidget<UVerticalBox>(BP,T,TEXT("Vertical_MilestoneRoot"));
	if(Size->GetContent()!=Root)
	{
		Root->RemoveFromParent();B->RemoveFromParent();Size->ClearChildren();Size->AddChild(Root);
	}
	if(Label->GetParent()!=Root){Label->RemoveFromParent();Root->AddChildToVerticalBox(Label);}
	auto* Tile=Get<USizeBox>(T,TEXT("Size_MilestoneTile"));
	if(!Tile)Tile=MakeWidget<USizeBox>(BP,T,TEXT("Size_MilestoneTile"));
	if(Tile->GetParent()!=Root){Tile->RemoveFromParent();Root->AddChildToVerticalBox(Tile);}
	if(B->GetParent()!=Tile){B->RemoveFromParent();Tile->ClearChildren();Tile->AddChild(B);}
	Root->ShiftChild(0,Label);Root->ShiftChild(1,Tile);
	Size->SetWidthOverride(100);Size->SetHeightOverride(116);
	Tile->SetWidthOverride(100);Tile->SetHeightOverride(96);
	if(auto* Slot=Cast<UVerticalBoxSlot>(Label->Slot))
	{Slot->SetPadding(FMargin(3,0,0,0));Slot->SetHorizontalAlignment(HAlign_Left);}
	if(auto* Slot=Cast<UVerticalBoxSlot>(Tile->Slot))
	{Slot->SetPadding(FMargin(0));Slot->SetHorizontalAlignment(HAlign_Fill);}
	StyleText(*Label,14,Ink,TEXT("Bold"));Label->SetText(NSLOCTEXT("PearlCodex","MilestoneThresholdNumber","10"));
	Label->SetJustification(ETextJustify::Left);Label->SetAutoWrapText(false);

	auto* Overlay=Get<UOverlay>(T,TEXT("Overlay_MilestoneTile"));
	if(!Overlay)Overlay=MakeWidget<UOverlay>(BP,T,TEXT("Overlay_MilestoneTile"));
	if(B->GetContent()!=Overlay)
	{Icon->RemoveFromParent();Amount->RemoveFromParent();Badge->RemoveFromParent();B->ClearChildren();B->AddChild(Overlay);}
	Overlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	if(auto* Slot=Cast<UButtonSlot>(Overlay->Slot))
	{Slot->SetPadding(FMargin(0));Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);}
	if(!IconBox)IconBox=MakeWidget<USizeBox>(BP,T,TEXT("Size_MilestoneGold"));
	if(IconBox->GetParent()!=Overlay){IconBox->RemoveFromParent();Overlay->AddChildToOverlay(IconBox);}
	if(Icon->GetParent()!=IconBox){Icon->RemoveFromParent();IconBox->ClearChildren();IconBox->AddChild(Icon);}
	IconBox->SetWidthOverride(62);IconBox->SetHeightOverride(62);
	if(auto* Slot=Cast<UOverlaySlot>(IconBox->Slot))
	{Slot->SetHorizontalAlignment(HAlign_Center);Slot->SetVerticalAlignment(VAlign_Center);Slot->SetPadding(FMargin(0));}
	StyleGoldIcon(*Icon,Gold,62.f);Icon->SetVisibility(ESlateVisibility::HitTestInvisible);

	if(Amount->GetParent()!=Overlay){Amount->RemoveFromParent();Overlay->AddChildToOverlay(Amount);}
	StyleText(*Amount,20,FLinearColor::White,TEXT("Bold"));
	Amount->SetText(NSLOCTEXT("PearlCodex","MilestoneAmountCompact","500"));
	Amount->SetJustification(ETextJustify::Right);Amount->SetAutoWrapText(false);
	Amount->SetShadowColorAndOpacity(FLinearColor(0.f,0.f,0.f,.85f));Amount->SetShadowOffset(FVector2D(1.f,1.f));
	Amount->SetVisibility(ESlateVisibility::HitTestInvisible);
	if(auto* Slot=Cast<UOverlaySlot>(Amount->Slot))
	{Slot->SetHorizontalAlignment(HAlign_Right);Slot->SetVerticalAlignment(VAlign_Bottom);Slot->SetPadding(FMargin(0,0,6,5));}

	if(Badge->GetParent()!=Overlay){Badge->RemoveFromParent();Overlay->AddChildToOverlay(Badge);}
	Badge->SetBrush(Rounded(Color(221,240,246),10,Cyan,1));Badge->SetBrushColor(FLinearColor::White);
	Badge->SetPadding(FMargin(5,1));Badge->SetVisibility(ESlateVisibility::Collapsed);
	if(auto* Slot=Cast<UOverlaySlot>(Badge->Slot))
	{Slot->SetHorizontalAlignment(HAlign_Right);Slot->SetVerticalAlignment(VAlign_Top);Slot->SetPadding(FMargin(0,5,5,0));}
	StyleText(*Status,11,Color(29,78,94),TEXT("Bold"));
	Status->SetText(NSLOCTEXT("PearlCodex","MilestoneCheck","✓"));
	Status->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Keep the old designer containers attached but collapsed. Their saved GUIDs
	// remain in the WBP, and detached widgets fail the builder's orphan check.
	if(RetiredStack)
	{
		if(RetiredRow&&RetiredRow->GetParent()!=RetiredStack)
		{RetiredRow->RemoveFromParent();RetiredStack->AddChildToVerticalBox(RetiredRow);}
		if(RetiredStack->GetParent()!=Root)
		{RetiredStack->RemoveFromParent();Root->AddChildToVerticalBox(RetiredStack);}
		RetiredStack->SetVisibility(ESlateVisibility::Collapsed);
	}
	StyleMilestoneButton(*B);
	return true;
}
UWidgetBlueprint* Choice(bool Milestone)
{
	const FString Name=Milestone?TEXT("WBP_FF_CodexCollectionReward"):TEXT("WBP_FF_CodexCollectionCategory");
	auto* BP=EnsureWidgetBlueprint(Prefix,Folder/TEXT("Controls")/Name,Folder/TEXT("Controls"),*UFF_PrepChoiceButton::StaticClass(),
		[Milestone](UWidgetBlueprint& BP,UWidgetTree& T)
		{
			auto* Size=MakeWidget<USizeBox>(BP,T,TEXT("Size_Choice")); Size->SetHeightOverride(Milestone?100:48);
			auto* B=MakeWidget<UButton>(BP,T,TEXT("Button_Root"),true);StyleButton(*B);
			auto* L=MakeWidget<UTextBlock>(BP,T,TEXT("Text_Label"),true);StyleText(*L,Milestone?15:17,Ink,TEXT("SemiBold"));
			L->SetText(FText::FromString(Milestone?TEXT("10개 발견"):TEXT("전체 조합")));
			L->SetJustification(Milestone?ETextJustify::Center:ETextJustify::Left);
			L->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Slot=Cast<UButtonSlot>(B->AddChild(L));Slot->SetHorizontalAlignment(Milestone?HAlign_Center:HAlign_Left);Slot->SetVerticalAlignment(VAlign_Center);
			Size->AddChild(B);T.RootWidget=Size;return true;
		});
	if(!BP||!BP->GeneratedClass)return nullptr;
	BP->Modify();BP->WidgetTree->Modify();
	if(auto* B=Get<UButton>(*BP->WidgetTree,TEXT("Button_Root")))StyleButton(*B);
	if(auto* Size=Get<USizeBox>(*BP->WidgetTree,TEXT("Size_Choice")))
	{
			Size->SetHeightOverride(Milestone?116:48);
			if(Milestone)Size->SetWidthOverride(100);
	}
	if(Milestone)
	{
		UTexture2D* Gold=GoldTexture();
		if(!Gold||!UpgradeMilestone(*BP,*Gold))return nullptr;
	}
	if(!SaveNewBlueprint(Prefix,*BP))return nullptr;
	auto* CDO=BP->GeneratedClass->GetDefaultObject();
	auto* Normal=FindFProperty<FStructProperty>(BP->GeneratedClass,TEXT("NormalContentColor"));
	auto* Selected=FindFProperty<FStructProperty>(BP->GeneratedClass,TEXT("SelectedContentColor"));
	if(!Normal||!Selected)return nullptr;
	*Normal->ContainerPtrToValuePtr<FLinearColor>(CDO)=Ink;
	*Selected->ContainerPtrToValuePtr<FLinearColor>(CDO)=Milestone?Color(29,78,94):FLinearColor::White;
	return SaveAssetPackage(Prefix,*BP)?BP:nullptr;
}
void ToCanvas(UCanvasPanel& C,UWidget& W,FVector2D P,FVector2D S,bool Visible=true)
{
	W.RemoveFromParent();auto* Slot=C.AddChildToCanvas(&W);Slot->SetPosition(P);Slot->SetSize(S);
	W.SetVisibility(Visible?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
}
bool Screen(UWidgetBlueprint& BP,UClass* CategoryClass,UClass* RewardClass)
{
	auto& T=*BP.WidgetTree;
	auto* Canvas=Get<UCanvasPanel>(T,TEXT("Canvas_CodexLayout"));
	auto* Browse=Get<USizeBox>(T,TEXT("Size_Browse"));
	auto* Detail=Get<USizeBox>(T,TEXT("Size_Detail"));
	auto* Hero=Get<USizeBox>(T,TEXT("Size_Showcase"));
	auto* Toolbar=Get<UHorizontalBox>(T,TEXT("Horizontal_Toolbar"));
	if(!Canvas||!Browse||!Detail||!Hero||!Toolbar)return false;
	BP.Modify();T.Modify();
	Get<UBorder>(T,TEXT("Border_Paper"))->SetBrush(MaterialBrush(TEXT("Sea")));
	Get<UBorder>(T,TEXT("Border_Paper"))->SetBrushColor(FLinearColor::White);
	Hide(T,TEXT("BackgroundBlur_Glass"));Hide(T,TEXT("Border_CodexDock"));
	Browse->ClearWidthOverride();Browse->ClearHeightOverride();
	Detail->ClearWidthOverride();Detail->ClearHeightOverride();Hero->ClearWidthOverride();Hero->ClearHeightOverride();
	ToCanvas(*Canvas,*Browse,{300,118},{1200,600});
	ToCanvas(*Canvas,*Detail,{1120,112},{580,700},false);
	ToCanvas(*Canvas,*Hero,{160,220},{820,600},false);
	if(auto* W=T.FindWidget(TEXT("Horizontal_PreviewRotation")))ToCanvas(*Canvas,*W,{400,830},{500,40},false);
	// The list page owns filters and collection progress. Its detail page is reached only by activation.
	if(auto* Search=T.FindWidget(TEXT("Size_Refined_Search_Entries")))
	{
		Search->RemoveFromParent();
		auto* Bounds=Ensure<USizeBox>(BP,TEXT("Size_CollectionSearch"));Bounds->AddChild(Search);
		ToCanvas(*Canvas,*Bounds,{48,84},{210,36});
	}
	ToCanvas(*Canvas,*Toolbar,{820,26},{900,44});
	for (UWidget* Child : Toolbar->GetAllChildren())
	{
		if (auto* Bounds=Cast<USizeBox>(Child)) Bounds->ClearWidthOverride();
		if (auto* Slot=Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(4,0));
		}
	}
	auto* Scroll=Ensure<UScrollBox>(BP,TEXT("Scroll_CollectionCategories"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);Scroll->SetAnimateWheelScrolling(true);
	auto* Categories=Ensure<UVerticalBox>(BP,TEXT("Column_CollectionCategories"),true);
	if(Categories->GetParent()!=Scroll){Categories->RemoveFromParent();Scroll->AddChild(Categories);}
	ToCanvas(*Canvas,*Scroll,{48,136},{210,760});
	auto* ProgressRow=Ensure<UHorizontalBox>(BP,TEXT("Row_CollectionProgress"));
	auto* ProgressSize=Ensure<USizeBox>(BP,TEXT("Size_SeaCollectionProgress"));ProgressSize->SetWidthOverride(170);
	auto* ProgressColumn=Ensure<UVerticalBox>(BP,TEXT("Column_SeaCollectionProgress"));
	if(!ProgressSize->GetContent())ProgressSize->AddChild(ProgressColumn);
	if(!ProgressColumn->GetChildrenCount())
	{
		auto* Heading=MakeWidget<UTextBlock>(BP,T,TEXT("Label_CollectionProgress"));
		StyleText(*Heading,16,Ink,TEXT("SemiBold"));Heading->SetText(NSLOCTEXT("SeaCodex","Progress","수집 진행도"));
		ProgressColumn->AddChildToVerticalBox(Heading);
		auto* Count=MakeWidget<UTextBlock>(BP,T,TEXT("Text_CollectionCount"),true);StyleText(*Count,24,Ink,TEXT("Bold"));
		ProgressColumn->AddChildToVerticalBox(Count)->SetPadding(FMargin(0,4,0,8));
		auto* Bar=T.FindWidget(TEXT("Size_DiscoveryProgress"));Bar->RemoveFromParent();ProgressColumn->AddChildToVerticalBox(Bar);
	}
	if(ProgressSize->GetParent()!=ProgressRow)ProgressRow->AddChildToHorizontalBox(ProgressSize);
	auto* Milestones=Ensure<UHorizontalBox>(BP,TEXT("Row_CollectionMilestones"),true);
	auto* ProgressTrack=Ensure<UOverlay>(BP,TEXT("Overlay_CollectionProgressTrack"));
	if(ProgressTrack->GetParent()!=ProgressRow)
	{
		auto* Slot=ProgressRow->AddChildToHorizontalBox(ProgressTrack);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetVerticalAlignment(VAlign_Center);
	}
	if(auto* Bar=T.FindWidget(TEXT("Size_DiscoveryProgress")))
	{
		Bar->RemoveFromParent();
		auto* Slot=ProgressTrack->AddChildToOverlay(Bar);Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Center);Slot->SetPadding(FMargin(0,16,0,0));
	}
	Milestones->RemoveFromParent();
	auto* MilestoneOverlaySlot=ProgressTrack->AddChildToOverlay(Milestones);
	MilestoneOverlaySlot->SetHorizontalAlignment(HAlign_Fill);MilestoneOverlaySlot->SetVerticalAlignment(VAlign_Center);
	ToCanvas(*Canvas,*ProgressRow,{300,932},{1300,120});
	if(auto* Bar=Get<UProgressBar>(T,TEXT("Progress_Discovery")))
	{
		auto S=Bar->GetWidgetStyle();S.BackgroundImage=Rounded(Color(135,150,172,.65f),2);S.FillImage=Rounded(FLinearColor::White,2);Bar->SetWidgetStyle(S);
		Bar->SetFillColorAndOpacity(Color(255,176,98));
	}
	for(const TCHAR* Name:{TEXT("Button_All"),TEXT("Button_Discovered"),TEXT("Button_Undiscovered"),TEXT("Button_Active"),TEXT("Button_RewardReady"),
		TEXT("Button_TargetCatalog"),TEXT("Button_PartnerCatalog"),TEXT("Button_DetailRecipe"),TEXT("Button_DetailPlacement"),TEXT("Button_DetailEffect"),
		TEXT("Button_PreviewTarget"),TEXT("Button_PreviewPartner"),TEXT("Button_RotatePreviewLeft"),TEXT("Button_RotatePreviewRight"),TEXT("Button_ResetPreview")})
		if(auto* B=Get<UButton>(T,Name))StyleButton(*B);
	if(auto* Search=Get<UEditableTextBox>(T,TEXT("Search_Entries")))
	{
		auto S=Search->GetWidgetStyle();S.BackgroundImageNormal=Rounded(Color(245,251,255,.54f),20);
		S.BackgroundImageHovered=Rounded(Pearl,20);S.BackgroundImageFocused=Rounded(Pearl,20,Cyan,1);S.ForegroundColor=Ink;S.Padding=FMargin(10,4);S.TextStyle.Font.Size=14;Search->SetWidgetStyle(S);
	}
	for(const TCHAR* Name:{TEXT("Panel_Detail"),TEXT("Border_Showcase")})
		if(auto* B=Get<UBorder>(T,Name)){B->SetBrush(Rounded(FLinearColor::Transparent));B->SetPadding(FMargin(0));B->SetBrushColor(FLinearColor::White);}
	if(auto* RewardPanel=Get<UBorder>(T,TEXT("Border_ShowcaseRewardInfo")))
	{
		RewardPanel->SetBrush(Rounded(FLinearColor::Transparent));
		RewardPanel->SetBrushColor(FLinearColor::White);
		RewardPanel->SetPadding(FMargin(0));
	}
	// The reward follows the combination description in the detail scroll content.
	// The earlier fixed footer placed the action at the bottom of a tall empty gap.
	if(auto* Reward=Get<USizeBox>(T,TEXT("Size_ShowcaseRewardInfo")))
		if(auto* ScrollColumn=Get<UVerticalBox>(T,TEXT("Vertical_ShowcaseScroll")))
		{
			Reward->RemoveFromParent();Reward->ClearWidthOverride();
			auto* Slot=ScrollColumn->AddChildToVerticalBox(Reward);
			Slot->SetPadding(FMargin(0,16,0,0));Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	if(auto* B=Get<UBorder>(T,TEXT("Border_Bonus")))B->SetBrush(Rounded(Color(241,247,255,.6f),18));
	for(const TCHAR* Name:{TEXT("Text_DetailTitle"),TEXT("Text_PreviewFacilityName")})Label(T,Name,27);
	for(const TCHAR* Name:{TEXT("Text_TargetName"),TEXT("Text_PartnerName"),TEXT("Text_Bonus"),TEXT("Text_Distance"),TEXT("Text_Condition"),TEXT("Text_Applied")})Label(T,Name,17);
	Label(T,TEXT("Text_ShowcaseDistance"),14);
	if(auto* Heading=Get<UTextBlock>(T,TEXT("Text_DetailRewardLabel")))
	{
		StyleText(*Heading,18,Ink,TEXT("Bold"));
		Heading->SetText(NSLOCTEXT("PearlCodex","DiscoveryRewardHeading","발견 보상"));
		if(auto* Slot=Cast<UVerticalBoxSlot>(Heading->Slot))Slot->SetPadding(FMargin(0,0,0,9));
	}
	Hide(T,TEXT("Text_ShowcaseDistance"));
	Hide(T,TEXT("Text_DetailRewardHint"));
	Hide(T,TEXT("Row_DetailRewardGold"));
	Hide(T,TEXT("Text_DetailRewardAmount"));
	Hide(T,TEXT("Image_DetailRewardGold"));
	if(auto* RewardColumn=Get<UVerticalBox>(T,TEXT("Vertical_ShowcaseRewardInfo")))
		if(auto* Amount=Get<UTextBlock>(T,TEXT("Text_DetailRewardAmount")))
		{
			if(!Get<UImage>(T,TEXT("Image_DetailRewardGold")) && Amount->GetParent()==RewardColumn)
			{
				const int32 Index=RewardColumn->GetChildIndex(Amount);
				Amount->RemoveFromParent();
				auto* Row=MakeWidget<UHorizontalBox>(BP,T,TEXT("Row_DetailRewardGold"));
				Row->SetVisibility(ESlateVisibility::HitTestInvisible);
				auto* Icon=MakeWidget<UImage>(BP,T,TEXT("Image_DetailRewardGold"),true);
				StyleGoldIcon(*Icon,*GoldTexture(),28.f);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
				auto* IconSlot=Row->AddChildToHorizontalBox(Icon);
				IconSlot->SetPadding(FMargin(0,0,7,0));IconSlot->SetVerticalAlignment(VAlign_Center);
				Row->AddChildToHorizontalBox(Amount)->SetVerticalAlignment(VAlign_Center);
				RewardColumn->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,10));
				RewardColumn->ShiftChild(Index,Row);
			}
			else if(auto* Icon=Get<UImage>(T,TEXT("Image_DetailRewardGold")))
			{
				StyleGoldIcon(*Icon,*GoldTexture(),28.f);
			}
		}
	Hide(T,TEXT("Row_DetailRewardGold"));
	Hide(T,TEXT("Text_DetailRewardAmount"));
	Hide(T,TEXT("Image_DetailRewardGold"));
	if(auto* Bounds=Get<USizeBox>(T,TEXT("Size_DetailRewardState")))
	{
		Bounds->ClearWidthOverride();
		Bounds->SetHeightOverride(72.f);
		if(auto* Slot=Cast<UVerticalBoxSlot>(Bounds->Slot))
		{Slot->SetPadding(FMargin(0));Slot->SetHorizontalAlignment(HAlign_Fill);}
	}
	if(auto* B=Get<UButton>(T,TEXT("Button_DetailClaimReward")))
	{
		StyleButton(*B);
		auto S=B->GetStyle();
		S.Normal=Rounded(Color(248,251,255),16,Line,1);
		S.Hovered=Rounded(Color(255,255,255),16,Cyan,1);
		S.Pressed=Rounded(Color(224,243,247),16,Cyan,1);
		S.Disabled=S.Normal;
		S.NormalPadding=S.PressedPadding=FMargin(0);
		B->SetStyle(S);
		if(UWidget* Content=B->GetContent())
			if(auto* Slot=Cast<UButtonSlot>(Content->Slot))
			{Slot->SetPadding(FMargin(16,10));Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Center);}
	}
	for(const TCHAR* Suffix:{TEXT("ClaimReward"),TEXT("RewardClaimed"),TEXT("RewardLocked")})
	{
		const FString IconName=FString(TEXT("Image_Detail"))+Suffix;
		const FString TextName=FString(TEXT("Text_Detail"))+Suffix;
		if(auto* Icon=Get<UImage>(T,*IconName))
		{
			StyleGoldIcon(*Icon,*GoldTexture(),32.f);
			Icon->SetColorAndOpacity(FLinearColor(1.f,1.f,1.f,FCString::Strcmp(Suffix,TEXT("RewardLocked"))==0?.55f:1.f));
			if(auto* Slot=Cast<UHorizontalBoxSlot>(Icon->Slot))
			{Slot->SetPadding(FMargin(0,0,12,0));Slot->SetVerticalAlignment(VAlign_Center);}
		}
		if(auto* StateText=Get<UTextBlock>(T,*TextName))
		{
			StateText->bIsVariable=true;
			StyleText(*StateText,18,FCString::Strcmp(Suffix,TEXT("RewardLocked"))==0?Muted:Ink,TEXT("SemiBold"));
			StateText->SetAutoWrapText(false);
			if(auto* Slot=Cast<UHorizontalBoxSlot>(StateText->Slot))
			{Slot->SetPadding(FMargin(0));Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetVerticalAlignment(VAlign_Center);}
		}
	}
	if(auto* ClaimText=Get<UTextBlock>(T,TEXT("Text_DetailClaimReward")))
		ClaimText->SetText(NSLOCTEXT("PearlCodex","DetailClaimAmount","100 골드"));
	if(auto* ClaimedText=Get<UTextBlock>(T,TEXT("Text_DetailRewardClaimed")))
		ClaimedText->SetText(NSLOCTEXT("PearlCodex","DetailClaimedAmount","100 골드 · 수령 완료"));
	if(auto* LockedText=Get<UTextBlock>(T,TEXT("Text_DetailRewardLocked")))
		LockedText->SetText(NSLOCTEXT("PearlCodex","DetailLockedAmount","100 골드 · 미발견"));
	if(auto* ClaimRow=Get<UHorizontalBox>(T,TEXT("Horizontal_DetailClaimReward")))
	{
		auto* Chevron=Ensure<UTextBlock>(BP,TEXT("Text_DetailRewardChevron"));
		if(Chevron->GetParent()!=ClaimRow)
		{Chevron->RemoveFromParent();ClaimRow->AddChildToHorizontalBox(Chevron);}
		StyleText(*Chevron,27,Muted,TEXT("SemiBold"));
		Chevron->SetText(NSLOCTEXT("PearlCodex","DetailRewardChevron","›"));
		Chevron->SetVisibility(ESlateVisibility::HitTestInvisible);
		if(auto* Slot=Cast<UHorizontalBoxSlot>(Chevron->Slot))Slot->SetVerticalAlignment(VAlign_Center);
	}
	for(const TCHAR* Name:{TEXT("Border_DetailRewardClaimed"),TEXT("Border_DetailRewardLocked")})
		if(auto* B=Get<UBorder>(T,Name))
		{
			B->SetBrush(Rounded(Color(245,248,252),16,Line,1));
			B->SetBrushColor(FLinearColor::White);
			B->SetPadding(FMargin(16,10));
			B->SetHorizontalAlignment(HAlign_Fill);B->SetVerticalAlignment(VAlign_Center);
		}
	Label(T,TEXT("Text_DetailStatus"),16,Muted);Label(T,TEXT("Text_PreviewHint"),14,Muted);
	if(auto* Close=Get<UButton>(T,TEXT("Button_Close")))
	{
		StyleButton(*Close);auto S=Close->GetStyle();S.Normal=Rounded(FLinearColor::Transparent);Close->SetStyle(S);
		if(auto* L=Cast<UTextBlock>(Close->GetContent())){StyleText(*L,25,Ink,TEXT("Bold"));L->SetText(NSLOCTEXT("SeaCodex","Title","‹   시설 조합 도감"));}
	}
	for(const TCHAR* N:{TEXT("Button_CollectionPrevious"),TEXT("Button_CollectionNext")})
	{
		auto* B=Ensure<UButton>(BP,N,true);StyleButton(*B);
		if(!B->GetContent()){auto* L=MakeWidget<UTextBlock>(BP,T,FName(*(FString(N)+TEXT("Label"))));StyleText(*L,30,Ink,TEXT("Bold"));L->SetText(FText::FromString(FString(N).Contains(TEXT("Previous"))?TEXT("‹"):TEXT("›")));B->AddChild(L);}
		ToCanvas(*Canvas,*B,{0,400},{46,62},false);B->SetVisibility(ESlateVisibility::Collapsed);
	}
	if(auto* Tiles=Get<UFF_FacilityCodexTileView>(T,TEXT("TileView_Entries"))){Tiles->SetEntryHeight(276);Tiles->SetHorizontalEntrySpacing(14);Tiles->SetVerticalEntrySpacing(16);}
	if(!SaveNewBlueprint(Prefix,BP))return false;
	auto* CDO=BP.GeneratedClass->GetDefaultObject();
	FindFProperty<FClassProperty>(BP.GeneratedClass,TEXT("CollectionCategoryClass"))->SetObjectPropertyValue_InContainer(CDO,CategoryClass);
	FindFProperty<FClassProperty>(BP.GeneratedClass,TEXT("CollectionMilestoneClass"))->SetObjectPropertyValue_InContainer(CDO,RewardClass);
	*FindFProperty<FStructProperty>(BP.GeneratedClass,TEXT("InactiveFilterTextColor"))->ContainerPtrToValuePtr<FLinearColor>(CDO)=Ink;
	return SaveAssetPackage(Prefix,BP);
}

bool Card(UWidgetBlueprint& BP)
{
	auto& T=*BP.WidgetTree;BP.Modify();T.Modify();
	if(auto* B=Get<UBorder>(T,TEXT("Border_Card"))){B->SetBrush(MaterialBrush(TEXT("Stamp")));B->SetBrushColor(FLinearColor::White);B->SetPadding(FMargin(12,14,12,10));}
	if(auto* S=Get<USizeBox>(T,TEXT("Size_Card")))S->SetHeightOverride(260);
	for(const TCHAR* N:{TEXT("Size_ThumbTarget"),TEXT("Size_ThumbPartner")})if(auto* S=Get<USizeBox>(T,N))S->SetHeightOverride(98);
	for(const TCHAR* N:{TEXT("Border_ThumbTarget"),TEXT("Border_ThumbPartner")})if(auto* B=Get<UBorder>(T,N)){B->SetBrush(MaterialBrush(TEXT("Hero")));B->SetBrushColor(FLinearColor::White);}
	if(auto* S=Get<USizeBox>(T,TEXT("Size_Name")))S->SetHeightOverride(54);
	if(auto* Images=T.FindWidget(TEXT("Horizontal_RecipeImages")))
		if(auto* Column=Get<UVerticalBox>(T,TEXT("Vertical_Card")))
		{
			Images->RemoveFromParent();Column->InsertChildAt(0,Images);
		}
	Hide(T,TEXT("Size_Recipe"));
	if(auto* Actions=T.FindWidget(TEXT("Scale_RewardActions")))Actions->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Label(T,TEXT("Text_Name"),16);Label(T,TEXT("Text_State"),12,Ink);
	Label(T,TEXT("Text_Bonus"),13,Color(93,91,118));
	Label(T,TEXT("Text_ClaimReward"),11,Color(110,66,20));
	Label(T,TEXT("Text_Plus"),16,Ink);
	if(auto* L=Get<UTextBlock>(T,TEXT("Text_Name"))){L->SetJustification(ETextJustify::Center);L->SetAutoWrapText(true);}
	if(auto* B=Get<UBorder>(T,TEXT("Border_State")))
	{
		B->SetBrush(Rounded(Color(247,248,253,.92f),10,Line,1));
		B->SetBrushColor(FLinearColor::White);B->SetPadding(FMargin(7,2));
		if(auto* Slot=Cast<UVerticalBoxSlot>(B->Slot))Slot->SetHorizontalAlignment(HAlign_Fill);
		if(auto* State=Get<UTextBlock>(T,TEXT("Text_State")))
		{
			if(!Get<UImage>(T,TEXT("Image_CardRewardGold")) && B->GetContent()==State)
			{
				State->RemoveFromParent();
				auto* Row=MakeWidget<UHorizontalBox>(BP,T,TEXT("Row_CardRewardState"));
				Row->SetVisibility(ESlateVisibility::HitTestInvisible);B->AddChild(Row);
				auto* Icon=MakeWidget<UImage>(BP,T,TEXT("Image_CardRewardGold"),true);
				StyleGoldIcon(*Icon,*GoldTexture(),20.f);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
				auto* IconSlot=Row->AddChildToHorizontalBox(Icon);
				IconSlot->SetPadding(FMargin(0,0,5,0));IconSlot->SetVerticalAlignment(VAlign_Center);
				Row->AddChildToHorizontalBox(State)->SetVerticalAlignment(VAlign_Center);
			}
			else if(auto* Icon=Get<UImage>(T,TEXT("Image_CardRewardGold")))
			{
				StyleGoldIcon(*Icon,*GoldTexture(),20.f);
			}
		}
	}
	if(auto* B=Get<UButton>(T,TEXT("Button_ClaimReward")))
	{
		auto S=B->GetStyle();
		S.Normal=Rounded(Color(255,224,178,.98f),8,Color(232,168,82),1);
		S.Hovered=Rounded(Color(255,238,203,1),8,Color(232,168,82),1);
		S.Pressed=Rounded(Charcoal,8,Cyan,1);S.Disabled=Rounded(Pearl,8,Line,1);
		S.NormalPadding=S.PressedPadding=FMargin(5,3);B->SetStyle(S);
	}
	if(auto* B=Get<UBorder>(T,TEXT("Border_Selection")))B->SetBrush(MaterialBrush(TEXT("Rim")));
	if(!SaveNewBlueprint(Prefix,BP))return false;
	auto* CDO=BP.GeneratedClass->GetDefaultObject();
	for(const TPair<FName,FLinearColor>& Value:{TPair<FName,FLinearColor>(TEXT("UndiscoveredBackgroundColor"),Color(194,205,219)),
		{TEXT("UndiscoveredTitleColor"),Ink},{TEXT("UndiscoveredBodyColor"),Muted},{TEXT("UndiscoveredOutlineColor"),Color(159,177,198)}})
		*FindFProperty<FStructProperty>(BP.GeneratedClass,Value.Key)->ContainerPtrToValuePtr<FLinearColor>(CDO)=Value.Value;
	return SaveAssetPackage(Prefix,BP);
}
void Apply()
{
	if(!GoldTexture())
	{
		UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_BUILD_PEARL_CODEX_FAIL: Gold badge texture missing"));
		return;
	}
	auto* Categories=Choice(false);auto* Rewards=Choice(true);
	auto* S=LoadExistingBlueprint(Folder/TEXT("WBP_FF_FacilityCodexScreen"));
	auto* C=LoadExistingBlueprint(Folder/TEXT("WBP_FF_FacilityCodexCard"));
	if(!Categories||!Rewards||!S||!C||!Screen(*S,Categories->GeneratedClass,Rewards->GeneratedClass)||!Card(*C))
	{
		UE_LOG(LogFFUIWbpBuilder,Error,TEXT("FF_BUILD_PEARL_CODEX_FAIL"));
	}
	else
	{
		UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_PEARL_CODEX_PASS"));
	}
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildPearlCodex"),TEXT("Apply the sea festival stamp collection and separate detail page."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
