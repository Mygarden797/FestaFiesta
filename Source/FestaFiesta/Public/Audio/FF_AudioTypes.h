// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_AudioTypes.generated.h"

class USoundBase;
class USoundConcurrency;

// 축제 사운드 관련 타입.
//
// 미니게임 오디오와는 별개다. 저쪽은 세션 단위라 게임이 끝나면 소리도 끝나는데,
// 축제 배경음은 계속 흐르면서 단계에 따라 바뀌어야 해서 모델이 다르다.

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaAudio, Log, All);

// 소리가 나야 하는 순간들.
//
// 번호를 띄엄띄엄 둔 이유는 중간에 끼워 넣을 때 기존 값이 밀리지 않게 하기 위해서다.
// 데이터 애셋이 이 값을 키로 저장하므로 번호가 바뀌면 매핑이 통째로 어긋난다.
UENUM(BlueprintType)
enum class EFF_AudioEvent : uint8
{
	None = 0                UMETA(Hidden),

	// ── 피날레 ──────────────────────────────────────────────────────────────
	FinaleBurst = 10        UMETA(DisplayName = "피날레 - 불꽃 터짐"),
	FinaleShowStarted = 11  UMETA(DisplayName = "피날레 - 연출 시작"),
	FinaleShowEnded = 12    UMETA(DisplayName = "피날레 - 연출 종료"),

	// ── 대기열 ──────────────────────────────────────────────────────────────
	QueueJoined = 30        UMETA(DisplayName = "대기열 - 줄 서기"),
	QueueRejected = 31      UMETA(DisplayName = "대기열 - 가입 거절"),
	QueueServiceStarted = 32 UMETA(DisplayName = "대기열 - 서비스 시작"),
	QueueServed = 33        UMETA(DisplayName = "대기열 - 이용 완료"),
	QueueAbandoned = 34     UMETA(DisplayName = "대기열 - 대기 포기"),

	// ── 건축 ────────────────────────────────────────────────────────────────
	BuildPlaced = 50        UMETA(DisplayName = "건축 - 시설 설치"),
	BuildRemoved = 51       UMETA(DisplayName = "건축 - 시설 철거"),
	BuildRejected = 52      UMETA(DisplayName = "건축 - 배치 불가"),
	BuildMoved = 53         UMETA(DisplayName = "건축 - 시설 이동"),
	BuildFacilityOpened = 54 UMETA(DisplayName = "건축 - 시설 개장"),
	BuildFacilityClosed = 55 UMETA(DisplayName = "건축 - 시설 폐장"),

	// ── UI ──────────────────────────────────────────────────────────────────
	UiClick = 70            UMETA(DisplayName = "UI - 클릭"),
	UiHover = 71            UMETA(DisplayName = "UI - 마우스 오버"),
	UiConfirm = 72          UMETA(DisplayName = "UI - 확인"),
	UiCancel = 73           UMETA(DisplayName = "UI - 취소"),
	UiError = 74            UMETA(DisplayName = "UI - 오류"),

	// ── 축제 진행 ───────────────────────────────────────────────────────────
	StageChanged = 90       UMETA(DisplayName = "진행 - 단계 전환"),
	DayStarted = 91         UMETA(DisplayName = "진행 - 낮 시작"),
	NightStarted = 92       UMETA(DisplayName = "진행 - 밤 시작")
};

// 배경음 갈래. 상황에 따라 크로스페이드로 갈아탄다.
UENUM(BlueprintType)
enum class EFF_AudioAmbience : uint8
{
	None = 0        UMETA(DisplayName = "없음"),
	Setup           UMETA(DisplayName = "준비 - 개장 전"),
	DayLive         UMETA(DisplayName = "축제 - 낮"),
	NightLive       UMETA(DisplayName = "축제 - 밤"),
	Finale          UMETA(DisplayName = "피날레"),
	Closing         UMETA(DisplayName = "정리 - 폐장"),

	// 최고 등급 3막 연출의 엔딩곡. 축제장 순회부터 끝까지 흐른다. 반복 재생을 끈 곡을 넣는다.
	// 맨 뒤에 붙인 이유는 뱅크가 이 값을 키로 저장해서, 중간에 끼우면 뒤의 곡이 한 칸씩 밀리기 때문이다.
	FinaleEnding    UMETA(DisplayName = "피날레 - 엔딩곡")
};

// 소리 갈래. 설정 창의 음량 슬라이더와 짝을 이룬다(FFF_AudioVolumes).
//
// 뱅크에서 소리마다 고를 수 있다. 기본값(효과음)으로 둔 소리는 이벤트 종류로 갈래를
// 정한다(UFF_AudioSubsystem::ResolveChannel). 소리를 넣을 때마다 칸을 고르지 않아도
// 불꽃은 불꽃놀이 슬라이더, 버튼 소리는 UI 슬라이더를 따르게 하려는 것이다.
UENUM(BlueprintType)
enum class EFF_AudioChannel : uint8
{
	Music       UMETA(DisplayName = "배경음"),
	Effects     UMETA(DisplayName = "효과음"),
	Ui          UMETA(DisplayName = "UI"),

	// 환경음은 따로 슬라이더를 두지 않고 효과음 슬라이더를 따른다. 아직 환경음 소리가 없어서
	// 빈 슬라이더가 되기 때문이다. 소리가 생기면 FFF_AudioVolumes 에 칸을 늘리면 된다.
	Ambience    UMETA(DisplayName = "환경음"),

	// 맨 뒤에 붙인다. 뱅크가 이 값을 숫자로 저장해서 중간에 끼우면 기존 소리의 갈래가 밀린다.
	Fireworks   UMETA(DisplayName = "불꽃놀이")
};

// 이벤트 하나에 붙는 소리 설정.
USTRUCT(BlueprintType)
struct FFF_AudioCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;

	// 여럿 넣으면 무작위로 고른다. 같은 소리가 반복되면 금방 질린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<TSoftObjectPtr<USoundBase>> Variations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	EFF_AudioChannel Channel = EFF_AudioChannel::Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeMultiplier = 1.f;

	// 재생마다 음높이를 조금씩 흔든다. 같은 소리를 연달아 낼 때 기계적으로 들리는 걸 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float PitchMin = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float PitchMax = 1.03f;

	// 이 간격 안에 같은 이벤트가 또 오면 무시한다.
	//
	// 불꽃이 한 번에 여러 발 터지거나 NPC 여럿이 동시에 줄을 서면 같은 소리가 겹쳐서
	// 찢어지는 소리가 난다. 0 이면 제한하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float MinRetriggerSeconds = 0.04f;

	// 동시에 몇 개까지 울릴지. 지정하지 않으면 엔진 기본 정책을 따른다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	bool IsValid() const
	{
		return !Sound.IsNull() || Variations.Num() > 0;
	}
};

// 배경음 하나.
USTRUCT(BlueprintType)
struct FFF_AudioAmbienceTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeMultiplier = 1.f;

	// 갈아탈 때 걸리는 시간. 0 이면 즉시 바뀌는데, 그러면 뚝 끊겨서 귀에 거슬린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 2.f;

	bool IsValid() const
	{
		return !Sound.IsNull();
	}
};
