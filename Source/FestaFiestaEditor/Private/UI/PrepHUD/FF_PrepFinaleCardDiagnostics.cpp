// 읽기 전용 진단: 피날레 카드를 실제로 만들어 등급 그림이 왜 안 보이는지 위젯 상태를 로그로 남긴다. 저장하지 않는다.
#include "UI/Core/FF_UIWbpBuilderCommon.h"

#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "UI/PrepHUD/Panels/FF_PrepFinaleCard.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelFinale.h"

namespace FFPrepFinaleCardDiagnostics
{
	constexpr const TCHAR* LogPrefix = TEXT("FF_FINALE_CARD_DIAG");

	FString Vis(const UWidget* Widget)
	{
		return Widget ? StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())) : TEXT("null");
	}

	void LogImage(const TCHAR* Stage, UFF_PrepFinaleCard& Card)
	{
		const UImage* Image = Cast<UImage>(Card.GetWidgetFromName(TEXT("Image_Preview")));
		if (!Image) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s %s: Image_Preview 없음"), LogPrefix, Stage); return; }
		const FSlateBrush& Brush = Image->GetBrush();
		const TSharedPtr<SWidget> Slate = Image->GetCachedWidget();
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s %s: res=%s size=%s draw=%d vis=%s opacity=%.2f tint=%s desired=%s"),
			LogPrefix, Stage, Brush.GetResourceObject() ? *Brush.GetResourceObject()->GetPathName() : TEXT("None"),
			*Brush.GetImageSize().ToString(), static_cast<int32>(Brush.DrawAs.GetValue()), *Vis(Image), Image->GetRenderOpacity(),
			*Image->GetColorAndOpacity().ToString(), Slate.IsValid() ? *Slate->GetDesiredSize().ToString() : TEXT("no-slate"));
		for (const UWidget* Parent = Image->GetParent(); Parent; Parent = Parent->GetParent())
		{
			const TSharedPtr<SWidget> ParentSlate = Parent->GetCachedWidget();
			UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s   parent %s (%s) vis=%s opacity=%.2f desired=%s"), LogPrefix,
				*Parent->GetName(), *Parent->GetClass()->GetName(), *Vis(Parent), Parent->GetRenderOpacity(),
				ParentSlate.IsValid() ? *ParentSlate->GetDesiredSize().ToString() : TEXT("no-slate"));
		}
	}

	void Run()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UClass* PanelClass = LoadClass<UFF_PrepPanelFinale>(nullptr, TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelFinale.WBP_FF_PrepPanelFinale_C"));
		if (!World || !PanelClass) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: 월드 또는 패널 클래스 없음"), LogPrefix); return; }

		// 패널 기본값의 카드 클래스와 등급별 그림을 리플렉션으로 읽는다(보호 멤버).
		const UObject* PanelDefaults = PanelClass->GetDefaultObject();
		const FClassProperty* CardProp = FindFProperty<FClassProperty>(PanelClass, TEXT("CardWidgetClass"));
		UClass* CardClass = CardProp ? Cast<UClass>(CardProp->GetPropertyValue_InContainer(PanelDefaults)) : nullptr;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s card class=%s"), LogPrefix, CardClass ? *CardClass->GetPathName() : TEXT("None"));
		const FMapProperty* MapProp = FindFProperty<FMapProperty>(PanelClass, TEXT("TierPresentations"));
		const TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation>* Presentations = MapProp
			? MapProp->ContainerPtrToValuePtr<TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation>>(PanelDefaults) : nullptr;
		const FFF_FinaleTierPresentation* C = Presentations ? Presentations->Find(EFF_FinaleGrade::C) : nullptr;
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s presentation C texture=%s"), LogPrefix,
			C && C->PreviewTexture ? *C->PreviewTexture->GetPathName() : TEXT("None"));
		if (!CardClass || !C) { return; }

		UFF_PrepFinaleCard* Card = CreateWidget<UFF_PrepFinaleCard>(World, CardClass);
		if (!Card) { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s: 카드 생성 실패"), LogPrefix); return; }
		LogImage(TEXT("created"), *Card);
		FFF_FinaleTier Tier; Tier.Grade = EFF_FinaleGrade::C; Tier.CostGold = 2000; Tier.SatisfactionBonus = 5;
		Card->ConfigureTier(Tier, *C);
		LogImage(TEXT("configured"), *Card);
		TSharedRef<SWidget> Root = Card->TakeWidget();
		Root->SlatePrepass(1.f);
		LogImage(TEXT("constructed"), *Card);
		Card->ApplyFinaleState(EFF_PrepCardState::Selectable, FText::GetEmpty());
		Root->SlatePrepass(1.f);
		LogImage(TEXT("state-applied"), *Card);
		Card->RemoveFromParent();
	}

	FAutoConsoleCommand Command(TEXT("FF.DiagnoseFinaleCardImage"),
		TEXT("피날레 카드를 임시로 만들어 등급 그림 위젯 상태를 로그로 남긴다(저장 안 함)."),
		FConsoleCommandDelegate::CreateStatic(&Run));
}
