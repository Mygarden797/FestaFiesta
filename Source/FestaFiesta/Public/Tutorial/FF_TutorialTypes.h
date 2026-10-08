// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "FF_TutorialTypes.generated.h"

// 튜토리얼과 대화 관련 타입.
//
// 대사와 조작 안내를 같은 흐름에 둔다. 대사만 있으면 읽고 넘기는 컷신이 되고,
// 조작만 있으면 왜 하는지 모르는 지시가 된다. 둘을 한 줄 단위로 섞을 수 있어야
// "이래서 이걸 누르세요" 가 성립한다.

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaTutorial, Log, All);

// 한 줄이 무엇을 하는가.
UENUM(BlueprintType)
enum class EFF_DialogueLineKind : uint8
{
	// 대사만. 클릭하면 다음으로.
	Speech          UMETA(DisplayName = "대사"),

	// 선택지. 고를 때까지 기다린다.
	Choice          UMETA(DisplayName = "선택지"),

	// 플레이어가 무언가 하기를 기다린다. 대사 없이 안내만 띄운다.
	WaitForAction   UMETA(DisplayName = "조작 대기"),

	// 화면 전환, 암전 같은 연출. 대사 없이 지나간다.
	Direction       UMETA(DisplayName = "연출")
};

/* 대사를 넘기려면 어디를 눌러야 하는가.

   비주얼 노벨은 화면 아무 데나 눌러 넘기는 게 기본이다. 그런데 운영 화면 위에서
   그렇게 하면 시설을 고르려고 누른 것까지 대사 넘기기로 먹힌다.

   그래서 줄마다 정한다. 스토리 구간은 어디나, 운영 구간은 대사창만.
   "어디를 눌렀나" 로 결과가 갈리게 두면 플레이어가 예측할 수 없다.
*/
UENUM(BlueprintType)
enum class EFF_DialogueClickArea : uint8
{
	// 화면 전체. 스토리 구간에 쓴다.
	FullScreen  UMETA(DisplayName = "화면 전체"),

	// 대사창 안에서만. 운영 화면 위에 대사가 뜰 때 쓴다.
	BoxOnly     UMETA(DisplayName = "대사창만")
};

// 무엇을 기다리는가.
//
// 조건을 문자열이 아니라 열거형으로 두는 이유는, 오타가 나면 영영 진행이 안 되기
// 때문이다. 문자열이면 컴파일도 되고 실행도 되는데 조건이 영원히 안 맞는다.
UENUM(BlueprintType)
enum class EFF_TutorialWaitCondition : uint8
{
	None                UMETA(DisplayName = "없음"),

	// 카메라를 일정 거리 이상 움직였다.
	CameraMoved         UMETA(DisplayName = "카메라 이동"),

	// 카메라를 일정 비율 이상 확대·축소했다.
	CameraZoomed        UMETA(DisplayName = "카메라 줌"),

	// 준비 화면의 특정 탭을 열었다.
	PrepTabOpened       UMETA(DisplayName = "준비 탭 열기"),

	// 시설을 하나 설치했다. 줄의 WaitTarget 에 시설 정의 이름을 적으면 그 시설만 센다.
	FacilityPlaced      UMETA(DisplayName = "시설 설치"),

	// 축제 단계가 넘어갔다.
	StageAdvanced       UMETA(DisplayName = "단계 진행"),

	// 밖에서 신호를 보내줄 때까지. 위 조건으로 표현이 안 되는 경우에 쓴다.
	ExternalSignal      UMETA(DisplayName = "외부 신호"),

	/* 아래는 2026-10-07 에 더했다(튜토리얼 직접 해 보기). 표는 이름으로 읽으므로 뒤에 붙인다.

	   전부 튜토리얼이 상대 시스템의 공개 값을 주기적으로 읽어 판정한다(FF_TutorialSubsystem_Watch.cpp).
	   상대 시스템이 튜토리얼을 알 필요가 없다.
	*/

	// 위에서 보는 카메라를 Q·E 로 돌렸다(한 칸의 절반 넘게).
	CameraRotated       UMETA(DisplayName = "카메라 회전"),

	// Tab 으로 시점(위에서 보기 ↔ 캐릭터)을 바꿨다.
	ViewToggled         UMETA(DisplayName = "시점 전환"),

	// 배치 탭에서 WaitTarget 카테고리(Booth · Info · Rest · Performance …)를 골랐다.
	BuildCategorySelected UMETA(DisplayName = "건축 카테고리 선택"),

	// WaitTarget 시설의 설치 미리보기를 시작했다(카드를 눌렀다). 비우면 아무 시설이나.
	PlacementStarted    UMETA(DisplayName = "설치 미리보기 시작"),

	// 설치 미리보기를 Q·E 로 돌렸다.
	PlacementRotated    UMETA(DisplayName = "설치 미리보기 회전"),

	// WaitTarget 조합(DT_FacilitySynergies 의 행 이름, 예: BurgerPicnic)이 지금 성립했다.
	SynergyActive       UMETA(DisplayName = "조합 성립"),

	// 안내 시설에 스태프를 한 명 이상 배정했다(출동할 수 있는 스태프가 생겼다).
	StaffAssigned       UMETA(DisplayName = "스태프 배정"),

