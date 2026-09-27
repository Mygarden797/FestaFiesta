// SNS·밀집 패널만 적용한다. 하단 HUD·탭·공용 버튼과 스타일 에셋은 수정하지 않는다.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Animation/WidgetAnimation.h"
#include "AssetToolsModule.h"
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
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveSocialPanelsWbpBuilder"

namespace FFLiveSocialPanelsWbpBuilder
{
	using namespace FFUIWbpBuilder;

	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_SOCIAL_PANELS");
	const FString PanelsFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels");

	FSlateBrush Surface(const FLinearColor& Color, const float Radius = 6.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Color;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	void AddVertical(UVerticalBox& Parent, UWidget* Child, const FMargin& Padding = FMargin(0.f),
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		UVerticalBoxSlot* Slot = Parent.AddChildToVerticalBox(Child);
		Slot->SetSize(FSlateChildSize(SizeRule));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetPadding(Padding);
	}

	void AddHorizontal(UHorizontalBox& Parent, UWidget* Child, const FMargin& Padding = FMargin(0.f),
		const ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic)
	{
		UHorizontalBoxSlot* Slot = Parent.AddChildToHorizontalBox(Child);
		Slot->SetSize(FSlateChildSize(SizeRule));
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(Padding);
	}

	UTextBlock* MakeText(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
		const FText& Text, const int32 Size, const FLinearColor& Color, const bool bVariable = false,
		const FName Typeface = TEXT("Medium"))
	{
		UTextBlock* Widget = MakeWidget<UTextBlock>(BP, Tree, Name, bVariable);
		StyleText(*Widget, Size, Color, Typeface);
		Widget->SetText(Text);
		Widget->SetAutoWrapText(true);
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Widget;
	}

	void StyleButton(UButton& Button, const FFF_UIPaletteInGame& Colors, const FMargin& Padding)
	{
		FButtonStyle Style = Button.GetStyle();
		Style.Normal = Surface(Colors.PaperSunken);
		Style.Hovered = Surface(Colors.Hover);
		Style.Pressed = Surface(Colors.Teal);
		Style.Disabled = Surface(Colors.PaperSunken);
		Style.NormalPadding = Padding;
		Style.PressedPadding = Padding;
		Button.SetStyle(Style);
	}

	void ButtonContent(UButton& Button, UWidget& Content)
	{
		UButtonSlot* Slot = CastChecked<UButtonSlot>(Button.AddChild(&Content));
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(0.f));
	}

