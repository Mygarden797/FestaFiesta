// 점검 패널의 기존 진행 사유/바로가기만 제목 아래 고정 안내띠로 옮긴다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepRequiredAttention"

namespace FFPrepRequiredAttentionBuilder
{
	using namespace FFUIWbpBuilder;
	using namespace FFPrepPanelSkyBuilder;
	constexpr const TCHAR* Prefix = TEXT("FF_PREP_REQUIRED_ATTENTION");
	constexpr const TCHAR* VersionKey = TEXT("FF_PrepRequiredAttentionVersion");
	const FString PanelPackage = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck");

	bool IsWithin(const UWidget* Widget, const UPanelWidget* Ancestor)
	{
		for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
		{
			if (Current == Ancestor) return true;
		}
		return false;
	}

	bool ValidateBindings(UWidgetBlueprint& BP, int32& OutCount)
	{
		OutCount = 0;
		if (!BP.GeneratedClass || BP.Status == BS_Error) return false;
		TStrongObjectPtr<UUserWidget> Instance(NewObject<UUserWidget>(GetTransientPackage(), BP.GeneratedClass.Get()));
		if (!Instance.IsValid() || !Instance->Initialize()) return false;
		for (const TCHAR* Name : { TEXT("Text_OpenGateReason"), TEXT("Text_OpenGateStaffLink"),
			TEXT("Button_OpenGateReason"), TEXT("Border_RequiredAttention"), TEXT("Text_RequiredAttentionTitle"),
			TEXT("Text_AdmissionFeeState"), TEXT("Button_AdmissionApply"), TEXT("Button_AdmissionReset") })
		{
			const UWidget* Template = BP.WidgetTree->FindWidget(Name);
			const auto* Property = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), Name);
			UObject* Bound = Property ? Property->GetObjectPropertyValue_InContainer(Instance.Get()) : nullptr;
			if (!Template || !Template->bIsVariable || !Bound || Bound != Instance->GetWidgetFromName(Name))
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_BINDING_FAIL: 실제 초기화/변수 연결 실패 %s"), Prefix, Name);
				return false;
			}
			++OutCount;
		}
		return true;
	}

	bool WriteValidation(UWidgetBlueprint& BP, int32 Count, bool bReuse)
	{
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("PrepRequiredAttention");
		const FString Report = FString::Printf(
			TEXT("{\"status\":\"passed\",\"token\":\"%s\",\"mode\":\"%s\",\"checked_bindings\":%d,\"original_binding_count\":%d,\"widget_guid_count\":%d,\"original_bindings_preserved\":true,\"original_variables_preserved\":true,\"initialized_bindings_valid\":true}"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits), bReuse ? TEXT("reuse") : TEXT("applied"),
			Count, BP.Bindings.Num(), BP.WidgetVariableNameToGuidMap.Num());
		if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
			!FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("binding-validation.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_BINDING_REPORT_FAIL: 검증 보고서 저장 실패"), Prefix);
			return false;
		}
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BINDINGS_OK: 실제 초기화한 위젯 바인딩 %d개 확인"), Prefix, Count);
		return true;
	}

	void Apply()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: PIE 중에는 점검 안내를 변경하지 않습니다."), Prefix);
			return;
		}
		UWidgetBlueprint* BP = LoadExistingBlueprint(PanelPackage);
		if (!BP || !BP->WidgetTree || !BP->GeneratedClass ||
			!BP->GeneratedClass->IsChildOf(UFF_PrepPanelCheck::StaticClass()))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 기존 점검 WBP가 필요합니다."), Prefix);
			return;
		}
		UPackage* Package = BP->GetOutermost();
		if (Package->GetMetaData().GetValue(BP, VersionKey) == TEXT("1"))
		{
			int32 CheckedBindings = 0;
			if (ValidateBindings(*BP, CheckedBindings) && WriteValidation(*BP, CheckedBindings, true))
			{
				UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: 적용된 안내띠와 이후 Designer 편집을 보존합니다."), Prefix);
			}
			else
			{
				UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_REUSE_VALIDATION_FAIL"), Prefix);
			}
			return;
		}
		UWidgetTree& Tree = *BP->WidgetTree;
		const TCHAR* NewNames[] = { TEXT("Border_RequiredAttention"), TEXT("HorizontalBox_RequiredAttentionContent"),
			TEXT("VerticalBox_RequiredAttentionCopy"), TEXT("Text_RequiredAttentionTitle") };
		bool bNameCollision = false;
		for (const TCHAR* Name : NewNames) bNameCollision |= Tree.FindWidget(Name) != nullptr;
		auto* Sections = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Sections")));
		auto* Header = Cast<USizeBox>(Tree.FindWidget(TEXT("SizeBox_Header")));
		auto* Scroll = Cast<UScrollBox>(Tree.FindWidget(TEXT("ScrollBox_Body")));
		auto* Footer = Cast<UVerticalBox>(Tree.FindWidget(TEXT("VerticalBox_Footer")));
		auto* ReasonRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("HorizontalBox_OpenGateReason")));
		auto* Reason = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_OpenGateReason")));
		auto* Action = Cast<UButton>(Tree.FindWidget(TEXT("Button_OpenGateReason")));
		auto* Link = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_OpenGateStaffLink")));
		// 기존 버튼이 사유 행 안에 있거나 Footer의 별도 자식인 두 편집 상태를 지원한다.
		// 그 외 사용자 계층은 추측해서 옮기지 않으며 입장료 상태/하단 버튼은 건드리지 않는다.
		if (Package->IsDirty() || bNameCollision || !Sections || !Header || !Scroll || !Footer ||
			!ReasonRow || !Reason || !Action || !Link || Header->GetParent() != Sections ||
			Scroll->GetParent() != Sections || Sections->GetChildIndex(Header) >= Sections->GetChildIndex(Scroll) ||
			!Cast<UVerticalBoxSlot>(Scroll->Slot) || Reason->GetParent() != ReasonRow ||
			Action->GetContent() != Link || !IsWithin(ReasonRow, Footer) || !IsWithin(Action, Footer) ||
			!ReasonRow->Slot || !Action->Slot || !Tree.FindWidget(TEXT("Text_AdmissionFeeState")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 미저장 에셋, 부분 적용 또는 예상과 다른 기존 계층입니다. 변경하지 않았습니다."), Prefix);
			return;
		}

		const FString File = FPackageName::LongPackageNameToFilename(PanelPackage, FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("UIBackups/PrepRequiredAttention") /
			FGuid::NewGuid().ToString(EGuidFormats::Digits);
		if (IFileManager::Get().IsReadOnly(*File) || !IFileManager::Get().MakeDirectory(*Backup, true) ||
			IFileManager::Get().Copy(*(Backup / FPaths::GetCleanFilename(File)), *File, false, false) != COPY_OK)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 점검 WBP 백업 또는 쓰기 권한 확인 실패."), Prefix);
			return;
		}
		const TMap<FName, FGuid> OriginalGuids = BP->WidgetVariableNameToGuidMap;
		const TArray<FDelegateEditorBinding> OriginalBindings = BP->Bindings;
		TMap<FName, UClass*> OriginalWidgets;
		TMap<FName, bool> OriginalVariables;
		Tree.ForEachWidget([&OriginalWidgets, &OriginalVariables](UWidget* Widget)
		{
			OriginalWidgets.Add(Widget->GetFName(), Widget->GetClass());
			OriginalVariables.Add(Widget->GetFName(), Widget->bIsVariable);
		});
		UPanelWidget* OldReasonParent = ReasonRow->GetParent();
		UPanelWidget* OldActionParent = Action->GetParent();
		BP->Modify(); Tree.Modify();
		Tree.ForEachWidget([](UWidget* Widget) { Widget->Modify(); if (Widget->Slot) Widget->Slot->Modify(); });

		auto* Strip = MakeWidget<UBorder>(*BP, Tree, TEXT("Border_RequiredAttention"), true);
		Strip->SetBrush(Brush(Color(255, 247, 228), 6.f, 1.f, Color(225, 190, 121)));
		Strip->SetPadding(FMargin(14.f, 10.f));
		auto* Content = MakeWidget<UHorizontalBox>(*BP, Tree, TEXT("HorizontalBox_RequiredAttentionContent"));
		auto* Copy = MakeWidget<UVerticalBox>(*BP, Tree, TEXT("VerticalBox_RequiredAttentionCopy"));
		Strip->AddChild(Content);
		Column(*Content, *Copy, FMargin(0.f, 0.f, 16.f, 0.f), true);
		CastChecked<UHorizontalBoxSlot>(Copy->Slot)->SetVerticalAlignment(VAlign_Center);
		auto* Title = Text(*BP, Tree, TEXT("Text_RequiredAttentionTitle"),
			LOCTEXT("RequiredPreview", "! 필수 준비 확인"), 16, Color(129, 82, 20), true);
		Title->SetAutoWrapText(true);
		Row(*Copy, *Title, FMargin(0.f, 0.f, 0.f, 4.f));
		// 본문/버튼 객체와 이름·스타일은 재사용하고 새 부모의 슬롯만 만든다.
		Row(*Copy, *ReasonRow);
		Column(*Content, *Action);
		CastChecked<UHorizontalBoxSlot>(Action->Slot)->SetVerticalAlignment(VAlign_Center);
		const FMargin BodyPadding = CastChecked<UVerticalBoxSlot>(Scroll->Slot)->GetPadding();
		auto* StripSlot = CastChecked<UVerticalBoxSlot>(Sections->InsertChildAt(Sections->GetChildIndex(Scroll), Strip));
		StripSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		StripSlot->SetHorizontalAlignment(HAlign_Fill);
		StripSlot->SetPadding(FMargin(BodyPadding.Left, 12.f, BodyPadding.Right, 0.f));
		// 권장 안내처럼 남아 있는 내용은 유지한다. 실제로 비게 된 전용 감싸기만 접는다.
		for (UPanelWidget* OldParent : { OldReasonParent, OldActionParent })
		{
			if (OldParent != Footer && OldParent != ReasonRow && OldParent->GetChildrenCount() == 0)
			{
				OldParent->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		bool bPreserved = BP->Status != BS_Error && BP->GeneratedClass && BP->Bindings.Num() == OriginalBindings.Num();
		for (int32 Index = 0; bPreserved && Index < OriginalBindings.Num(); ++Index)
		{
			bPreserved = FDelegateEditorBinding::StaticStruct()->CompareScriptStruct(&OriginalBindings[Index], &BP->Bindings[Index], 0);
		}
		for (const auto& Pair : OriginalGuids)
		{
			const FGuid* Current = BP->WidgetVariableNameToGuidMap.Find(Pair.Key);
			bPreserved &= Current && *Current == Pair.Value;
		}
		for (const auto& Pair : OriginalWidgets)
		{
			const UWidget* Current = BP->WidgetTree->FindWidget(Pair.Key);
			bPreserved &= Current && Current->GetClass() == Pair.Value && Current->bIsVariable == OriginalVariables.FindChecked(Pair.Key);
		}
		int32 CheckedBindings = 0;
		if (!bPreserved || !ValidateBindings(*BP, CheckedBindings))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: 컴파일 또는 기존 위젯/바인딩 보존 검증 실패. 저장하지 않았습니다. 백업=%s"), Prefix, *Backup);
			return;
		}
		Package->GetMetaData().SetValue(BP, VersionKey, TEXT("1"));
		if (!SaveAssetPackage(Prefix, *BP)) return;
		if (!WriteValidation(*BP, CheckedBindings, false)) return;
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_APPLIED: 상단 고정 안내띠 적용. 기존 위젯=%d 바인딩=%d GUID=%d 보존. 점검 WBP 1개만 저장. 백업=%s"),
			Prefix, OriginalWidgets.Num(), OriginalBindings.Num(), OriginalGuids.Num(), *FPaths::ConvertRelativePathToFull(Backup));
	}

	FAutoConsoleCommand Command(TEXT("FF.ApplyPrepRequiredAttentionDesign"),
		TEXT("점검 WBP의 기존 진행 사유/버튼을 제목 아래로 옮긴다. 헤더·본문·하단 치수와 재실행 Designer 편집을 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