	// 설치 미리보기에서 시설 목록으로 돌아왔다. 설치는 연속 배치라 놓은 뒤에도 미리보기가 남고,
	// 그동안 카테고리 목록은 접혀 있다. 다른 시설을 고르게 하기 전에 거친다.
	BuildCatalogOpened  UMETA(DisplayName = "시설 목록 열림")
};

// 선택지 하나.
USTRUCT(BlueprintType)
struct FFF_DialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Text;

	// 이 선택지를 고르면 여기로 간다. 비어 있으면 그냥 다음 줄로.
	//
	// 분기를 라벨로 두는 이유는 줄 번호로 두면 중간에 대사를 끼워 넣을 때
	// 전부 어긋나기 때문이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName JumpToLabel;
};

// 대사 한 줄.
USTRUCT(BlueprintType)
struct FFF_DialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EFF_DialogueLineKind Kind = EFF_DialogueLineKind::Speech;

	// 이 줄로 건너뛸 수 있게 하는 이름. 비어 있어도 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName Label;

	// 이 줄이 끝나면 갈 곳. 비어 있으면 바로 다음 줄로 간다.
	//
	// 분기가 합쳐지는 자리를 지정하는 데 쓴다. 선택지로 갈라진 뒤 그냥 두면
	// 반대편 분기까지 이어서 재생된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName NextLabel;

	// 화자. 비어 있으면 내레이션이나 독백으로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = "true"))
	FText Text;

	// 선택지. Kind 가 Choice 일 때만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FFF_DialogueChoice> Choices;

	// 무엇을 기다리는가. Kind 가 WaitForAction 일 때만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	EFF_TutorialWaitCondition WaitCondition = EFF_TutorialWaitCondition::None;

	/* 이 줄에서 보여줄 배경 그림.

	   비어 있으면 배경을 바꾸지 않는다. 앞 줄의 그림이 그대로 남는다.
	   줄마다 적지 않아도 되게 하려는 것이다. 한 장면이 여러 줄로 이어질 때
	   같은 그림을 매 줄에 적으면 나중에 바꿀 때 전부 고쳐야 한다.

	   비우고 싶으면 None 이라고 적는다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	// 배경을 지울지. BackgroundImage 가 비어 있고 이 값이 참이면 배경이 사라진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bClearBackground = false;

	/* 어느 탭이 열리기를 기다리는가. PrepTabOpened 일 때만 쓴다.

	   조건과 따로 두는 이유는, 조건 열거형에 탭 종류를 전부 넣으면 값이 여섯 개 늘어서다.
	   무엇을 기다리는지와 어느 것을 기다리는지는 다른 질문이다.

	   비어 있으면 아무 탭이나 열리면 통과한다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName ExpectedTab;

	/* 조작 대기 조건의 대상. 조건마다 뜻이 다르다(EFF_TutorialWaitCondition 주석).

	   카테고리 이름(BuildCategorySelected), 시설 정의 이름(PlacementStarted · FacilityPlaced),
	   조합 이름(SynergyActive). ExpectedTab 과 나눈 이유는 그 칸이 준비 탭 이름만 받기 때문이다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName WaitTarget;

	// 어디를 가리킬지. 비어 있으면 강조하지 않는다.
	//
	// 위젯을 직접 참조하지 않고 이름으로 두는 이유는, 데이터가 UI 구조를 몰라야
	// UI 를 바꿔도 대사가 안 깨지기 때문이다. 실제 연결은 실행 중에 등록받는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName HighlightAnchor;

	/* 강조를 테두리만 그릴지. 켜면 화면을 어둡게 덮지 않는다.

	   덮개는 플레이어가 무언가 눌러야 할 때 시선을 모으는 것이다. 읽고 넘기는 대사에서
	   덮으면 대사를 읽는 내내 시야를 가린다. 그렇다고 강조를 빼면 "여기가 예산이에요" 처럼
	   가리키며 설명하는 줄을 만들 수 없다. 그래서 그런 줄은 테두리만 그린다.
	   표에서 읽을 때는 조작을 기다리지 않는 줄이면 자동으로 켜진다(FF_DialogueRow).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bHighlightOutlineOnly = false;

	/* 이 줄을 넘기려면 어디를 눌러야 하는가.

	   기본이 화면 전체인 이유는 대사의 대부분이 스토리라서다.
	   운영 화면이 함께 보이는 구간에서만 대사창으로 좁힌다.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EFF_DialogueClickArea ClickArea = EFF_DialogueClickArea::FullScreen;

	// 이 줄을 건너뛸 수 있는가.
	//
	// 조작을 기다리는 줄은 자동으로 건너뛸 수 없다. 건너뛰면 진행이 막히기 때문이다.
	// 서사 대사만 건너뛰고 안내는 남기려는 것이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bSkippable = true;

	// 건너뛸 때 대신 보여줄 짧은 안내. 비어 있으면 그냥 넘어간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText SkipSummary;

	bool RequiresPlayerAction() const
	{
		return Kind == EFF_DialogueLineKind::WaitForAction
			&& WaitCondition != EFF_TutorialWaitCondition::None;
	}

	bool CanSkip() const
	{
		// 조작 대기와 선택지는 건너뛸 수 없다. 진행이 막힌다.
		if (Kind == EFF_DialogueLineKind::WaitForAction
			|| Kind == EFF_DialogueLineKind::Choice)
		{
			return false;
		}
		return bSkippable;
	}
};