	UVerticalBox* PanelFrame(UWidgetBlueprint& BP, UWidgetTree& Tree, const FFF_UIPaletteInGame& Colors,
		const FText& Title)
	{
		UBorder* Root = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PanelRoot"), true);
		Root->SetBrush(Surface(Colors.Paper, 8.f));
		Root->SetPadding(FMargin(24.f, 20.f));
		Tree.RootWidget = Root;

		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Content"));
		UBorderSlot* ContentSlot = CastChecked<UBorderSlot>(Root->AddChild(Content));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);

		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Header"));
		AddHorizontal(*Header, MakeText(BP, Tree, TEXT("Text_PanelTitle"), Title, 20,
			Colors.Ink, true, TEXT("SemiBold")), FMargin(0.f, 0.f, 12.f, 0.f), ESlateSizeRule::Fill);
		UButton* Close = MakeWidget<UButton>(BP, Tree, TEXT("Button_Close"), true);
		StyleButton(*Close, Colors, FMargin(10.f, 3.f));
		Close->SetToolTipText(LOCTEXT("CloseTooltip", "닫기"));
		ButtonContent(*Close, *MakeText(BP, Tree, TEXT("Text_CloseGlyph"), LOCTEXT("CloseGlyph", "×"),
			18, Colors.InkMuted));
		AddHorizontal(*Header, Close);
		AddVertical(*Content, Header, FMargin(0.f, 0.f, 0.f, 8.f));
		return Content;
	}

	UScrollBox* MakeScroll(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
		const bool bVariable = false)
	{
		UScrollBox* Scroll = MakeWidget<UScrollBox>(BP, Tree, Name, bVariable);
		Scroll->SetOrientation(Orient_Vertical);
		Scroll->SetAllowOverscroll(false);
		Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		Scroll->SetScrollbarThickness(FVector2D(5.f, 5.f));
		Scroll->SetScrollbarPadding(FMargin(6.f, 0.f, 0.f, 0.f));
		Scroll->SetClipping(EWidgetClipping::ClipToBounds);
		return Scroll;
	}

	bool BuildSNS(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette) return false;
		const FFF_UIPaletteInGame& Colors = Palette->InGame;
		UVerticalBox* Content = PanelFrame(BP, Tree, Colors, LOCTEXT("SNSTitle", "SNS"));
		AddVertical(*Content, MakeText(BP, Tree, TEXT("Text_Subtitle"),
			LOCTEXT("SNSSubtitle", "관객들이 전하는 축제 이야기"), 13, Colors.InkMuted), FMargin(0.f, 0.f, 0.f, 14.f));

		UHorizontalBox* Filters = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Filters"));
		const FName FilterKeys[] = { TEXT("All"), TEXT("Positive"), TEXT("Negative") };
		const FText FilterLabels[] = { LOCTEXT("FilterAll", "전체"), LOCTEXT("FilterPositive", "긍정"),
			LOCTEXT("FilterNegative", "아쉬움") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FilterKeys); ++Index)
		{
			const FString Suffix = FilterKeys[Index].ToString();
			UButton* Filter = MakeWidget<UButton>(BP, Tree, FName(TEXT("Button_Filter") + Suffix), true);
			StyleButton(*Filter, Colors, FMargin(16.f, 7.f));
			ButtonContent(*Filter, *MakeText(BP, Tree, FName(TEXT("Text_Filter") + Suffix),
				FilterLabels[Index], 13, Colors.InkMuted, true));
			AddHorizontal(*Filters, Filter, FMargin(0.f, 0.f, Index < 2 ? 8.f : 0.f, 0.f));
		}
		AddVertical(*Content, Filters, FMargin(0.f, 0.f, 0.f, 9.f));
		AddVertical(*Content, MakeText(BP, Tree, TEXT("Text_Count"),
			LOCTEXT("SNSCountWaiting", "반응을 불러오는 중입니다."), 12, Colors.InkMuted, true), FMargin(0.f, 0.f, 0.f, 10.f));

		UButton* NewPosts = MakeWidget<UButton>(BP, Tree, TEXT("Button_NewPosts"), true);
		StyleButton(*NewPosts, Colors, FMargin(12.f, 7.f));
		ButtonContent(*NewPosts, *MakeText(BP, Tree, TEXT("Text_NewPosts"),
			LOCTEXT("SNSNewPosts", "↑ 새 글 보기"), 13, Colors.TealDeep, true));
		NewPosts->SetVisibility(ESlateVisibility::Collapsed);
		AddVertical(*Content, NewPosts, FMargin(0.f, 0.f, 0.f, 10.f));

		UOverlay* Feed = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Feed"));
		UScrollBox* Scroll = MakeScroll(BP, Tree, TEXT("Scroll_Posts"), true);
		UVerticalBox* Posts = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_Posts"), true);
		UScrollBoxSlot* PostsSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Posts));
		PostsSlot->SetHorizontalAlignment(HAlign_Fill);
		PostsSlot->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
		Feed->AddChildToOverlay(Scroll);
		UTextBlock* Empty = MakeText(BP, Tree, TEXT("Text_Empty"),
			LOCTEXT("SNSEmpty", "아직 올라온 반응이 없습니다."), 14, Colors.InkMuted, true);
		Empty->SetJustification(ETextJustify::Center);
		UOverlaySlot* EmptySlot = Feed->AddChildToOverlay(Empty);
		EmptySlot->SetVerticalAlignment(VAlign_Center);
		EmptySlot->SetPadding(FMargin(12.f));
		AddVertical(*Content, Feed, FMargin(0.f), ESlateSizeRule::Fill);

		UTextBlock* MissingNote = MakeText(BP, Tree, TEXT("Text_MissingNote"), FText::GetEmpty(), 12,
			Colors.InkMuted, true);
		MissingNote->SetVisibility(ESlateVisibility::Collapsed);
		AddVertical(*Content, MissingNote, FMargin(0.f, 8.f));
		AddVertical(*Content, MakeText(BP, Tree, TEXT("Text_ReadPositionNote"),
			LOCTEXT("SNSReadPosition", "새 글이 와도 읽던 위치를 유지합니다."), 11, Colors.InkMuted), FMargin(0.f, 8.f, 0.f, 0.f));
		return true;
	}

	/** 인물은 공용 실루엣이다. 작성자별 프로필 이미지나 성별·연령 데이터를 가정하지 않는다. */
	USizeBox* MakeAvatar(UWidgetBlueprint& BP, UWidgetTree& Tree, const FFF_UIPaletteInGame& Colors)
	{
		USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_Avatar"));
		Size->SetWidthOverride(36.f);
		Size->SetHeightOverride(36.f);
		UOverlay* Parts = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Avatar"));
		Size->AddChild(Parts);
		UImage* Background = MakeWidget<UImage>(BP, Tree, TEXT("Image_AvatarBackground"));
		Background->SetBrush(Surface(Colors.LineStrong, 18.f));
		Parts->AddChildToOverlay(Background);
		UImage* Head = MakeWidget<UImage>(BP, Tree, TEXT("Image_AvatarHead"));
		Head->SetBrush(Surface(Colors.White, 5.f));
		Head->SetDesiredSizeOverride(FVector2D(10.f, 10.f));
		UOverlaySlot* HeadSlot = Parts->AddChildToOverlay(Head);
		HeadSlot->SetHorizontalAlignment(HAlign_Center);
		HeadSlot->SetVerticalAlignment(VAlign_Top);
		HeadSlot->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
		UImage* Body = MakeWidget<UImage>(BP, Tree, TEXT("Image_AvatarBody"));
		Body->SetBrush(Surface(Colors.White, 5.f));
		Body->SetDesiredSizeOverride(FVector2D(20.f, 11.f));
		UOverlaySlot* BodySlot = Parts->AddChildToOverlay(Body);
		BodySlot->SetHorizontalAlignment(HAlign_Center);
		BodySlot->SetVerticalAlignment(VAlign_Bottom);
		BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		Size->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Size;
	}

	bool BuildPostEntry(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette) return false;
		const FFF_UIPaletteInGame& Colors = Palette->InGame;
		UBorder* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Card"), true);
		Card->SetBrush(Surface(Colors.PaperSunken));
		Card->SetPadding(FMargin(14.f, 13.f));
		Tree.RootWidget = Card;

		UHorizontalBox* Row = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Row"));
		Card->AddChild(Row);
		AddHorizontal(*Row, MakeAvatar(BP, Tree, Colors), FMargin(0.f, 0.f, 12.f, 0.f));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_Post"));
		AddHorizontal(*Row, Content, FMargin(0.f), ESlateSizeRule::Fill);
		UHorizontalBox* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_PostHeader"));
		AddHorizontal(*Header, MakeText(BP, Tree, TEXT("Text_Author"),
			LOCTEXT("PostAuthorPreview", "관객"), 14, Colors.Ink, true, TEXT("SemiBold")),
			FMargin(0.f, 0.f, 8.f, 0.f), ESlateSizeRule::Fill);

		UBorder* Sentiment = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Sentiment"), true);
		Sentiment->SetBrush(Surface(Colors.White, 12.f));
		Sentiment->SetPadding(FMargin(8.f, 3.f));
		UHorizontalBox* Badge = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Sentiment"));
		Sentiment->AddChild(Badge);
		AddHorizontal(*Badge, MakeText(BP, Tree, TEXT("Text_SentimentSymbol"),
			LOCTEXT("PostNeutralSymbol", "·"), 13, Colors.Neutral, true), FMargin(0.f, 0.f, 4.f, 0.f));
		AddHorizontal(*Badge, MakeText(BP, Tree, TEXT("Text_Sentiment"),
			LOCTEXT("PostNeutralLabel", "반응"), 12, Colors.Neutral, true));
		AddHorizontal(*Header, Sentiment);
		AddVertical(*Content, Header, FMargin(0.f, 0.f, 0.f, 7.f));
		AddVertical(*Content, MakeText(BP, Tree, TEXT("Text_Body"),
			LOCTEXT("PostBodyPreview", "관객 반응 본문이 표시됩니다."), 13, Colors.Ink, true));
		return true;
	}

	UVerticalBox* MetricCard(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
		const FFF_UIPaletteInGame& Colors, UHorizontalBox& Parent, const FMargin& Padding)
	{
		UBorder* Card = MakeWidget<UBorder>(BP, Tree, Name);
		Card->SetBrush(Surface(Colors.PaperSunken));
		Card->SetPadding(FMargin(14.f, 12.f));
		UVerticalBox* Content = MakeWidget<UVerticalBox>(BP, Tree, FName(Name.ToString() + TEXT("_Content")));
		Card->AddChild(Content);
		AddHorizontal(Parent, Card, Padding, ESlateSizeRule::Fill);
		return Content;
	}

	bool BuildDensity(UWidgetBlueprint& BP, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette) return false;
		const FFF_UIPaletteInGame& Colors = Palette->InGame;
		UVerticalBox* Content = PanelFrame(BP, Tree, Colors, LOCTEXT("DensityTitle", "밀집"));
		UScrollBox* Scroll = MakeScroll(BP, Tree, TEXT("Scroll_Density"));
		AddVertical(*Content, Scroll, FMargin(0.f), ESlateSizeRule::Fill);
		UVerticalBox* Details = MakeWidget<UVerticalBox>(BP, Tree, TEXT("VerticalBox_DensityDetails"));
		UScrollBoxSlot* DetailsSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(Details));
		DetailsSlot->SetHorizontalAlignment(HAlign_Fill);
		AddVertical(*Details, MakeText(BP, Tree, TEXT("Text_Subtitle"),
			LOCTEXT("DensitySubtitle", "관객이 모이는 곳을 확인하세요."), 13, Colors.InkMuted), FMargin(0.f, 0.f, 0.f, 12.f));

		UButton* Toggle = MakeWidget<UButton>(BP, Tree, TEXT("Button_Toggle"), true);
		StyleButton(*Toggle, Colors, FMargin(12.f, 8.f));
		ButtonContent(*Toggle, *MakeText(BP, Tree, TEXT("Text_ToggleLabel"),
			LOCTEXT("DensityToggle", "히트맵 켜기"), 13, Colors.TealDeep, true));
		AddVertical(*Details, Toggle, FMargin(0.f, 0.f, 0.f, 12.f));

		UHorizontalBox* Metrics = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_Metrics"));
		UVerticalBox* Peak = MetricCard(BP, Tree, TEXT("Border_PeakCard"), Colors, *Metrics, FMargin(0.f, 0.f, 8.f, 0.f));
		AddVertical(*Peak, MakeText(BP, Tree, TEXT("Text_PeakTitle"),
			LOCTEXT("PeakTitle", "최대 밀도"), 12, Colors.InkMuted), FMargin(0.f, 0.f, 0.f, 4.f));
		AddVertical(*Peak, MakeText(BP, Tree, TEXT("Text_Peak"),
			LOCTEXT("PeakWaiting", "—"), 23, Colors.TealDeep, true, TEXT("SemiBold")));
		AddVertical(*Peak, MakeText(BP, Tree, TEXT("Text_PeakGrade"), FText::GetEmpty(), 12, Colors.InkMuted, true), FMargin(0.f, 3.f, 0.f, 0.f));
		UVerticalBox* Congested = MetricCard(BP, Tree, TEXT("Border_CongestedCard"), Colors, *Metrics, FMargin(0.f));
		AddVertical(*Congested, MakeText(BP, Tree, TEXT("Text_CongestedTitle"),
			LOCTEXT("CongestedTitle", "혼잡한 칸"), 12, Colors.InkMuted), FMargin(0.f, 0.f, 0.f, 4.f));
		AddVertical(*Congested, MakeText(BP, Tree, TEXT("Text_Congested"),
			LOCTEXT("CongestedWaiting", "—"), 23, Colors.TealDeep, true, TEXT("SemiBold")));
		AddVertical(*Details, Metrics, FMargin(0.f, 0.f, 0.f, 9.f));
		AddVertical(*Details, MakeText(BP, Tree, TEXT("Text_CellSize"),
			LOCTEXT("CellSizeWaiting", "집계 단위를 확인하는 중입니다."), 12, Colors.InkMuted, true), FMargin(0.f, 0.f, 0.f, 18.f));

		UHorizontalBox* LegendHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("HorizontalBox_LegendHeader"));
		AddHorizontal(*LegendHeader, MakeText(BP, Tree, TEXT("Text_LegendTitle"),
			LOCTEXT("LegendTitle", "밀집 등급"), 15, Colors.Ink, false, TEXT("SemiBold")),
			FMargin(0.f), ESlateSizeRule::Fill);
		AddHorizontal(*LegendHeader, MakeText(BP, Tree, TEXT("Text_LegendUnit"),
			LOCTEXT("LegendUnit", "단위: 명/㎡"), 11, Colors.InkMuted));
		AddVertical(*Details, LegendHeader, FMargin(0.f, 0.f, 0.f, 8.f));
		AddVertical(*Details, MakeWidget<UVerticalBox>(BP, Tree, TEXT("Box_Legend"), true));
		AddVertical(*Details, MakeText(BP, Tree, TEXT("Text_LegendHint"),
			LOCTEXT("LegendHint", "색상과 함께 등급·수치를 확인하세요."), 11, Colors.InkMuted), FMargin(0.f, 10.f, 0.f, 0.f));
		AddVertical(*Details, MakeText(BP, Tree, TEXT("Text_MissingNote"), FText::GetEmpty(),
			12, Colors.InkMuted, true), FMargin(0.f, 12.f, 0.f, 0.f));
		return true;
	}

	struct FTarget
	{
		FString Package;
		UClass* Parent = nullptr;
		FBuildLayout Build;
		UWidgetBlueprint* Blueprint = nullptr;
	};

	/** 세 대상 모두를 검사한 뒤 백업한다. 다른 세션의 미저장 디자이너 변경을 덮어쓰지 않는다. */
	bool PreflightAndBackup(TArray<FTarget>& Targets, FString& BackupFolder)
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE를 종료한 뒤 실행해야 한다."), LogPrefix);
			return false;
		}
		for (FTarget& Target : Targets)
		{
			UPackage* LoadedPackage = FindPackage(nullptr, *Target.Package);
			if (LoadedPackage && LoadedPackage->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s에 미저장 변경이 있어 아무 에셋도 수정하지 않았다."), LogPrefix, *Target.Package);
				return false;
			}
			if (FPackageName::DoesPackageExist(Target.Package))
			{
				Target.Blueprint = LoadExistingBlueprint(Target.Package);
				if (!Target.Blueprint || Target.Blueprint->ParentClass != Target.Parent || Target.Blueprint->GetOutermost()->IsDirty())
				{
					UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 로드·부모·미저장 변경 검사를 통과하지 못했다. 수정하지 않았다."), LogPrefix, *Target.Package);
					return false;
				}
				const FString Filename = FPackageName::LongPackageNameToFilename(Target.Package, FPackageName::GetAssetPackageExtension());
				if (IFileManager::Get().IsReadOnly(*Filename))
				{
					UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 읽기 전용 에셋 %s. 수정하지 않았다."), LogPrefix, *Filename);
					return false;
				}
			}
		}

		BackupFolder = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveSocialPanels") /
			(FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		if (!IFileManager::Get().MakeDirectory(*BackupFolder, true)) return false;
		for (const FTarget& Target : Targets)
		{
			if (!Target.Blueprint) continue;
			const FString Stem = FPackageName::LongPackageNameToFilename(Target.Package);
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk") })
			{
				const FString Source = Stem + Extension;
				if (!IFileManager::Get().FileExists(*Source)) continue;
				const FString Destination = BackupFolder / FPaths::GetCleanFilename(Source);
				if (IFileManager::Get().Copy(*Destination, *Source, false, false) != COPY_OK)
				{
					UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 백업 실패. 에셋은 수정하지 않았다."), LogPrefix, *Source);
					return false;
				}
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(BackupFolder));
		return true;
	}

	bool BuildAndCompile(FTarget& Target)
	{
		if (Target.Blueprint)
		{
			if (!ResetWidgetTree(LogPrefix, *Target.Blueprint)) return false;
		}
		else
		{
			UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
			Factory->ParentClass = Target.Parent;
			FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			Target.Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
				FPackageName::GetLongPackageAssetName(Target.Package), PanelsFolder, UWidgetBlueprint::StaticClass(), Factory));
		}
		UWidgetBlueprint* BP = Target.Blueprint;
		if (!BP || !BP->WidgetTree || !Target.Build(*BP, *BP->WidgetTree)) return false;

		// 공용 저장 함수와 같은 고아 위젯 검사를 먼저 수행하되 여기서는 아직 저장하지 않는다.
		TSet<FName> ReachableNames;
		BP->WidgetTree->ForEachWidget([&ReachableNames](UWidget* Widget)
		{
			if (Widget) ReachableNames.Add(Widget->GetFName());
		});
		for (const UWidgetAnimation* Animation : BP->Animations)
		{
			if (Animation) ReachableNames.Add(Animation->GetFName());
		}
		for (const TPair<FName, FGuid>& Entry : BP->WidgetVariableNameToGuidMap)
		{
			if (!ReachableNames.Contains(Entry.Key))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s의 고아 위젯 %s. 저장하지 않았다."),
					LogPrefix, *Target.Package, *Entry.Key.ToString());
				return false;
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		return BP->Status != BS_Error && BP->GeneratedClass != nullptr;
	}

	void ApplyDesign()
	{
		// 하단 버튼은 이미 존재하는 SNS·Density 탭을 사용한다. HUD 루트나 다른 WBP에 접근하지 않는다.
		TArray<FTarget> Targets;
		Targets.Add({ PanelsFolder / TEXT("WBP_FF_LiveSnsPostEntry"), UFF_LiveSnsPostEntry::StaticClass(), BuildPostEntry });
		Targets.Add({ PanelsFolder / TEXT("WBP_FF_LivePanelSNS"), UFF_LivePanelSNS::StaticClass(), BuildSNS });
		Targets.Add({ PanelsFolder / TEXT("WBP_FF_LivePanelDensity"), UFF_LivePanelDensity::StaticClass(), BuildDensity });
		FString BackupFolder;
		if (!PreflightAndBackup(Targets, BackupFolder)) return;
		for (FTarget& Target : Targets)
		{
			if (!BuildAndCompile(Target))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 구성·컴파일 실패. 어떤 대상도 저장하지 않았다. 메모리의 변경은 저장하지 말고 대상 에셋을 다시 불러올 것. 백업: %s"),
					LogPrefix, *Target.Package, *BackupFolder);
				return;
			}
		}
		// 경로 fallback에만 기대면 패키징에서 카드 WBP가 빠질 수 있어 SNS CDO에 명시 참조를 저장한다.
		UClass* SNSClass = Targets[1].Blueprint->GeneratedClass;
		FClassProperty* EntryClassProperty = FindFProperty<FClassProperty>(SNSClass, TEXT("PostEntryClass"));
		if (!EntryClassProperty || !SNSClass->GetDefaultObject())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: SNS PostEntryClass 계약이 없어 어떤 대상도 저장하지 않았다."), LogPrefix);
			return;
		}
		EntryClassProperty->SetObjectPropertyValue_InContainer(
			SNSClass->GetDefaultObject(), Targets[0].Blueprint->GeneratedClass);
		for (FTarget& Target : Targets)
		{
			if (!SaveAssetPackage(LogPrefix, *Target.Blueprint))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 저장 실패. 앞선 대상은 저장됐을 수 있다. 원본 백업: %s"),
					LogPrefix, *Target.Package, *BackupFolder);
				return;
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: SNS·밀집 패널과 SNS 글 카드 3개만 적용했다. 백업: %s"), LogPrefix, *BackupFolder);
	}
}

static FAutoConsoleCommand GFFApplyLiveSocialPanelsDesign(
	TEXT("FF.ApplyLiveSocialPanelsDesign"),
	TEXT("SNS·밀집 WBP와 SNS 글 카드만 백업 후 교체한다. 하단 HUD·탭·공용 스타일은 변경하지 않는다. 미저장 대상이나 PIE에서는 중단한다."),
	FConsoleCommandDelegate::CreateStatic(&FFLiveSocialPanelsWbpBuilder::ApplyDesign));

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
