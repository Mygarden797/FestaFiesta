// FF_UIWbpBuilderCommon.cpp

#include "UI/Core/FF_UIWbpBuilderCommon.h"

#if WITH_EDITOR

#include "Animation/WidgetAnimation.h"
#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprintFactory.h"

DEFINE_LOG_CATEGORY(LogFFUIWbpBuilder);

namespace FFUIWbpBuilder
{
	FString MakeObjectPath(const FString& PackageName)
	{
		return FString::Printf(
			TEXT("%s.%s"),
			*PackageName,
			*FPackageName::GetLongPackageAssetName(PackageName));
	}

	FString GetUiFontPackage()
	{
		// 런타임 soft reference · 생성기 · 자동화 테스트 세 곳이 같은 문자열을 계약으로 쓴다.
		// 이름을 바꾸면 세 곳을 함께 고쳐야 한다.
		return TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard");
	}

	UUserWidget* MakeWidgetOfClass(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		UClass* WidgetClass,
		const bool bVariable)
	{
		if (!WidgetClass)
		{
			return nullptr;
		}

		UUserWidget* Widget = Tree.ConstructWidget<UUserWidget>(WidgetClass, Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	void StyleText(UTextBlock& Text, const int32 FontSize, const FLinearColor& Color, const FName TypefaceName)
	{
		FSlateFontInfo Font = Text.GetFont();

		if (UFont* UiFont = LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage())))
		{
			Font.FontObject = UiFont;
			Font.TypefaceFontName = TypefaceName;
		}

		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(FSlateColor(Color));
	}

	UCanvasPanelSlot* PlaceOnCanvas(
		UCanvasPanel& Canvas,
		UWidget& Child,
		const float X,
		const float Y,
		const float W,
		const float H)
	{
		UCanvasPanelSlot* PlacedSlot = Canvas.AddChildToCanvas(&Child);
		if (!PlacedSlot)
		{
			return nullptr;
		}

		PlacedSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		PlacedSlot->SetAlignment(FVector2D(0.f, 0.f));
		PlacedSlot->SetAutoSize(false);
		PlacedSlot->SetOffsets(FMargin(X, Y, W, H));
		return PlacedSlot;
	}

	UCanvasPanelSlot* StretchOnCanvas(UCanvasPanel& Canvas, UWidget& Child)
	{
		UCanvasPanelSlot* PlacedSlot = Canvas.AddChildToCanvas(&Child);
		if (!PlacedSlot)
		{
			return nullptr;
		}

		PlacedSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		PlacedSlot->SetOffsets(FMargin(0.f));
		return PlacedSlot;
	}

	UWidgetBlueprint* LoadExistingBlueprint(const FString& PackageName)
	{
		return LoadObject<UWidgetBlueprint>(nullptr, *MakeObjectPath(PackageName));
	}

