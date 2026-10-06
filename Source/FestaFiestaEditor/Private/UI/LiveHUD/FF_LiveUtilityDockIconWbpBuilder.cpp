// 왼쪽 위 SNS·음악 도크와 음악 펼침 패널의 아이콘 정리. 처음 한 번만 바꾸고 이후 Designer 편집은 보존한다.
//
// - 원형 버튼의 흰 원을 없애고 아이콘·글자를 흰색+그림자로 둔다. 선택 표시는 런타임이 색으로 한다.
// - `♫` 글리프는 프로젝트 글꼴에 없어 대체 글꼴 상자로 그려졌다. 직접 그린 음표 텍스처로 바꾼다.
//   옛 글리프 글자는 지우지 않고 접어 둔다 — 위젯 삭제는 변수 GUID 맵 정리가 따로 필요하다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
#include "UObject/Package.h"

namespace FFLiveDockIconDesign
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_DOCK_ICON");
	const FString DockPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/WBP_FF_LiveUtilityDock");
	const FString MusicPackage = TEXT("/Game/FestaFiesta/UI/Audio/WBP_FF_PlaylistFlyout");
	const FString MusicIconPath = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Music_White_256.T_FF_LH_Icon_Music_White_256");
	const FLinearColor ShadowColor(0.f, 0.f, 0.f, 0.55f);

	FSlateBrush Circle(const FLinearColor& Fill)
	{
		FSlateBrush Result = Brush(Fill, 30.f);
		Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Result;
	}

	bool Backup(const FString& Package)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
		const FString Folder = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveDockIcon") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const bool bOk = IFileManager::Get().MakeDirectory(*Folder, true)
			&& IFileManager::Get().Copy(*(Folder / FPaths::GetCleanFilename(File)), *File) == COPY_OK;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s → %s"), LogPrefix, *Package, *Folder);
		return bOk;
	}

	/** 그림자 이미지를 아래에 깔고 아이콘을 위에 겹친 칸. 흰 아이콘이 밝은 바닥에서도 보이게 한다. */
	UOverlay* ShadowedIcon(UWidgetBlueprint& BP, UWidgetTree& Tree, const FString& Key, UImage& Icon)
	{
		UOverlay* Layers = MakeWidget<UOverlay>(BP, Tree, FName(TEXT("Overlay_") + Key + TEXT("Icon")));
		UImage* Shadow = MakeWidget<UImage>(BP, Tree, FName(TEXT("Image_") + Key + TEXT("Shadow")));
		Shadow->SetBrush(Icon.GetBrush());
		Shadow->SetColorAndOpacity(ShadowColor);
		Shadow->SetRenderTranslation(FVector2D(0.f, 1.5f));
		Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
		for (UImage* Layer : { Shadow, &Icon })
		{
			UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Layer);
			LayerSlot->SetHorizontalAlignment(HAlign_Center); LayerSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Layers;
	}

	void StyleLabel(UTextBlock& Label)
	{
		Label.Modify();
		Label.bIsVariable = true;
		Label.SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label.SetShadowOffset(FVector2D(0.f, 1.5f));
		Label.SetShadowColorAndOpacity(ShadowColor);
	}

	bool ApplyDock(UWidgetBlueprint& BP, UTexture2D& MusicIcon)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		if (Tree.FindWidget(TEXT("Image_Music")))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 도크 아이콘 정리가 이미 적용됐다. Designer 편집 보존"), LogPrefix); return true;
		}
		UButton* Buttons[] = { Cast<UButton>(Tree.FindWidget(TEXT("Button_SNS"))), Cast<UButton>(Tree.FindWidget(TEXT("Button_Music"))) };
		UVerticalBox* SnsContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Content_SNS")));
		UVerticalBox* MusicContent = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Content_Music")));
		UImage* SnsIcon = Cast<UImage>(Tree.FindWidget(TEXT("Image_SNS")));
		UTextBlock* Glyph = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MusicIcon")));
		UTextBlock* SnsLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_SNSLabel")));
		UTextBlock* MusicLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_MusicLabel")));
		if (!Buttons[0] || !Buttons[1] || !SnsContent || !MusicContent || !SnsIcon || !Glyph || !SnsLabel || !MusicLabel
			|| SnsIcon->GetParent() != SnsContent || Glyph->GetParent() != MusicContent)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 도크 구조가 예상과 다르다. 전체를 다시 만들지 않는다."), LogPrefix); return false;
		}
		if (!Backup(DockPackage)) { return false; }
		BP.Modify(); Tree.Modify();

		// 흰 원 대신 평소엔 그리지 않고, 마우스·누름에만 옅은 어두운 원으로 반응한다.
		for (UButton* Control : Buttons)
		{
			Control->Modify();
			FButtonStyle Style = Control->GetStyle();
			Style.Normal = FSlateNoResource();
			Style.Hovered = Circle(FLinearColor(0.f, 0.f, 0.f, 0.22f));
			Style.Pressed = Circle(FLinearColor(0.f, 0.f, 0.f, 0.34f));
			Control->SetStyle(Style);
		}

		// SNS 아이콘: 흰색으로 바꾸고 같은 자리에서 그림자 칸으로 감싼다.
		const int32 SnsIndex = SnsContent->GetChildIndex(SnsIcon);
		SnsIcon->Modify(); SnsIcon->bIsVariable = true;
		SnsIcon->SetColorAndOpacity(FLinearColor::White);
		SnsIcon->RemoveFromParent();
		UOverlay* SnsLayers = ShadowedIcon(BP, Tree, TEXT("SNS"), *SnsIcon);
		CastChecked<UVerticalBoxSlot>(SnsContent->InsertChildAt(SnsIndex, SnsLayers))->SetHorizontalAlignment(HAlign_Center);

		// 음악 아이콘: 글리프 자리 앞에 음표 이미지를 넣고 글리프는 접는다.
		// 크기는 SNS 아이콘 브러시와 맞춘다. SetDesiredSizeOverride 는 실행 중 Slate 에만 적용돼 저장되지 않는다.
		UImage* MusicImage = MakeWidget<UImage>(BP, Tree, TEXT("Image_Music"), true);
		FSlateBrush MusicBrush = SnsIcon->GetBrush();
		MusicBrush.SetResourceObject(&MusicIcon);
		MusicImage->SetBrush(MusicBrush);
		MusicImage->SetColorAndOpacity(FLinearColor::White);
		MusicImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlay* MusicLayers = ShadowedIcon(BP, Tree, TEXT("Music"), *MusicImage);
		CastChecked<UVerticalBoxSlot>(MusicContent->InsertChildAt(MusicContent->GetChildIndex(Glyph), MusicLayers))->SetHorizontalAlignment(HAlign_Center);
		Glyph->Modify(); Glyph->SetVisibility(ESlateVisibility::Collapsed);

		StyleLabel(*SnsLabel);
		StyleLabel(*MusicLabel);
		return true;
	}

	bool ApplyMusicPanel(UWidgetBlueprint& BP, UTexture2D& MusicIcon)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		if (Tree.FindWidget(TEXT("Image_TitleIcon")))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 음악 패널 아이콘 정리가 이미 적용됐다."), LogPrefix); return true;
		}
		UHorizontalBox* Header = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_Header")));
		UTextBlock* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_PanelTitle")));
		UBorder* Cover = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Cover")));
		UTextBlock* CoverGlyph = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CoverNote")));
		if (!Header || !Title || Title->GetParent() != Header || !Cover || !CoverGlyph || CoverGlyph->GetParent() != Cover)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 음악 패널 구조가 예상과 다르다."), LogPrefix); return false;
		}
		if (!Backup(MusicPackage)) { return false; }
		BP.Modify(); Tree.Modify(); Header->Modify(); Cover->Modify();

		// 제목: 글자에서 ♫ 를 빼고 같은 줄 앞에 음표 이미지를 둔다. 글꼴·크기는 그대로다.
		Title->Modify();
		FString TitleText = Title->GetText().ToString();
		TitleText.ReplaceInline(TEXT("♫"), TEXT(""));
		Title->SetText(FText::FromString(TitleText.TrimStartAndEnd()));
		UImage* TitleIcon = MakeWidget<UImage>(BP, Tree, TEXT("Image_TitleIcon"));
		TitleIcon->SetBrushFromTexture(&MusicIcon);
		TitleIcon->SetColorAndOpacity(Ink);
		USizeBox* TitleIconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_TitleIcon"));
		TitleIconSize->SetWidthOverride(30.f); TitleIconSize->SetHeightOverride(30.f); TitleIconSize->AddChild(TitleIcon);
		UHorizontalBoxSlot* IconSlot = CastChecked<UHorizontalBoxSlot>(Header->InsertChildAt(Header->GetChildIndex(Title), TitleIconSize));
		IconSlot->SetVerticalAlignment(VAlign_Center); IconSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(Title->Slot)) { TitleSlot->SetVerticalAlignment(VAlign_Center); }

		// 앨범 자리: 글리프를 접고 같은 칸에 음표 이미지를 겹친다.
		CoverGlyph->Modify(); CoverGlyph->RemoveFromParent(); CoverGlyph->SetVisibility(ESlateVisibility::Collapsed);
		UOverlay* CoverLayers = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Cover"));
		UImage* CoverIcon = MakeWidget<UImage>(BP, Tree, TEXT("Image_CoverIcon"));
		CoverIcon->SetBrushFromTexture(&MusicIcon);
		CoverIcon->SetColorAndOpacity(Ink);
		USizeBox* CoverIconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("SizeBox_CoverIcon"));
		CoverIconSize->SetWidthOverride(56.f); CoverIconSize->SetHeightOverride(56.f); CoverIconSize->AddChild(CoverIcon);
		for (UWidget* Layer : { static_cast<UWidget*>(CoverGlyph), static_cast<UWidget*>(CoverIconSize) })
		{
			UOverlaySlot* LayerSlot = CoverLayers->AddChildToOverlay(Layer);
			LayerSlot->SetHorizontalAlignment(HAlign_Center); LayerSlot->SetVerticalAlignment(VAlign_Center);
		}
		Cover->AddChild(CoverLayers);
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		UTexture2D* MusicIcon = LoadObject<UTexture2D>(nullptr, *MusicIconPath);
		if (!MusicIcon) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 음표 텍스처가 없다. 먼저 임포트한다: %s"), LogPrefix, *MusicIconPath); return; }
		const TPair<FString, bool (*)(UWidgetBlueprint&, UTexture2D&)> Steps[] = {
			{ DockPackage, &ApplyDock }, { MusicPackage, &ApplyMusicPanel } };
		for (const auto& Step : Steps)
		{
			UWidgetBlueprint* BP = LoadExistingBlueprint(Step.Key);
			if (!BP || !BP->WidgetTree) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: WBP가 없다: %s"), LogPrefix, *Step.Key); return; }
			if (BP->GetOutermost()->IsDirty()) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 WBP 보존: %s"), LogPrefix, *Step.Key); return; }
			const bool bAlreadyApplied = BP->WidgetTree->FindWidget(Step.Key == DockPackage ? TEXT("Image_Music") : TEXT("Image_TitleIcon")) != nullptr;
			if (!Step.Value(*BP, *MusicIcon)) { return; }
			if (!bAlreadyApplied && !SaveNewBlueprint(LogPrefix, *BP))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 저장 실패: %s"), LogPrefix, *Step.Key); return;
			}
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 도크 흰 원 제거·흰 아이콘+그림자·음표 이미지, 음악 패널 제목·앨범 음표 이미지"), LogPrefix);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveDockIconDesign"),
		TEXT("SNS·음악 도크의 흰 원을 없애고 음표 글리프를 이미지로 바꾼다. 처음 한 번만 적용한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}
