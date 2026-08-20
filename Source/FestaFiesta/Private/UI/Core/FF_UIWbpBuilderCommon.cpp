// FF_UIWbpBuilderCommon.cpp

#include "UI/Core/FF_UIWbpBuilderCommon.h"

#if WITH_EDITOR

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
		// 원인은 콜스택 수십 줄 아래에 묻힌다. 2026-08-19 에 실제로 겪었다.
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
