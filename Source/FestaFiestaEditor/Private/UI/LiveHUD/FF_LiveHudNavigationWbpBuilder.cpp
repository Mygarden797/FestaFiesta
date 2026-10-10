// 기존 라이브 HUD의 크기·스타일·자리를 보존하며 매출 순위 진입점과 화면 명칭만 추가/정리한다.
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/LiveHUD/FF_LiveTabButton.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveHudNavigation"

namespace FFLiveHudNavigationBuilder
{
	using namespace FFUIWbpBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_LIVE_HUD_NAVIGATION");
	constexpr const TCHAR* VersionKey = TEXT("FF_LiveHudNavigationVersion");
	const FString Folder = TEXT("/Game/FestaFiesta/UI/LiveHUD");

	bool HasVersion(UWidgetBlueprint& BP)
	{
		return BP.GetOutermost()->GetMetaData().GetValue(&BP, VersionKey) == TEXT("1");
	}

	bool SaveVersion(UWidgetBlueprint& BP)
	{
		BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, TEXT("1"));
		if (SaveNewBlueprint(Prefix, BP)) return true;
		BP.GetOutermost()->GetMetaData().RemoveValue(&BP, VersionKey);
		return false;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), Prefix); return;
		}
		auto* Hud = LoadExistingBlueprint(Folder / TEXT("WBP_FF_LiveHudScreen"));
		auto* Guests = LoadExistingBlueprint(Folder / TEXT("Panels/WBP_FF_LivePanelGuests"));
		auto* Facilities = LoadExistingBlueprint(Folder / TEXT("Panels/WBP_FF_LivePanelFacilities"));
		if (!Hud || !Guests || !Facilities || !Hud->WidgetTree || !Guests->WidgetTree || !Facilities->WidgetTree)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 라이브 HUD/현황/시설 상태 원본이 없습니다."), Prefix); return;
		}
		auto* FacilityTab = Cast<UFF_LiveTabButton>(Hud->WidgetTree->FindWidget(TEXT("Tab_Facilities")));
		auto* GuestTab = Cast<UFF_LiveTabButton>(Hud->WidgetTree->FindWidget(TEXT("Tab_Guests")));
		auto* Tabs = FacilityTab ? Cast<UHorizontalBox>(FacilityTab->GetParent()) : nullptr;
		auto* GuestTitle = Cast<UTextBlock>(Guests->WidgetTree->FindWidget(TEXT("Text_PanelTitle")));
		auto* FacilityTitle = Cast<UTextBlock>(Facilities->WidgetTree->FindWidget(TEXT("Text_PanelTitle")));
		if (!FacilityTab || !GuestTab || !Tabs || !GuestTitle || !FacilityTitle)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 위젯 구조가 예상과 다릅니다. 교체하지 않습니다."), Prefix); return;
		}
		const TArray<UWidgetBlueprint*> Assets = { Hud, Guests, Facilities };
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/LiveHudNavigation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		for (UWidgetBlueprint* BP : Assets)
		{
			if (HasVersion(*BP)) continue;
			if (BP->GetOutermost()->IsDirty())
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 편집을 먼저 저장하세요: %s"), Prefix, *BP->GetName()); return;
			}
			const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().MakeDirectory(*Backup, true) ||
				IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(Filename)), *Filename, false, false) != COPY_OK)
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 원본 백업 실패: %s"), Prefix, *Filename); return;
			}
		}
		if (!HasVersion(*Hud))
		{
			GuestTab->DisplayLabelOverride = LOCTEXT("Guests", "현황");
			if (!Hud->WidgetTree->FindWidget(TEXT("Tab_RevenueRanking")))
			{
				auto* Ranking = Cast<UFF_LiveTabButton>(MakeWidgetOfClass(*Hud, *Hud->WidgetTree, TEXT("Tab_RevenueRanking"), FacilityTab->GetClass()));
				if (!Ranking) return;
				Ranking->DisplayLabelOverride = LOCTEXT("RevenueRanking", "매출 순위");
				Ranking->DesignerIcon = FacilityTab->DesignerIcon;
				Ranking->DesignerIcon.SetResourceObject(LoadObject<UTexture2D>(nullptr,
					TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Revenue_White_256.T_FF_LH_Icon_Revenue_White_256")));
				Ranking->SetToolTipText(LOCTEXT("RankingTooltip", "시설별 오늘 매출과 축제 누적 매출 순위를 확인합니다."));
				auto* Slot = Tabs->AddChildToHorizontalBox(Ranking);
				if (const auto* Source = Cast<UHorizontalBoxSlot>(FacilityTab->Slot))
				{
					Slot->SetPadding(Source->GetPadding());
					Slot->SetSize(Source->GetSize());
					Slot->SetHorizontalAlignment(Source->GetHorizontalAlignment());
					Slot->SetVerticalAlignment(Source->GetVerticalAlignment());
				}
				// 에디터 트리 순서만 변경하므로 기존 탭의 슬롯·크기·스타일은 그대로 남는다.
				Tabs->ShiftChild(Tabs->GetChildIndex(FacilityTab) + 1, Ranking);
			}
			if (!SaveVersion(*Hud)) return;
		}
		if (!HasVersion(*Guests))
		{
			GuestTitle->SetText(LOCTEXT("GuestTitle", "현황"));
			if (!SaveVersion(*Guests)) return;
		}
		if (!HasVersion(*Facilities))
		{
			FacilityTitle->SetText(LOCTEXT("FacilityTitle", "시설 상태"));
			if (!SaveVersion(*Facilities)) return;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: 현황·시설 상태 명칭과 기존 매출 순위 화면 진입점 추가."), Prefix);
	}
	FAutoConsoleCommand Command(TEXT("FF.ApplyLiveHudNavigation"),
		TEXT("기존 HUD 편집을 보존하며 현황/시설 상태 명칭과 매출 순위 탭을 한 번 적용한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}
#undef LOCTEXT_NAMESPACE
