// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialOverlayWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Tutorial/FF_TutorialSubsystem.h"

namespace
{
	// 어두운 사각형 한 장을 그린다.
	//
	// 채우기를 MakeBox 대신 굵은 선으로 그린다. 브러시 애셋 없이 되고,
	// 이 프로젝트의 다른 커스텀 그리기도 전부 MakeLines 를 쓰고 있어 방식이 일관된다.
	void DrawMaskRect(
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FGeometry& Geometry,
		const FVector2f& TopLeft,
		const FVector2f& Size,
		const FLinearColor& Color)
	{
		// 크기가 0 이하면 그리지 않는다. 목표가 화면 가장자리에 붙으면 생긴다.
		if (Size.X <= 0.f || Size.Y <= 0.f)
		{
			return;
		}

		// 가로로 긴 선 한 줄로 세로를 다 덮는다. 선 굵기가 곧 높이다.
		const float HalfHeight = Size.Y * 0.5f;

		TArray<FVector2f> Points {
			FVector2f(TopLeft.X, TopLeft.Y + HalfHeight),
			FVector2f(TopLeft.X + Size.X, TopLeft.Y + HalfHeight)
		};

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Color,
			false,
			Size.Y);
	}
}

void UFF_TutorialOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 클릭이 그대로 아래로 지나가게 한다. 이 위젯은 보여주기만 한다.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	if (!Tutorial)
	{
		return;
	}

	Tutorial->OnHighlightAnchorChanged.AddDynamic(
		this, &UFF_TutorialOverlayWidget::HandleHighlightAnchorChanged);
	Tutorial->OnDialogueActiveChanged.AddDynamic(
		this, &UFF_TutorialOverlayWidget::HandleDialogueActiveChanged);
	bBound = true;

	// 위젯이 대사보다 늦게 뜨는 경우가 있다. 지금 상태를 반영한다.
	CurrentAnchor = Tutorial->GetHighlightAnchor();
	HandleDialogueActiveChanged(Tutorial->IsDialogueActive());
}

void UFF_TutorialOverlayWidget::NativeDestruct()
{
	if (bBound)
	{
		if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
		{
			Tutorial->OnHighlightAnchorChanged.RemoveDynamic(
				this, &UFF_TutorialOverlayWidget::HandleHighlightAnchorChanged);
			Tutorial->OnDialogueActiveChanged.RemoveDynamic(
				this, &UFF_TutorialOverlayWidget::HandleDialogueActiveChanged);
		}
		bBound = false;
	}
	Super::NativeDestruct();
}

void UFF_TutorialOverlayWidget::HandleHighlightAnchorChanged(const FName AnchorId)
{
	CurrentAnchor = AnchorId;

	// 대상이 바뀌면 깜빡임을 처음부터. 안 그러면 흐려진 상태로 나타난다.
	PulseTime = 0.f;
}

void UFF_TutorialOverlayWidget::HandleDialogueActiveChanged(const bool bActive)
{
	// 대사가 없으면 덮개도 없앤다. 대사 중이 아닌데 화면이 어두우면 버그로 보인다.
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UFF_TutorialOverlayWidget::ResolveHighlightRect(
	FVector2D& OutTopLeft, FVector2D& OutSize) const
{
	if (CurrentAnchor.IsNone())
	{
		return false;
	}

	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const UWidget* Target = Tutorial ? Tutorial->FindAnchorWidget(CurrentAnchor) : nullptr;
	if (!Target)
	{
		// 아직 등록되지 않았거나 그 화면이 닫혔다. 덮개만 그리고 구멍은 없다.
		return false;
	}

	const FGeometry& TargetGeometry = Target->GetCachedGeometry();

	// 크기가 0 이면 아직 배치되기 전이다. 첫 프레임에 흔히 나온다.
	const FVector2D LocalSize = TargetGeometry.GetLocalSize();
	if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f)
	{
		return false;
	}

	// 화면 좌표로 바꾼다. 목표가 어느 패널 안에 있든 여기서는 화면 기준이어야 한다.
	FVector2D ViewportTopLeft = FVector2D::ZeroVector;
	FVector2D ViewportBottomRight = FVector2D::ZeroVector;
	FVector2D PixelPosition = FVector2D::ZeroVector;

	USlateBlueprintLibrary::LocalToViewport(
		this, TargetGeometry, FVector2D::ZeroVector, PixelPosition, ViewportTopLeft);
	USlateBlueprintLibrary::LocalToViewport(
		this, TargetGeometry, LocalSize, PixelPosition, ViewportBottomRight);

	// UUserWidget 에 이미 Padding 멤버가 있어서 다른 이름을 쓴다.
	const float Margin = FMath::Max(0.f, HolePadding);

	OutTopLeft = ViewportTopLeft - FVector2D(Margin, Margin);
	OutSize = (ViewportBottomRight - ViewportTopLeft) + FVector2D(Margin * 2.f, Margin * 2.f);

	return OutSize.X > 0.f && OutSize.Y > 0.f;
}

