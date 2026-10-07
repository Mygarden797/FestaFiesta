// P6 화제성 카드만 추가한다. 기존 운영 위젯·CDO·HUD 배치는 그대로 보존한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
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
#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveOperationsBuzz"

namespace FFLiveOperationsBuzzWbpBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_LIVE_OPERATIONS_BUZZ");
	constexpr const TCHAR* VersionKey = TEXT("FF.LiveOperationsBuzz.Version");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelOperations");

	UTextBlock* Wrapped(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
		const FText& Preview, int32 FontSize, FLinearColor Tint, bool bVariable = true)
	{
		UTextBlock* Result = Text(BP, Tree, Name, Preview, FontSize, Tint, bVariable);
		Result->SetAutoWrapText(true);
		Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Result;
	}

	bool Build(UWidgetBlueprint& BP, UVerticalBox& Parent, UBorder& Goal)
	{
		UWidgetTree& Tree = *BP.WidgetTree;
		const int32 Index = Parent.GetChildIndex(&Goal);
		UVerticalBoxSlot* ExistingSlot = Cast<UVerticalBoxSlot>(Goal.Slot);
		if (Index == INDEX_NONE || !ExistingSlot) { return false; }
		BP.Modify(); Tree.Modify(); Parent.Modify(); Goal.Modify(); ExistingSlot->Modify();
		UHorizontalBox* Columns = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Ops_ProgressColumns"));
		// 기존 세로 슬롯을 템플릿으로 복사해 사용자가 정한 패딩·정렬·채움 설정을 유지한다.
		// 먼저 새 슬롯을 만든 뒤 목표 카드를 떼므로 다른 형제 위젯의 슬롯은 바뀌지 않는다.
		if (!Parent.InsertChildAt(Index, Columns, ExistingSlot)) { return false; }
		Goal.RemoveFromParent();
		Column(*Columns, Goal, FMargin(0.f), true);

		UBorder* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Ops_BuzzBorder"));
		Card->SetBrush(Goal.Background);
		Card->SetBrushColor(Goal.GetBrushColor());
		Card->SetPadding(Goal.GetPadding());
		UVerticalBox* Body = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Ops_BuzzBody"));
		Card->AddChild(Body);
		Column(*Columns, *Card, FMargin(16.f, 0.f, 0.f, 0.f), true);

		UTextBlock* Title = Wrapped(BP, Tree, TEXT("Ops_BuzzTitle"), LOCTEXT("Title", "화제성"), 22, Ink, false);
		if (const UTextBlock* GoalTitle = Cast<UTextBlock>(Tree.FindWidget(TEXT("Ops_GoalTitle"))))
		{
			Title->SetFont(GoalTitle->GetFont());
			Title->SetColorAndOpacity(GoalTitle->GetColorAndOpacity());
		}
		Row(*Body, *Title);
		Row(*Body, *Rule(BP, Tree, TEXT("Ops_BuzzDivider")), FMargin(0.f, 8.f, 0.f, 10.f));
		Row(*Body, *Wrapped(BP, Tree, TEXT("Text_BuzzNote"), LOCTEXT("Note", "축제를 향한 관심도"), 16, Muted), FMargin(0.f, 0.f, 0.f, 8.f));
		UHorizontalBox* Numbers = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Ops_BuzzNumbers"));
		Column(*Numbers, *Wrapped(BP, Tree, TEXT("Text_BuzzValue"), LOCTEXT("ValuePreview", "68 / 100"), 29, Ink), FMargin(0.f, 0.f, 12.f, 0.f), true);
		UTextBlock* Change = Wrapped(BP, Tree, TEXT("Text_BuzzChange"), LOCTEXT("ChangePreview", "오늘 +8"), 17, Muted);
		Change->SetJustification(ETextJustify::Right);
		Column(*Numbers, *Change);
		CastChecked<UHorizontalBoxSlot>(Change->Slot)->SetVerticalAlignment(VAlign_Center);
		Row(*Body, *Numbers, FMargin(0.f, 0.f, 0.f, 12.f));
		UProgressBar* Bar = MakeWidget<UProgressBar>(BP, Tree, TEXT("Bar_Buzz"), true);
		FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
		BarStyle.BackgroundImage = Brush(SkyWash, 8.f, 1.f, SkyLine);
		BarStyle.FillImage = Brush(FLinearColor::White, 8.f);
		BarStyle.MarqueeImage = BarStyle.FillImage;
		Bar->SetWidgetStyle(BarStyle);
		Bar->SetFillColorAndOpacity(Color(0, 167, 184));
		Bar->SetPercent(.68f);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		USizeBox* BarSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Ops_BuzzProgressSize"));
		BarSize->SetHeightOverride(18.f);
		BarSize->AddChild(Bar);
		Row(*Body, *BarSize, FMargin(0.f, 0.f, 0.f, 10.f));
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return;
		}
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(UFF_LivePanelOperations::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 운영 패널 WBP가 필요합니다."), LogPrefix); return;
		}
		UPackage* Package = BP->GetOutermost();
		if (Package->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 운영·화제성 Designer 편집값을 보존합니다."), LogPrefix); return;
		}
		if (Package->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 운영 WBP를 보존합니다. 저장 후 실행하세요."), LogPrefix); return;
		}
		UWidgetTree& Tree = *BP->WidgetTree;
		const TCHAR* ReservedNames[] = {
			TEXT("Ops_ProgressColumns"), TEXT("Ops_BuzzBorder"), TEXT("Ops_BuzzBody"), TEXT("Ops_BuzzTitle"),
			TEXT("Ops_BuzzDivider"), TEXT("Ops_BuzzDivider_Size"), TEXT("Ops_BuzzNumbers"), TEXT("Ops_BuzzProgressSize"),
			TEXT("Text_BuzzValue"), TEXT("Text_BuzzChange"), TEXT("Text_BuzzNote"), TEXT("Bar_Buzz")
		};
		for (const TCHAR* Name : ReservedNames)
		{
			if (Tree.FindWidget(Name))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 수동 편집/부분 적용 위젯 이름을 덮어쓰지 않습니다: %s"), LogPrefix, Name); return;
			}
		}
		UBorder* Goal = Cast<UBorder>(Tree.FindWidget(TEXT("Ops_GoalBorder")));
		UVerticalBox* Parent = Goal ? Cast<UVerticalBox>(Goal->GetParent()) : nullptr;
		if (!Goal || !Parent || !Cast<UVerticalBoxSlot>(Goal->Slot))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 일일 목표 카드 구조를 확인하세요. 전체 화면을 재생성하지 않습니다."), LogPrefix); return;
		}
		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveOperationsBuzz") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (IFileManager::Get().IsReadOnly(*File) || !IFileManager::Get().MakeDirectory(*Backup, true)
			|| IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 운영 WBP 백업 또는 쓰기 가능 여부를 확인하세요."), LogPrefix); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BACKUP: %s"), LogPrefix, *FPaths::ConvertRelativePathToFull(Backup));
		if (!Build(*BP, *Parent, *Goal) || !SaveNewBlueprint(LogPrefix, *BP))
		{
			Package->GetMetaData().RemoveValue(BP, VersionKey);
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 화제성 카드 적용/저장 실패. 백업=%s"), LogPrefix, *Backup); return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 기존 운영 패널에 화제성 카드만 추가했습니다."), LogPrefix);
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveOperationsBuzzDesign"),
		TEXT("운영 패널의 일일 목표 옆에 화제성 카드만 처음 한 번 추가한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
