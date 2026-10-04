// P1: 기존 라이브 패널의 객체·GUID·본문을 보존하고 공통 외곽만 바꾼다.
#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFLivePanelSkyWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_PANEL_SKY");
	constexpr const TCHAR* VersionKey = TEXT("FF.LivePanelSky.FrameVersion");
	constexpr const TCHAR* CloseTexture = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Close_White_256");

	bool IsApplied(const UWidgetBlueprint& BP)
	{
		return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
	}

	// 예상 밖의 사용자 구조는 추측하여 재조립하지 않는다. 기존 객체를 그대로 쓸 수 있는지 먼저 확인한다.
	bool CanApply(const UWidgetBlueprint& BP)
	{
		const UWidgetTree* Tree = BP.WidgetTree;
		if (!Tree || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_LivePanelBase::StaticClass())) return false;
		if (IsApplied(BP)) return true;
		const UBorder* Root = Cast<UBorder>(Tree->RootWidget);
		const UVerticalBox* Content = Cast<UVerticalBox>(Tree->FindWidget(TEXT("VerticalBox_Content")));
		const UHorizontalBox* Header = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("HorizontalBox_Header")));
		const UTextBlock* Title = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_PanelTitle")));
		const UButton* Close = Cast<UButton>(Tree->FindWidget(TEXT("Button_Close")));
		return Root && Root->GetFName() == TEXT("Border_PanelRoot") && Content && Header && Title && Close
			&& Root->GetContent() == Content && Content->GetChildAt(0) == Header
			&& Header->GetChildrenCount() == 2 && Header->GetChildAt(0) == Title && Header->GetChildAt(1) == Close
			&& Close->GetContent() && Close->GetContent()->GetFName() == TEXT("Text_CloseGlyph")
			&& !Tree->FindWidget(TEXT("Sky_VerticalBox_Sections"));
	}

	bool HasButtonAncestor(const UWidget& Widget)
	{
		for (const UPanelWidget* Parent = Widget.GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (Parent->IsA<UButton>()) return true;
		}
		return false;
	}

	bool ApplyFrame(UWidgetBlueprint& BP, UTexture2D& CloseIconTexture)
	{
		if (IsApplied(BP)) return true; // 디자이너가 바꾼 크기·폰트·색은 재실행으로 덮어쓰지 않는다.
		if (!CanApply(BP)) return false;
		BP.Modify();
		UWidgetTree& Tree = *BP.WidgetTree;
		Tree.Modify();
		UBorder* Root = CastChecked<UBorder>(Tree.RootWidget);
		UVerticalBox* Content = CastChecked<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Content")));
		UHorizontalBox* Header = CastChecked<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		UTextBlock* Title = CastChecked<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UButton* Close = CastChecked<UButton>(Tree.FindWidget(TEXT("Button_Close")));

		// 색만 정리한다. 본문 수치·문구·폰트 크기·상태색과 SNS 선택 버튼의 흰 글자는 보존한다.
		Tree.ForEachWidget([](UWidget* Widget)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Widget); Label && !HasButtonAncestor(*Label))
			{
				const FLinearColor Old = Label->GetColorAndOpacity().GetSpecifiedColor();
				const bool bAmberOrRed = Old.R > Old.G * 1.4f && Old.R > 0.15f;
				if (!bAmberOrRed) Label->SetColorAndOpacity(FSlateColor(Old.GetLuminance() > 0.20f ? Muted : Ink));
			}
		});

		// FindWidget은 연결된 루트부터 탐색하므로 본문을 분리하기 전에 기존 스크롤을 확인한다.
		const bool bOwnsScroll = Tree.FindWidget(TEXT("Scroll_Posts")) || Tree.FindWidget(TEXT("Scroll_Density"));
		Content->RemoveChild(Header);
		Root->RemoveChild(Content);
		UVerticalBox* Sections = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Sky_VerticalBox_Sections"));
		UBorderSlot* RootSlot = CastChecked<UBorderSlot>(Root->AddChild(Sections));
		RootSlot->SetHorizontalAlignment(HAlign_Fill);
		RootSlot->SetVerticalAlignment(VAlign_Fill);
		RootSlot->SetPadding(FMargin(2.f));
		Root->SetBrush(Brush(Paper, 8.f, 1.5f, SkyLine));
		Root->SetPadding(FMargin(2.f));
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		USizeBox* HeaderSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Sky_SizeBox_Header"));
		HeaderSize->SetHeightOverride(60.f);
		HeaderSize->AddChild(Header);
		Row(*Sections, *HeaderSize, FMargin(22.f, 0.f));
		StyleText(*Title, 24, Ink, TEXT("SemiBold"));
		Row(*Sections, *Rule(BP, Tree, TEXT("Sky_Border_HeaderDivider")), FMargin(22.f, 0.f));

		// SNS와 밀집은 이미 스크롤을 소유한다. 특히 SNS의 읽기 위치·새 글 로직을 다른 ScrollBox로 옮기지 않는다.
		if (bOwnsScroll)
		{
			Row(*Sections, *Content, FMargin(22.f, 16.f), true);
		}
		else
		{
			UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Sky_ScrollBox_Body"));
			Scroll->SetAllowOverscroll(false);
			Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
			Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f));
			Scroll->SetClipping(EWidgetClipping::ClipToBounds);
			UScrollBoxSlot* BodySlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Content));
			BodySlot->SetHorizontalAlignment(HAlign_Fill);
			Row(*Sections, *Scroll, FMargin(22.f, 16.f), true);
		}

		// 닫기 버튼과 기존 글리프 객체도 삭제하지 않는다. 기존 글리프는 숨겨 GUID/애니메이션 참조를 보존한다.
		UWidget* OldGlyph = Close->GetContent();
		Close->RemoveChild(OldGlyph);
		OldGlyph->SetVisibility(ESlateVisibility::Collapsed);
		UOverlay* CloseContent = MakeWidget<UOverlay>(BP, Tree, TEXT("Sky_Overlay_CloseContent"));
		CloseContent->AddChildToOverlay(OldGlyph);
		USizeBox* IconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Sky_SizeBox_CloseIcon"));
		IconSize->SetWidthOverride(18.f); IconSize->SetHeightOverride(18.f);
		UImage* Icon = MakeWidget<UImage>(BP, Tree, TEXT("Sky_Image_CloseIcon"));
		Icon->SetBrushFromTexture(&CloseIconTexture);
		Icon->SetColorAndOpacity(Muted);
		IconSize->AddChild(Icon);
		UOverlaySlot* IconSlot = CloseContent->AddChildToOverlay(IconSize);
		IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);
		UButtonSlot* CloseContentSlot = CastChecked<UButtonSlot>(Close->AddChild(CloseContent));
		CloseContentSlot->SetPadding(FMargin(0.f));
		CloseContentSlot->SetHorizontalAlignment(HAlign_Center); CloseContentSlot->SetVerticalAlignment(VAlign_Center);
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.Normal = Brush(Paper, 6.f, 1.f, SkyLine);
		CloseStyle.Hovered = Brush(SkyWash, 6.f, 2.f, SkyLine);
		CloseStyle.Pressed = Brush(Color(221, 239, 246), 6.f, 2.f, Ink);
		CloseStyle.Disabled = Brush(SkyWash, 6.f, 1.f, SkyLine);
		CloseStyle.NormalPadding = CloseStyle.PressedPadding = FMargin(0.f);
		Close->SetStyle(CloseStyle);
		Header->RemoveChild(Close);
		USizeBox* CloseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Sky_SizeBox_Close"));
		CloseSize->SetWidthOverride(36.f); CloseSize->SetHeightOverride(36.f); CloseSize->AddChild(Close);
		Column(*Header, *CloseSize);
		CastChecked<UHorizontalBoxSlot>(CloseSize->Slot)->SetVerticalAlignment(VAlign_Center);

		if (UFF_LivePanelBase* Defaults = BP.GeneratedClass ? Cast<UFF_LivePanelBase>(BP.GeneratedClass->GetDefaultObject()) : nullptr)
		{
			Defaults->Modify();
			Defaults->DynamicRowFont = Title->GetFont();
			Defaults->DynamicRowFont.Size = 16;
			Defaults->DynamicRowFont.TypefaceFontName = TEXT("Medium");
			Defaults->DynamicRowColor = Ink;
			Defaults->DynamicRowNoteColor = Muted;
		}
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix);
			return;
		}
		UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(CloseTexture));
		if (!Icon) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 닫기 아이콘을 찾지 못했습니다."), LogPrefix); return; }
		TArray<UWidgetBlueprint*> Targets;
		for (const TCHAR* Tab : { TEXT("Operations"), TEXT("Facilities"), TEXT("Guests"), TEXT("Queues"), TEXT("SNS"), TEXT("Staff"), TEXT("Density") })
		{
			const FString Package = FString(TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanel")) + Tab;
			UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
			if (!BP || BP->GetOutermost()->IsDirty() || !CanApply(*BP))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 로드/미저장/구조 검사 실패. 아무 패널도 수정하지 않았습니다."), LogPrefix, *Package);
				return;
			}
			if (!IsApplied(*BP)) Targets.Add(BP);
		}
		if (Targets.IsEmpty()) { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 7패널 적용 완료. Designer 편집을 보존했습니다."), LogPrefix); return; }
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LivePanelSky") /
			(FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		if (!IFileManager::Get().MakeDirectory(*Backup, true)) return;
		for (const UWidgetBlueprint* BP : Targets)
		{
			const FString Stem = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName());
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk") })
			{
				const FString Source = Stem + Extension;
				if (!IFileManager::Get().FileExists(*Source)) continue;
				if (IFileManager::Get().IsReadOnly(*Source) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(Source)), *Source, false, false) != COPY_OK)
				{
					UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 백업/쓰기 검사 실패. 수정하지 않았습니다."), LogPrefix, *Source);
					return;
				}
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(Backup));
		for (UWidgetBlueprint* BP : Targets)
		{
			if (!ApplyFrame(*BP, *Icon) || !SaveNewBlueprint(LogPrefix, *BP))
			{
				// 실패한 에셋은 재시도할 수 있게 버전을 지운다. 기존 저장본은 백업에 있다.
				BP->GetOutermost()->GetMetaData().RemoveValue(BP, VersionKey);
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 적용 실패. 백업=%s"), LogPrefix, *BP->GetPathName(), *Backup);
				return;
			}
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: %s"), LogPrefix, *BP->GetPathName());
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLivePanelSkyDesign"),
		TEXT("P1 라이브 7패널 공통 외곽을 최초 한 번 적용한다. 기존 본문·객체·바인딩과 재실행 시 Designer 편집을 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFLiveSkyFramePreservationTest,
	"FestaFiesta.UI.LiveHUD.SkyFrame.PreserveContentAndDesignerEdits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFLiveSkyFramePreservationTest::RunTest(const FString& Parameters)
{
	using namespace FFLivePanelSkyWbpBuilder;
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/LiveSkyFrame_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UWidgetBlueprint* BP = NewObject<UWidgetBlueprint>(Package);
	BP->ParentClass = UFF_LivePanelBase::StaticClass();
	BP->WidgetTree = NewObject<UWidgetTree>(BP);
	UWidgetTree& Tree = *BP->WidgetTree;
	UBorder* Root = MakeWidget<UBorder>(*BP, Tree, TEXT("Border_PanelRoot")); Tree.RootWidget = Root;
	UVerticalBox* Content = MakeWidget<UVerticalBox>(*BP, Tree, TEXT("VerticalBox_Content")); Root->AddChild(Content);
	UHorizontalBox* Header = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("HorizontalBox_Header")); Content->AddChildToVerticalBox(Header);
	UTextBlock* Title = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_PanelTitle"), true); Header->AddChildToHorizontalBox(Title);
	UButton* Close = MakeWidget<UButton>(*BP, Tree, TEXT("Button_Close"), true); Header->AddChildToHorizontalBox(Close);
	UTextBlock* Glyph = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_CloseGlyph")); Close->AddChild(Glyph);
	UTextBlock* Value = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_ExistingValue"), true);
	Value->SetText(FText::FromString(TEXT("123")));
	UVerticalBoxSlot* ValueSlot = Content->AddChildToVerticalBox(Value); ValueSlot->SetPadding(FMargin(3.f, 7.f));
	const TMap<FName, FGuid> OriginalGuids = BP->WidgetVariableNameToGuidMap;
	UTexture2D* Icon = NewObject<UTexture2D>(Package);
	TestTrue(TEXT("검사된 기존 트리에 적용"), ApplyFrame(*BP, *Icon));
	TestEqual(TEXT("값 객체 보존"), Tree.FindWidget(TEXT("Text_ExistingValue")), static_cast<UWidget*>(Value));
	TestEqual(TEXT("본문 슬롯 보존"), Value->Slot.Get(), static_cast<UPanelSlot*>(ValueSlot));
	TestEqual(TEXT("본문 문구 보존"), Value->GetText().ToString(), FString(TEXT("123")));
	TestEqual(TEXT("닫기 연결 대상 보존"), Tree.FindWidget(TEXT("Button_Close")), static_cast<UWidget*>(Close));
	TestEqual(TEXT("기존 글리프 참조 보존"), Tree.FindWidget(TEXT("Text_CloseGlyph")), static_cast<UWidget*>(Glyph));
	for (const TPair<FName, FGuid>& Pair : OriginalGuids)
	{
		TestTrue(*FString::Printf(TEXT("GUID 보존 %s"), *Pair.Key.ToString()), BP->WidgetVariableNameToGuidMap.FindRef(Pair.Key) == Pair.Value);
	}
	TestTrue(TEXT("헤더는 스크롤 바깥"), Header->GetParent()->IsA<USizeBox>());
	TestTrue(TEXT("본문만 스크롤"), Content->GetParent()->IsA<UScrollBox>());
	FSlateFontInfo Edited = Title->GetFont(); Edited.Size = 31; Title->SetFont(Edited);
	Root->SetPadding(FMargin(9.f));
	TestTrue(TEXT("재실행 성공"), ApplyFrame(*BP, *Icon));
	TestEqual(TEXT("재실행 후 Designer 글꼴 유지"), Title->GetFont().Size, 31.f);
	TestTrue(TEXT("재실행 후 Designer 여백 유지"), Root->GetPadding() == FMargin(9.f));
	for (const TCHAR* ScrollName : { TEXT("Scroll_Posts"), TEXT("Scroll_Density") })
	{
		UWidgetBlueprint* ScrollingBP = NewObject<UWidgetBlueprint>(Package);
		ScrollingBP->ParentClass = UFF_LivePanelBase::StaticClass();
		ScrollingBP->WidgetTree = NewObject<UWidgetTree>(ScrollingBP);
		UWidgetTree& ScrollingTree = *ScrollingBP->WidgetTree;
		UBorder* ScrollingRoot = MakeWidget<UBorder>(*ScrollingBP, ScrollingTree, TEXT("Border_PanelRoot"));
		ScrollingTree.RootWidget = ScrollingRoot;
		UVerticalBox* Body = MakeWidget<UVerticalBox>(*ScrollingBP, ScrollingTree, TEXT("VerticalBox_Content"));
		ScrollingRoot->AddChild(Body);
		UHorizontalBox* ScrollingHeader = MakeWidget<UHorizontalBox>(*ScrollingBP, ScrollingTree, TEXT("HorizontalBox_Header"));
		Body->AddChild(ScrollingHeader);
		ScrollingHeader->AddChild(MakeWidget<UTextBlock>(*ScrollingBP, ScrollingTree, TEXT("Text_PanelTitle")));
		UButton* ScrollingClose = MakeWidget<UButton>(*ScrollingBP, ScrollingTree, TEXT("Button_Close"));
		ScrollingHeader->AddChild(ScrollingClose);
		ScrollingClose->AddChild(MakeWidget<UTextBlock>(*ScrollingBP, ScrollingTree, TEXT("Text_CloseGlyph")));
		UScrollBox* ExistingScroll = MakeWidget<UScrollBox>(*ScrollingBP, ScrollingTree, ScrollName);
		Body->AddChild(ExistingScroll);
		TestTrue(TEXT("자체 스크롤 패널 적용"), ApplyFrame(*ScrollingBP, *Icon));
		TestEqual(TEXT("기존 스크롤과 그 부모 보존"), ExistingScroll->GetParent(), static_cast<UPanelWidget*>(Body));
		TestTrue(TEXT("본문을 이중 스크롤로 감싸지 않는다"), Body->GetParent()->IsA<UVerticalBox>());
		TestNull(TEXT("자체 스크롤이 있을 때 외부 스크롤 없음"), ScrollingTree.FindWidget(TEXT("Sky_ScrollBox_Body")));
	}
	return true;
}
#endif
#endif