void UFF_TutorialOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	PulseTime += DeltaTime;

	// 매 프레임 다시 구한다. 스크롤이나 창 크기 변경으로 목표가 움직일 수 있다.
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	const bool bFound = ResolveHighlightRect(TopLeft, Size);

	// 값이 바뀔 때만 알린다. 매 프레임 알리면 BP 쪽에서 불필요한 갱신이 돈다.
	const bool bChanged =
		bFound != bHasCachedRect
		|| !TopLeft.Equals(CachedTopLeft, 0.5f)
		|| !Size.Equals(CachedSize, 0.5f);

	bHasCachedRect = bFound;
	CachedTopLeft = TopLeft;
	CachedSize = Size;

	if (bChanged)
	{
		BP_OnHighlightRectChanged(TopLeft, Size, bFound);
	}
}

int32 UFF_TutorialOverlayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();

	// 강조할 대상이 없으면 화면 전체를 덮는다.
	if (!bHasCachedRect)
	{
		DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
			FVector2f::ZeroVector, FVector2f(ScreenSize.X, ScreenSize.Y), MaskColor);
		return BaseLayer + 1;
	}

	// 구멍 주변을 네 장으로 감싼다.
	//
	// 구멍 뚫린 텍스처를 만들면 크기가 매번 달라져서 늘어난다.
	// 사각형 네 장이면 어떤 크기에도 늘어남 없이 맞는다.
	const float Left = static_cast<float>(FMath::Clamp(CachedTopLeft.X, 0.0, ScreenSize.X));
	const float Top = static_cast<float>(FMath::Clamp(CachedTopLeft.Y, 0.0, ScreenSize.Y));
	const float Right = static_cast<float>(
		FMath::Clamp(CachedTopLeft.X + CachedSize.X, 0.0, ScreenSize.X));
	const float Bottom = static_cast<float>(
		FMath::Clamp(CachedTopLeft.Y + CachedSize.Y, 0.0, ScreenSize.Y));

	const float ScreenW = static_cast<float>(ScreenSize.X);
	const float ScreenH = static_cast<float>(ScreenSize.Y);

	// 위
	DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
		FVector2f(0.f, 0.f), FVector2f(ScreenW, Top), MaskColor);

	// 아래
	DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
		FVector2f(0.f, Bottom), FVector2f(ScreenW, ScreenH - Bottom), MaskColor);

	// 왼쪽 (구멍 높이만큼만)
	DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
		FVector2f(0.f, Top), FVector2f(Left, Bottom - Top), MaskColor);

	// 오른쪽
	DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
		FVector2f(Right, Top), FVector2f(ScreenW - Right, Bottom - Top), MaskColor);

	// 테두리. 깜빡여서 시선을 끈다.
	float OutlineAlpha = OutlineColor.A;
	if (PulsePeriodSeconds > 0.f)
	{
		const float Phase = FMath::Fmod(PulseTime, PulsePeriodSeconds) / PulsePeriodSeconds;
		const float Wave = 0.5f + 0.5f * FMath::Cos(Phase * 2.f * PI);
		OutlineAlpha *= FMath::Lerp(PulseMinAlpha, 1.f, Wave);
	}

	FLinearColor PulsedOutline = OutlineColor;
	PulsedOutline.A = OutlineAlpha;

	TArray<FVector2f> OutlinePoints {
		FVector2f(Left, Top),
		FVector2f(Right, Top),
		FVector2f(Right, Bottom),
		FVector2f(Left, Bottom),
		FVector2f(Left, Top)
	};

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		MoveTemp(OutlinePoints),
		ESlateDrawEffect::None,
		PulsedOutline,
		true,
		OutlineThickness);

	return BaseLayer + 2;
}
