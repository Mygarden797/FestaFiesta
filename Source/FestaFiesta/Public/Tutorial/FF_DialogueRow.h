// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_DialogueRow.generated.h"

// CSV 한 줄에 대응하는 구조체.
//
// ─── 왜 데이터 애셋이 아니라 CSV 인가 ───────────────────────────────────────
// 데이터 애셋은 바이너리라 두 사람이 같은 파일을 고치면 한쪽을 버려야 한다.
// CSV 는 텍스트라 줄 단위로 합쳐지고, 엑셀에서 쓰고 찾아바꾸기도 된다.
// 나중에 번역을 맡길 때도 컬럼 하나 늘리면 끝난다.
//
// ─── 행 이름이 곧 라벨이다 ──────────────────────────────────────────────────
// CSV 는 첫 컬럼이 항상 행 이름이다. 그걸 분기 목적지로 쓰면 라벨을 따로
// 관리하지 않아도 되고, 이름이 중복되면 임포트할 때 걸린다.
//
// ─── 열거형을 문자열로 받는다 ───────────────────────────────────────────────
// 엑셀에서 Speech, Choice 처럼 쓴다. 오타가 나면 조용히 첫 값이 되므로
// 변환할 때 경고를 남긴다.
USTRUCT(BlueprintType)
struct FFF_DialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	// Speech / Choice / WaitForAction / Direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString Kind = TEXT("Speech");

	// 이 줄 다음에 갈 행 이름. 비어 있으면 바로 아래 행으로.
	//
	// 선택지로 갈라진 분기의 끝에 적어서 합류 지점을 정한다.
	// 안 적으면 반대편 분기까지 이어서 재생된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString NextJump;

	// 비어 있으면 내레이션이나 독백으로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString Text;

	// None / CameraMoved / CameraZoomed / PrepTabOpened / FacilityPlaced
	// / StageAdvanced / ExternalSignal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FString WaitCondition;

	// FullScreen / BoxOnly. 비어 있으면 화면 전체.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString ClickArea;

	// PrepTabOpened 일 때 기다릴 탭. Build / Program / Staff / Promotion / Finale / Check
	// 비어 있으면 아무 탭이나 열리면 통과한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FString ExpectedTab;

	// 강조할 대상 이름. 비어 있으면 강조하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FString HighlightAnchor;

	// 비워두면 건너뛸 수 있다. false 라고 적으면 남는다.
	//
	// 기본을 "건너뛸 수 있음" 으로 둔 이유는, 대부분의 줄이 서사라서다.
	// 조작 대기와 선택지는 이 값과 무관하게 자동으로 막힌다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool Skippable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString SkipSummary;

	// 선택지. 텍스트가 비어 있으면 그 자리부터 없는 것으로 본다.
	//
	// 배열 대신 컬럼을 늘린 이유는 CSV 가 평평한 표이기 때문이다.
	// 구분자로 우겨넣으면 엑셀에서 읽기 어렵고 쉼표가 섞이면 깨진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice1Text;

	// 이 선택지를 고르면 갈 행 이름. 비어 있으면 다음 줄로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice1Jump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice2Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice2Jump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice3Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString Choice3Jump;
};

// CSV 표를 대사 줄로 바꾼다.
//
// 변환을 런타임에 하는 이유는 중간 애셋을 하나 줄이기 위해서다.
// 80줄 남짓이라 변환 비용이 없고, CSV 를 고치면 바로 반영된다.
namespace FFF_DialogueTable
{
	// 표 전체를 줄 배열로. 행 순서는 CSV 순서를 그대로 따른다.
	FESTAFIESTA_API bool BuildLines(
		const UDataTable* Table,
		TArray<FFF_DialogueLine>& OutLines);

	// 한 행을 줄 하나로. 행 이름이 라벨이 된다.
	FESTAFIESTA_API FFF_DialogueLine BuildLine(FName RowName, const FFF_DialogueRow& Row);
}
