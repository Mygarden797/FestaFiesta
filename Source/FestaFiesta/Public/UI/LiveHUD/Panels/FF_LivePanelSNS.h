// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelSNS.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * SNS 반응 한 건. **아직 이 값을 채우는 시스템이 없다.**
 *
 * 🔴 화면이 먼저 서고 데이터가 나중에 오는 자리라, 구조만 정해 두고 **더미 글을 만들지
 * 않는다.** 그럴듯한 가짜 반응을 띄우면 팀이 "SNS 는 됐다" 고 생각해서 진짜 API 가
 * 안 만들어진다. 지금은 비어 있고, 비어 있다는 사실을 화면에 적는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveSocialPost
{
	GENERATED_BODY()

	/** 누가 썼는지. 현지화 대상이라 `FText` 다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText Author;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText Body;

	/** -100 ~ +100. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 SentimentScore = INDEX_NONE;
};

/**
 * 라이브 `SNS` 패널.
 *
 * === 🔴 지금은 빈 화면이다. 그것이 의도다 ===
 * SNS 반응을 만드는 시스템이 **아직 없다** (`UI_API.md` B 표 · 소유자 미정).
 * 그래서 이 패널은 —
 *   · 무엇이 들어올 자리인지 보여 주고
 *   · **왜 비어 있는지**와 누가 채워야 하는지 적는다
 *
 * 가짜 반응을 띄우지 않는 이유는 그럴듯한 화면이 있으면 팀이 "SNS 는 됐다" 고 생각해서
 * 진짜 API 가 안 만들어지기 때문이다. 홍보 패널은 아래에 **진짜 경제**가 있어서 더미를
 * 허용했지만 (Gold 가 실제로 줄었다), 여기는 받칠 것이 아무것도 없다.
 *
 * === API 가 오면 바뀌는 것 ===
 * `UFF_FestivalViewModelSubsystem` 에 읽기 표면을 붙이고 `RefreshAll` 이 그것을 읽게 한다.
 * 위젯 트리와 이 클래스의 모양은 그대로다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - SNS"))
class FESTAFIESTA_API UFF_LivePanelSNS : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	/**
	 * 반응 목록을 손으로 넣는다. **소유자 API 가 생기면 이 함수를 지운다.**
	 *
	 * 화면 모양을 확인할 때만 쓴다 — 하단 바의 `Debug_SetPlaceholders` 와 같은 성격이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Debug")
	void Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts);

protected:
	virtual void NativeOnPanelOpened() override;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UVerticalBox> Box_Posts;

	/** 왜 비어 있는지. **이 패널의 본론이다.** */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_MissingNote;

private:
	void RefreshAll();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	/** 소유자 API 가 없어 비어 있다. 디버그로만 채워진다. */
	TArray<FFF_LiveSocialPost> Posts;
};
