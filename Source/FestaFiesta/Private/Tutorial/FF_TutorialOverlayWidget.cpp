// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialOverlayWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Brushes/SlateColorBrush.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Widget.h"
#include "Tutorial/FF_TutorialSubsystem.h"

namespace
{
	// 어두운 사각형 한 장을 그린다.
	//
	// 단색 상자로 칠한다. 예전에는 굵은 선 한 줄(선 굵기 = 높이)로 칠했는데, 카테고리 안내 때
	// 덮개에 검은 줄이 심하게 생긴다는 피드백이 있었다(2026-10-08). 선은 해상도마다 굵기 · 끝 처리가
	// 달라 칸 경계가 겹치거나 비어 줄이 될 수 있다. 상자는 주어진 칸을 정확히 채우고 픽셀에 맞춰진다.
	// 브러시 애셋 없이 쓰려고 흰 단색 브러시를 코드에 두고 색으로 물들인다.
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

		static const FSlateColorBrush WhiteBrush(FLinearColor::White);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
			&WhiteBrush,
			ESlateDrawEffect::None,
			Color);
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
	bBlockingViewOnlyPanel = false;
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFF_TutorialOverlayWidget::SetBlockingViewOnlyPanel(const bool bBlock)
{
	if (bBlock == bBlockingViewOnlyPanel)
	{
		return;
	}
	bBlockingViewOnlyPanel = bBlock;

	// 막을 때만 Visible 로 올린다. 화면 전체를 덮는 위젯이라 이 동안은 아래 UI 가 눌리지 않는다.
	SetVisibility(bBlock ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
}

FReply UFF_TutorialOverlayWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bBlockingViewOnlyPanel)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// 어디를 눌렀든 패널만 닫는다. 이 클릭은 아래로 보내지 않는다 — 대사를 넘기거나
	// 배치 중인 시설을 놓는 데 쓰이면 플레이어가 예상하지 못한 일이 생긴다.
	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		Tutorial->DismissViewOnlyPrepPanel();
	}
	SetBlockingViewOnlyPanel(false);
	return FReply::Handled();
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

bool UFF_TutorialOverlayWidget::ResolveWorldCircle(FVector2D& OutCenter, float& OutRadius) const
{
	if (CurrentAnchor.IsNone())
	{
		return false;
	}

	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	if (!Tutorial)
	{
		return false;
	}

	FVector WorldLocation = FVector::ZeroVector;
	float WorldRadius = 0.f;
	if (!Tutorial->FindAnchorWorldPoint(CurrentAnchor, WorldLocation, WorldRadius))
	{
		return false;
	}

	APlayerController* Controller = GetOwningPlayer();
	if (!Controller)
	{
		return false;
	}

	FVector2D ScreenCenter = FVector2D::ZeroVector;
	if (!UGameplayStatics::ProjectWorldToScreen(Controller, WorldLocation, ScreenCenter))
	{
		// 화면 밖이거나 카메라 뒤다. 탑뷰라 드물지만 가장자리에서 나올 수 있다.
		return false;
	}

	// 반경도 화면 크기로 바꾼다. 줌에 따라 원이 같이 커지고 작아져야 한다.
	//
	// 옆으로 한 칸 떨어진 지점을 같이 투영해서 그 거리를 쓴다.
	// 카메라 방향을 계산하는 것보다 단순하고, 탑뷰에서는 어느 방향이든 비슷하다.
	FVector2D ScreenEdge = FVector2D::ZeroVector;
	const FVector EdgeWorld = WorldLocation + FVector(WorldRadius, 0.f, 0.f);

	if (UGameplayStatics::ProjectWorldToScreen(Controller, EdgeWorld, ScreenEdge))
	{
		OutRadius = static_cast<float>(FVector2D::Distance(ScreenCenter, ScreenEdge));
	}
	else
	{
		// 가장자리를 못 구했으면 눈에 보일 만한 값으로 둔다.
		OutRadius = 60.f;
	}

	// 투영은 픽셀 좌표를 주는데 위젯은 DPI 가 적용된 좌표를 쓴다.
	// 이걸 안 나누면 해상도가 다를 때 원이 엉뚱한 데 그려진다.
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (Scale > KINDA_SMALL_NUMBER)
	{
		ScreenCenter /= Scale;
		OutRadius /= Scale;
	}

	OutCenter = ScreenCenter;

	// 너무 작으면 안 보이고 너무 크면 화면을 덮는다.
	OutRadius = FMath::Clamp(OutRadius, 30.f, 600.f);
	return true;
}

