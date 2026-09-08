// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

	// 시설을 하나 설치했다.
	FacilityPlaced      UMETA(DisplayName = "시설 설치"),

	// 축제 단계가 넘어갔다.
	StageAdvanced       UMETA(DisplayName = "단계 진행"),

	// 밖에서 신호를 보내줄 때까지. 위 조건으로 표현이 안 되는 경우에 쓴다.
	ExternalSignal      UMETA(DisplayName = "외부 신호")
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

	// 어디를 가리킬지. 비어 있으면 강조하지 않는다.
	//
	// 위젯을 직접 참조하지 않고 이름으로 두는 이유는, 데이터가 UI 구조를 몰라야
	// UI 를 바꿔도 대사가 안 깨지기 때문이다. 실제 연결은 실행 중에 등록받는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FName HighlightAnchor;

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
