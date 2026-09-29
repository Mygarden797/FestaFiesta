// 사용자 편집한 패널 트리는 그대로 두고 닫기 버튼의 글리프만 교체한다.
#include "UI/PrepHUD/FF_PrepPanelSkyBuilder.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "HAL/IConsoleManager.h"
#include "WidgetBlueprintEditorUtils.h"

namespace FFPrepCloseIconFix
{
	void Apply()
	{
		using namespace FFUIWbpBuilder;
		for (const TCHAR* Name : { TEXT("Program"), TEXT("Staff"), TEXT("Promotion") })
		{
			const FString Path = FString(TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanel")) + Name;
			UWidgetBlueprint* BP = LoadExistingBlueprint(Path);
			if (!BP || !BP->WidgetTree) { continue; }
			UWidgetTree& Tree = *BP->WidgetTree;
			if (Tree.FindWidget(TEXT("Image_Close"))) { continue; }
			UButton* Close = Cast<UButton>(Tree.FindWidget(TEXT("Button_Close")));
			if (!Close) { continue; }
			if (UWidget* Old = Close->GetContent())
			{
				// 에디터의 삭제 경로로 GUID와 고아 객체까지 함께 정리한다.
				FWidgetBlueprintEditorUtils::DeleteWidgets(BP, TSet<UWidget*>{Old},
					FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
			}
			UButtonSlot* Slot = CastChecked<UButtonSlot>(Close->AddChild(FFPrepPanelSkyBuilder::CloseIcon(*BP, Tree)));
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
			Close->SetToolTipText(NSLOCTEXT("FestaFiestaPrepSky", "Close", "닫기"));
			SaveNewBlueprint(TEXT("FF_PREP_CLOSE_ICON"), *BP);
		}
	}
	FAutoConsoleCommand Command(TEXT("FF.FixPrepSkyCloseIcons"),
		TEXT("프로그램/인력/홍보의 X만 중앙 아이콘으로 변경한다. 패널 크기와 레이아웃은 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&Apply));
}