void UFF_TutorialOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	// 보기 전용 패널이 열려 있으면 클릭을 받는다. NativeOnMouseButtonDown 참고.
	{
		const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
		SetBlockingViewOnlyPanel(Tutorial && Tutorial->IsDialogueActive() && Tutorial->IsPrepPanelViewOnly());
	}

	PulseTime += DeltaTime;

	// 매 프레임 다시 구한다. 스크롤이나 창 크기 변경으로 목표가 움직일 수 있다.
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	const bool bFound = ResolveHighlightRect(TopLeft, Size);

	// UI 에서 못 찾았을 때만 세계를 본다.
	//
	// 둘 다 그리면 화면이 어수선하고, 이름이 겹쳤다는 것도 눈치채기 어렵다.
	FVector2D WorldCenter = FVector2D::ZeroVector;
	float WorldRadius = 0.f;
	const bool bFoundCircle = !bFound && ResolveWorldCircle(WorldCenter, WorldRadius);

	const bool bCircleChanged =
		bFoundCircle != bHasCachedCircle
		|| !WorldCenter.Equals(CachedWorldCenter, 0.5f)
		|| !FMath::IsNearlyEqual(WorldRadius, CachedWorldRadius, 0.5f);

	bHasCachedCircle = bFoundCircle;
	CachedWorldCenter = WorldCenter;
	CachedWorldRadius = WorldRadius;

	if (bCircleChanged)
	{
		BP_OnHighlightCircleChanged(WorldCenter, WorldRadius, bFoundCircle);
	}

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

	// 세계 지점을 가리키는 중이다. 화면 위가 아니라 저기를 보라는 뜻이므로
	// 사각 테두리가 아니라 원을 그린다. 모양이 달라야 무엇을 가리키는지 안다.
	if (bHasCachedCircle)
	{
		if (bMaskScreenForWorldTarget)
		{
			DrawMaskRect(OutDrawElements, BaseLayer, AllottedGeometry,
				FVector2f::ZeroVector, FVector2f(ScreenSize.X, ScreenSize.Y), MaskColor);
		}

		float CircleAlpha = OutlineColor.A;
		if (PulsePeriodSeconds > 0.f)
		{
			const float Phase = FMath::Fmod(PulseTime, PulsePeriodSeconds) / PulsePeriodSeconds;
			const float Wave = 0.5f + 0.5f * FMath::Cos(Phase * 2.f * PI);
			CircleAlpha *= FMath::Lerp(PulseMinAlpha, 1.f, Wave);
		}

		FLinearColor PulsedCircle = OutlineColor;
		PulsedCircle.A = CircleAlpha;

		// 선분으로 원을 만든다. 32 등분이면 가장자리가 각져 보이지 않는다.
		constexpr int32 CircleSegments = 32;

		TArray<FVector2f> CirclePoints;
		CirclePoints.Reserve(CircleSegments + 1);

		for (int32 Index = 0; Index <= CircleSegments; ++Index)
		{
			const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(CircleSegments);
			CirclePoints.Add(FVector2f(
				static_cast<float>(CachedWorldCenter.X) + FMath::Cos(Angle) * CachedWorldRadius,
				static_cast<float>(CachedWorldCenter.Y) + FMath::Sin(Angle) * CachedWorldRadius));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			BaseLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(CirclePoints),
			ESlateDrawEffect::None,
			PulsedCircle,
			true,
			OutlineThickness);

		return BaseLayer + 2;
	}

	// 가리킬 것이 없으면 아무것도 그리지 않는다.
	//
	// 덮개는 강조할 대상을 돋보이게 하려는 것이다. 대상이 없는데 화면만 어두우면
	// 대사를 읽는 내내 시야만 가린다. 대사 86줄 중 강조가 붙은 줄은 셋뿐이다.
	if (!bHasCachedRect)
	{
		return BaseLayer;
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

	// 읽고 넘기는 줄의 강조면 덮개 없이 테두리만 그린다. FFF_DialogueLine::bHighlightOutlineOnly 주석 참고.
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const bool bOutlineOnly = Tutorial && Tutorial->IsHighlightOutlineOnly();

	if (!bOutlineOnly)
	{
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
	}

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
