// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_IncidentToast.generated.h"

class UBorder;
class UCanvasPanel;
class UFont;
class UTextBlock;
class UVerticalBox;

enum class EFF_IncidentToastKind : uint8
{
	Good,
	Danger,
	Neutral
};

/**
 * 사건 처리 결과를 화면 위쪽 가운데에 잠깐 띄우는 알림 묶음. WBP 없이 C++ 만으로 트리를 만든다.
 *
 * Show 를 부를 때마다 알림 한 줄을 맨 위에 더하고, 이전 줄은 아래로 밀린다. 동시에 MaxEntries(3)줄까지만 보이고,
 * 넘치면 가장 오래된 줄(맨 아래)부터 지운다. 줄마다 DisplaySeconds 동안 따로 보이다가 마지막 FadeSeconds 동안 흐려지며 사라진다.
 * 시간은 위젯 틱의 실제 시간으로 센다. 배속·일시정지와 상관없이 같은 시간 동안 읽을 수 있어야 한다.
 * 전체가 HitTestInvisible 이라 월드 클릭을 막지 않는다. 한 줄도 없으면 Collapsed.
 *
 * 모양은 UFF_UIPalette(LightTeal)를 따른다. 카드는 PaperStrong 바탕 · Line 테두리 · 카드 모서리 6px, 본문은 Ink.
 * 결과는 색만으로 가르지 않는다(AT-011). 왼쪽 세로 띠의 색(BandGood/Danger/InkMuted)과 앞머리 표시(출동·처리·불가·완료·소멸·방치·놓침)를 함께 쓴다.
 * 중립(InkMuted)은 좋지도 나쁘지도 않은 닫힘(자연 소멸·기회 놓침)에 쓴다.
 * 글꼴은 프로젝트 UI 글꼴 Pretendard 로, 앞머리는 SemiBold, 본문은 Regular.
 */
UCLASS(meta = (DisplayName = "FF Incident Toast"))
class FESTAFIESTA_API UFF_IncidentToast : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxEntries = 3;

	/** Label 은 앞머리의 짧은 표시(출동·처리·불가), bPositive 는 띠와 앞머리 색(성공 BandGood / 실패 Danger)을 고른다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Incident")
	void Show(const FText& Label, const FText& Message, bool bPositive);

	void Show(const FText& Label, const FText& Message, EFF_IncidentToastKind Kind);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	FText GetLabelAt(int32 Index) const;

	/** 가장 최근 줄의 문구. 없으면 빈 문구. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	FText GetMessage() const { return GetMessageAt(0); }

	/** Index 0 이 가장 최근 줄(맨 위)이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	FText GetMessageAt(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	float GetOpacityAt(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	int32 GetEntryCount() const { return Entries.Num(); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	bool IsShowing() const { return !Entries.IsEmpty(); }

	/** 틱 없이 시간을 흘린다. 자동화 테스트와 NativeTick 이 같은 길을 쓴다. */
	void AdvanceTime(float DeltaSeconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float DisplaySeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float FadeSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float TopOffset = 96.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float EntryGap = 6.f;

private:
	struct FEntry
	{
		TObjectPtr<UBorder> Card;
		TObjectPtr<UTextBlock> Label;
		TObjectPtr<UTextBlock> Text;
		float RemainingSeconds = 0.f;
	};

	UBorder* MakeCard(const FText& Label, const FText& Message, EFF_IncidentToastKind Kind, FEntry& OutEntry);
	FSlateFontInfo MakeFont(const TCHAR* Typeface, int32 Size) const;

	void BuildTree();
	void RemoveEntryAt(int32 Index);
	void ApplyEntryOpacity(const FEntry& Entry) const;
	void ApplyVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> List;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UiFont;

	/** 위젯 소유권은 List 가 가진다. 여기는 순서와 남은 시간만 든다. 0 이 가장 최근 줄이다. */
	TArray<FEntry> Entries;
};
