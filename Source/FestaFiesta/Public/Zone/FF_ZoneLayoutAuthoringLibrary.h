// FF_ZoneLayoutAuthoringLibrary.h
// 역할: 에디터 Python과 블루프린트가 실제 시설 배치 규칙으로 검증·설치하고 반복 검사 배치를 저장하게 한다.
#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_ZoneLayoutAuthoringLibrary.generated.h"

class UWorld;

/** 배치 규칙을 복제하지 않는 에디터 제작용 진입점. 게임 빌드에는 제작 함수를 노출하지 않는다. */
UCLASS()
class FESTAFIESTA_API UFF_ZoneLayoutAuthoringLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** 기존 ZoneSystem의 지면·공간·경로·외부 정책 검증 결과를 변경 없이 반환한다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Layout")
	static FFFZSPlacementValidation ValidatePlacement(UWorld* World, const FFFZSCommand& Command);

	/** 기존 명령 실행 경로를 사용하여 검증, 정책 예약, 시설 생성과 실패 처리를 함께 수행한다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Layout")
	static FFFZSCommandResult ExecutePlacement(UWorld* World, const FFFZSCommand& Command);

	/** 검증을 마친 미개장 시설만 저장하고 재읽기로 전체 배치를 비교한다. 슬롯명은 문자·숫자·밑줄·하이픈을 허용한다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Editor|Layout")
	static bool SaveLayout(UWorld* World, const FString& SlotName, bool bOverwrite, TArray<FText>& OutErrors);
#endif
};
