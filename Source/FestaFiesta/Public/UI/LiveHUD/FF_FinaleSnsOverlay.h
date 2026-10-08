// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "FF_FinaleSnsOverlay.generated.h"

class UBorder;
class UCanvasPanel;
class UFont;
class UVerticalBox;
class UFF_FestivalViewModelSubsystem;

/**
 * 피날레 연출 중 SNS 후기를 화면 왼쪽 아래에 라이브 채팅처럼 흘려보내는 오버레이. WBP 없이 C++ 로 트리를 만든다.
 *
 * 불꽃을 가리지 않는 것이 우선이다. 카드 대신 반투명 어두운 띠 한 줄에 "작성자 본문"을 작은 흰 글씨로 쓰고,
 * 왼쪽 끝 가는 색 띠로 반응(긍정·부정)을 보인다. 새 줄은 맨 아래에 붙고 이전 줄은 위로 밀리며,
 * MaxEntries 를 넘으면 가장 위(오래된) 줄부터 지운다. 오래된 줄일수록 흐리다.
 * 피날레 단계를 벗어나면 비우고 숨긴다. 전체가 HitTestInvisible 이라 월드 클릭을 막지 않는다.
 */
UCLASS(meta = (DisplayName = "FF Finale SNS Overlay"))
class FESTAFIESTA_API UFF_FinaleSnsOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Finale")
	void AddPost(const FFF_LiveSocialPost& Post);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Finale")
	void ClearPosts();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Finale")
	int32 GetEntryCount() const { return Entries.Num(); }

	/** 틱 없이 시간을 흘린다. 자동화 테스트와 NativeTick 이 같은 길을 쓴다. */
	void AdvanceTime(float DeltaSeconds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "1"))
	int32 MaxEntries = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FVector2D ScreenMargin = FVector2D(24.f, 120.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "120.0"))
	float ListWidth = 340.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	float EntryGap = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "8"))
	int32 BodyFontSize = 13;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "8"))
	int32 AuthorFontSize = 11;

	/** 줄 뒤 띠의 색. 불꽃 위에서도 글씨가 읽힐 만큼만 어둡게 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FLinearColor BackingColor = FLinearColor(0.f, 0.f, 0.f, 0.38f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FLinearColor PositiveColor = FLinearColor::FromSRGBColor(FColor(110, 231, 190));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FLinearColor NegativeColor = FLinearColor::FromSRGBColor(FColor(255, 166, 107));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 0.2f;

	/** 가장 오래된 줄의 불투명도. 새 줄은 1 이고 위로 갈수록 이 값까지 내려간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OldestOpacity = 0.25f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FEntry
	{
		TObjectPtr<UBorder> Row;
		float AgeSeconds = 0.f;
	};

	void BuildTree();
	UBorder* MakeRow(const FFF_LiveSocialPost& Post);
	void ApplyOpacities() const;
	void ApplyVisibility();
	void BindViewModel();

	UFUNCTION()
	void HandleFinalePostAdded(FFF_LiveSocialPost NewPost);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> List;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UiFont;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> BoundViewModel;

	bool bWasFinaleStage = false;

	/** 위젯 소유권은 List 가 가진다. 0 이 가장 오래된 줄(맨 위)이다. */
	TArray<FEntry> Entries;
};
