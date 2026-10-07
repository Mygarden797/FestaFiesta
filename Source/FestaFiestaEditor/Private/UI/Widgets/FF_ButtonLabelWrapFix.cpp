// 버튼 안 자동 줄바꿈 글자를 버튼 폭 전체에서 줄바꿈하게 고친다. 크기·색·글꼴 등 다른 편집은 그대로 둔다.
//
// 가운데 정렬 칸에 자동 줄바꿈 글자가 있으면 엔진은 **직전 프레임에 받은 폭**에서 줄을 바꾼다
// (5.7 `FSlateTextBlockLayout::CalculateWrappingWidth`). 칸 폭이 곧 글자 폭이라 한 번 좁게 그려지거나
// 글자가 길어지면(`전체` → `✓ 전체`) 줄바꿈이 풀리지 않는다. 칸을 Fill 로 바꾸고 가운데 맞춤은
// 글자 정렬로 하면 줄바꿈 폭이 버튼 폭이 된다.
#include "UI/Core/FF_UIWbpBuilderCommon.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace FFButtonLabelWrapFix
{
	using namespace FFUIWbpBuilder;
	constexpr const TCHAR* LogPrefix = TEXT("FF_BUTTON_LABEL_WRAP");

	struct FTarget
	{
		const TCHAR* Package;
		TArray<const TCHAR*> Labels;
	};

	/** 한 WBP의 지정 글자만 고친다. 이미 Fill 이면 건드리지 않는다. */
	bool FixPackage(const FTarget& Target, int32& OutChanged)
	{
		UWidgetBlueprint* BP = LoadExistingBlueprint(Target.Package);
		if (!BP || !BP->WidgetTree)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: WBP가 없습니다: %s"), LogPrefix, Target.Package); return false;
		}
		if (BP->GetOutermost()->IsDirty())
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 WBP를 보존합니다: %s"), LogPrefix, Target.Package); return false;
		}
		TArray<UTextBlock*> Pending;
		for (const TCHAR* Name : Target.Labels)
		{
			UTextBlock* Label = Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name));
			const UButtonSlot* LabelSlot = Label ? Cast<UButtonSlot>(Label->Slot) : nullptr;
			if (!LabelSlot)
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 버튼 안 글자를 찾지 못했습니다: %s.%s"), LogPrefix, Target.Package, Name); return false;
			}
			if (LabelSlot->GetHorizontalAlignment() != HAlign_Fill) { Pending.Add(Label); }
		}
		if (Pending.IsEmpty())
		{
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 이미 버튼 폭을 씁니다: %s"), LogPrefix, Target.Package); return true;
		}
		const FString File = FPackageName::LongPackageNameToFilename(Target.Package, TEXT(".uasset"));
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/ButtonLabelWrap") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (!IFileManager::Get().MakeDirectory(*Backup, true) || IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 백업 실패: %s"), LogPrefix, Target.Package); return false;
		}
		BP->Modify();
		for (UTextBlock* Label : Pending)
		{
			UButtonSlot* LabelSlot = CastChecked<UButtonSlot>(Label->Slot);
			Label->Modify(); LabelSlot->Modify();
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			Label->SetJustification(ETextJustify::Center);
		}
		if (!SaveNewBlueprint(LogPrefix, *BP))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 저장 실패: %s 백업=%s"), LogPrefix, Target.Package, *Backup); return false;
		}
		OutChanged += Pending.Num();
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_APPLIED: %s 글자 %d개, 백업=%s"), LogPrefix, Target.Package, Pending.Num(), *Backup);
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 종료 후 실행하세요."), LogPrefix); return; }
		const FTarget Targets[] = {
			// 셋업 프로그램 패널의 `{시설} · 건축목록에서 보기` 버튼.
			{ TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramFacilityChoice"), { TEXT("Text_Label") } },
			// 왼쪽 위 SNS 펼침 패널의 전체·긍정·아쉬움 필터.
			{ TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyout"),
				{ TEXT("Text_FilterAll"), TEXT("Text_FilterPositive"), TEXT("Text_FilterNegative") } },
		};
		int32 Changed = 0;
		for (const FTarget& Target : Targets)
		{
			if (!FixPackage(Target, Changed)) { return; }
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_DONE: 고친 글자 %d개"), LogPrefix, Changed);
	}

	FAutoConsoleCommand Command(TEXT("FF.FixButtonLabelWrap"),
		TEXT("프로그램 시설 버튼과 SNS 필터 글자가 버튼 폭 전체에서 줄바꿈하게 한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}