	UClass* LoadGeneratedClass(const FString& PackageName)
	{
		const UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PackageName);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	bool SaveNewBlueprint(const TCHAR* LogPrefix, UWidgetBlueprint& Blueprint)
	{
		// !! 부모에 붙지 않은 위젯을 먼저 잡는다 !!
		//
		// MakeWidget 은 이름을 WidgetVariableNameToGuidMap 에 등록한다. 그런데 AddChild 를
		// 빼먹으면 그 위젯은 트리에 없다. 그러면 위젯 블루프린트 컴파일러가
		//   `Variable [X] was deleted but still has a GUID referenced by WidgetBlueprint`
		// ensure 로 터지고 **BindWidget 이 전부 실패해 화면이 통째로 안 뜬다.**
		// ensure 는 치명적이지 않아 저장까지 성공하므로, 깨진 .uasset 이 디스크에 남고
		// 원인은 콜스택 수십 줄 아래에 묻힌다. 2026-08-20 에 실제로 겪었다.
		//
		// 여기서 막으면 어느 위젯이 떠 있는지 이름으로 바로 알 수 있고, 저장하지 않는다.
		if (Blueprint.WidgetTree)
		{
			TSet<FName> ReachableNames;
			Blueprint.WidgetTree->ForEachWidget([&ReachableNames](UWidget* Widget)
			{
				if (Widget)
				{
					ReachableNames.Add(Widget->GetFName());
				}
			});

			// 애니메이션도 같은 GUID 맵을 쓴다. 트리에 없다고 고아로 잡으면 안 된다.
			for (const UWidgetAnimation* Animation : Blueprint.Animations)
			{
				if (Animation)
				{
					ReachableNames.Add(Animation->GetFName());
				}
			}

			TArray<FString> OrphanNames;
			for (const TPair<FName, FGuid>& Entry : Blueprint.WidgetVariableNameToGuidMap)
			{
				if (!ReachableNames.Contains(Entry.Key))
				{
					OrphanNames.Add(Entry.Key.ToString());
				}
			}

			if (OrphanNames.Num() > 0)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s — 부모에 붙지 않은 위젯이 %d개 있다: %s. "
						 "AddChild 를 빼먹은 것이다. 저장하지 않았다."),
					LogPrefix, *Blueprint.GetName(), OrphanNames.Num(),
					*FString::Join(OrphanNames, TEXT(", ")));
				return false;
			}
		}

		// WidgetTree 와 변수 GUID 변경을 구조 변경으로 알린 뒤 컴파일해야
		// GeneratedClass 에 최신 트리가 반영된다.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		if (Blueprint.Status == BS_Error)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 컴파일 실패."), LogPrefix, *Blueprint.GetPathName());
			return false;
		}

		Blueprint.MarkPackageDirty();
		UPackage* Package = Blueprint.GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Blueprint, *Filename, Args);
	}

	bool SaveAssetPackage(const TCHAR* LogPrefix, UObject& Asset)
	{
		UPackage* Package = Asset.GetOutermost();
		if (!Package)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 의 패키지를 찾을 수 없어 저장하지 않았다."), LogPrefix, *Asset.GetName());
			return false;
		}

		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *Filename, Args);
	}

	bool ResetWidgetTree(const TCHAR* LogPrefix, UWidgetBlueprint& Blueprint)
	{
		// 새 트리의 위젯 계약이 과거 템플릿 정보와 충돌하지 않도록 바인딩과 이름 GUID 맵을 먼저 비운다.
		// GUID 맵을 남겨 두면 `SaveNewBlueprint` 의 고아 위젯 가드가 옛 이름을 고아로 잡아 저장을 막는다.
		Blueprint.Modify();
		Blueprint.Bindings.Reset();

		// 🔴 **애니메이션 이름은 지우면 안 된다.**
		// 위젯 블루프린트 컴파일러가 애니메이션마다
		//   `ensure(WidgetBP->WidgetVariableNameToGuidMap.Contains(Animation->GetFName()))`
		// 를 확인하고, 없으면 `Animation [X] was added but did not get a GUID` 로 터진다.
		// 트리와 애니메이션은 별개 목록이라 트리를 갈아 끼워도 애니메이션은 남는다.
		// 2026-09-04 캐주얼 타이틀 교체에서 실제로 겪었다 (`Anim_Highlight`).
		TMap<FName, FGuid> PreservedAnimationGuids;
		for (const UWidgetAnimation* Animation : Blueprint.Animations)
		{
			if (!Animation)
			{
				continue;
			}

			const FName AnimationName = Animation->GetFName();
			const FGuid* Existing = Blueprint.WidgetVariableNameToGuidMap.Find(AnimationName);
			PreservedAnimationGuids.Add(AnimationName, Existing ? *Existing : FGuid::NewGuid());
		}

		Blueprint.WidgetVariableNameToGuidMap.Reset();
		Blueprint.WidgetVariableNameToGuidMap.Append(PreservedAnimationGuids);

		if (UWidgetTree* OldTree = Blueprint.WidgetTree)
		{
			// 같은 Outer 아래에 `WidgetTree` 라는 이름을 다시 쓸 수 있도록 옛 객체를 밀어낸다.
			// Rename 이 실패하면 기존 트리를 그대로 두고 중단한다 — 반쯤 지워진 상태를 만들지 않는다.
			const FName DiscardedName = MakeUniqueObjectName(
				&Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedWidgetTree"));
			if (!OldTree->Rename(
				*DiscardedName.ToString(), &Blueprint, REN_DontCreateRedirectors | REN_NonTransactional))
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 의 WidgetTree 를 교체하지 못했다."), LogPrefix, *Blueprint.GetPathName());
				return false;
			}

			// 옛 트리가 새 패키지에 직렬화되지 않게 한다. 실제 회수는 GC 에 맡긴다.
			OldTree->SetFlags(RF_Transient);
		}

		// 컴파일러가 기대하는 고정 이름으로 빈 트리를 만든다. 채우는 것은 호출부의 레이아웃 함수다.
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional);
		return Blueprint.WidgetTree != nullptr;
	}

	UWidgetBlueprint* ReplaceWidgetBlueprint(
		const TCHAR* LogPrefix,
		const FString& PackageName,
		const FString& Folder,
		UClass& ParentClass,
		FBuildLayout&& BuildLayout)
	{
		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PackageName);
		if (!Blueprint)
		{
			// 아직 없으면 비파괴 생성 경로와 같은 결과를 만든다.
			return EnsureWidgetBlueprint(LogPrefix, PackageName, Folder, ParentClass, MoveTemp(BuildLayout));
		}

		if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(&ParentClass))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 의 부모 클래스가 예상(%s)과 달라 손대지 않았다."),
				LogPrefix, *PackageName, *ParentClass.GetName());
			return nullptr;
		}

		if (!ResetWidgetTree(LogPrefix, *Blueprint) ||
			!BuildLayout(*Blueprint, *Blueprint->WidgetTree) ||
			!SaveNewBlueprint(LogPrefix, *Blueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 에 레이아웃을 적용하지 못했다."), LogPrefix, *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REPLACE: %s 레이아웃을 교체했다."), LogPrefix, *PackageName);
		return Blueprint;
	}

	UWidgetBlueprint* EnsureWidgetBlueprint(
		const TCHAR* LogPrefix,
		const FString& PackageName,
		const FString& Folder,
		UClass& ParentClass,
		FBuildLayout&& BuildLayout)
	{
		// 비파괴 경로: 패키지가 있으면 레이아웃 콜백을 절대 실행하지 않는다.
		// 명령을 여러 번 실행해도 디자이너 수정이 보존된다.
		if (FPackageName::DoesPackageExist(PackageName))
		{
			UWidgetBlueprint* Existing = LoadExistingBlueprint(PackageName);
			if (!Existing)
			{
				UE_LOG(LogFFUIWbpBuilder, Error,
					TEXT("%s_FAIL: %s 이(가) 있지만 로드할 수 없다. 수정하지 않았다."),
					LogPrefix, *PackageName);
				return nullptr;
			}

			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REUSE: %s 이미 존재. 디자이너 수정을 보존했다."), LogPrefix, *PackageName);
			return Existing;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = &ParentClass;

		FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
			AssetName, Folder, UWidgetBlueprint::StaticClass(), Factory));
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 생성 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		if (!BuildLayout(*Blueprint, *Blueprint->WidgetTree))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: %s 레이아웃 구성 실패. 저장하지 않았다."), LogPrefix, *PackageName);
			return nullptr;
		}

		if (!SaveNewBlueprint(LogPrefix, *Blueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: %s 저장 실패."), LogPrefix, *PackageName);
			return nullptr;
		}

		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_CREATE: %s 생성 완료."), LogPrefix, *PackageName);
		return Blueprint;
	}
}

#endif // WITH_EDITOR
