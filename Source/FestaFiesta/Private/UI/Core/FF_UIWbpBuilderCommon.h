// FF_UIWbpBuilderCommon.h

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

// UI WBP 생성기들이 공유하는 헬퍼. **에디터 전용이다.**
//
// === 왜 뽑았나 ===
// 같은 헬퍼가 세 빌더에 복붙돼 있었다 (Report 589줄 · BuildCatalog 1,313줄 ·
// FacilityInfo 539줄). U1.5~U7 에서 생성기가 여섯 개 더 생기므로 지금 뽑지 않으면 아홉 벌이 된다.
// 뒤의 둘은 한광훈님 파일이라 건드리지 않았다 (AGENTS.md 규칙 3) — 이관 대상은 Report 뿐이다.
//
// === 생성기는 1회용 스캐폴딩이다 ===
// 하는 일은 `계층 · 위젯 이름 · BindWidget 대상 · 틀리기 쉬운 값`까지다.
// 색·패딩·폰트 마감은 디자이너에서 한다. **한 번 생성한 뒤 `.cpp` 는 레이아웃의 원본이 아니다.**
// Report 는 목업 `manifest.json` 이 좌표 원천이라 예외였고, 그 뒤의 화면들은 그게 없다.
// 좌표를 상수로 들고 있으면 손으로 조정한 값과 갈라져 어느 쪽이 진짜인지 모르는 죽은 문서가 된다.
// 근거와 폐기 전례: docs/memory/wbp-codegen-workflow.md
//
// === 로그 ===
// 카테고리는 `LogFFUIWbpBuilder` 하나를 공유하고 **어느 빌더인지는 접두어로 구분한다.**
// `UE_LOG` 가 컴파일 타임에 카테고리 타입을 요구해서 `FLogCategoryBase&` 를 인자로 넘길 수 없다.
// 접두어를 인자로 받는 이유는, 박아두면 다른 화면에서 그대로 써서 로그가 거짓말을 하기 때문이다.
// 관례: `FF_<화면>_UMG` — 예) `FF_REPORT_UMG` · `FF_COMMON_UMG`

#include "Blueprint/WidgetTree.h"
#include "Templates/Function.h"
#include "WidgetBlueprint.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UUserWidget;
class UWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogFFUIWbpBuilder, Log, All);

namespace FFUIWbpBuilder
{
	/** 레이아웃 조립 콜백. false 를 돌려주면 저장하지 않고 실패로 끝낸다. */
	using FBuildLayout = TFunction<bool(UWidgetBlueprint&, UWidgetTree&)>;

	/** `/Game/A/B` → `/Game/A/B.B`. LoadObject 가 요구하는 형태다. */
	FString MakeObjectPath(const FString& PackageName);

	/** 프로젝트 UI 기본 폰트 패키지. 없어도 생성은 성공한다 — 폰트는 마감 단계 항목이다. */
	FString GetUiFontPackage();

	/**
	 * 위젯을 만들고 이름을 블루프린트 변수로 등록한다.
	 *
	 * !! `Blueprint.OnVariableAdded(Name)` 을 빼면 안 된다 !!
	 * 위젯 블루프린트 컴파일러가 `WidgetVariableNameToGuidMap` 을 보기 때문에, 없으면 동적 생성한
	 * 트리가 컴파일·재저장 후 위젯 이름을 잃는다. 모르면 며칠 태우는 부분이다.
	 *
	 * `bVariable` 은 `BindWidget` 으로 잡을 대상에만 true 로 준다.
	 */
	template <typename TWidget>
	TWidget* MakeWidget(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name, const bool bVariable = false)
	{
		TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = bVariable;
			Blueprint.OnVariableAdded(Name);
		}
		return Widget;
	}

	/**
	 * 이미 생성된 WBP 클래스를 자식으로 넣을 때 쓴다.
	 * 선행 WBP 컴파일이 실패해 클래스가 없으면 **잘못된 기본 타입을 대신 만들지 않고** 실패를 전파한다.
	 */
	UUserWidget* MakeWidgetOfClass(
		UWidgetBlueprint& Blueprint,
		UWidgetTree& Tree,
		const FName Name,
		UClass* WidgetClass,
		const bool bVariable = true);

	/** 폰트가 없어도 크기와 색은 적용한다. */
	void StyleText(UTextBlock& Text, const int32 FontSize, const FLinearColor& Color, const FName TypefaceName);

	/**
	 * 캔버스에 절대 좌표로 배치한다.
	 * 앵커를 점(0,0)으로 두면 Offsets 의 Right·Bottom 이 크기로 해석되어 1920x1080 좌표와 1:1 이 된다.
	 * 화면 배율은 `UIScaleRule=ShortestSide` 가 처리한다 (Config/DefaultEngine.ini:90).
	 */
	UCanvasPanelSlot* PlaceOnCanvas(
		UCanvasPanel& Canvas,
		UWidget& Child,
		const float X,
		const float Y,
		const float W,
		const float H);

	/** 캔버스를 가득 채운다. 배경·레이어에 쓴다. */
	UCanvasPanelSlot* StretchOnCanvas(UCanvasPanel& Canvas, UWidget& Child);

	/** 없으면 nullptr. 로그를 남기지 않는다 — 없는 것이 정상인 호출부가 있다. */
	UWidgetBlueprint* LoadExistingBlueprint(const FString& PackageName);

	/** 컴파일된 클래스. 다른 WBP 를 자식으로 넣기 전에 이걸로 확인한다. */
	UClass* LoadGeneratedClass(const FString& PackageName);

	/** 구조 변경을 알린 뒤 컴파일하고 저장한다. `GeneratedClass` 에 최신 트리가 반영되게 하는 순서다. */
	bool SaveNewBlueprint(const TCHAR* LogPrefix, UWidgetBlueprint& Blueprint);

	/**
	 * **비파괴 핵심.** 패키지가 이미 있으면 레이아웃 콜백을 **아예 실행하지 않고** 그것을 돌려준다.
	 * 명령을 여러 번 실행해도 디자이너 수정이 보존된다. 덮어쓰는 경로를 여기 만들지 않는다.
	 */
	UWidgetBlueprint* EnsureWidgetBlueprint(
		const TCHAR* LogPrefix,
		const FString& PackageName,
		const FString& Folder,
		UClass& ParentClass,
		FBuildLayout&& BuildLayout);
}

#endif // WITH_EDITOR
