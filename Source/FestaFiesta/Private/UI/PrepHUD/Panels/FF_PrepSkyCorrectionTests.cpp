// 홍보 확인창의 실제 Configure 결과와 세 패널 닫기 아이콘의 중앙 정렬을 검사한다.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/Frontend/FF_ConfirmDialog.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"

namespace FFPrepSkyCorrectionTest
{
	constexpr float Tolerance = .5f;

	bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
	{
		return Inner.Left >= Outer.Left - Tolerance && Inner.Right <= Outer.Right + Tolerance &&
			Inner.Top >= Outer.Top - Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
	}

	void CheckText(FAutomationTestBase& Test, UUserWidget& Owner, const TCHAR* Name,
		const FSlateRect& Container, const FString& Context)
	{
		UTextBlock* Text = Cast<UTextBlock>(Owner.GetWidgetFromName(Name));
		const FString Label = Context + TEXT(" / ") + Name;
		if (!Test.TestNotNull(Label, Text)) { return; }
		const FSlateRect Rect = Text->GetCachedGeometry().GetLayoutBoundingRect();
		const FVector2D Desired = Text->GetDesiredSize();
		Test.TestFalse(Label + TEXT(" 빈 문구가 아니다"), Text->GetText().IsEmpty());
		Test.TestTrue(Label + TEXT(" 영역 안에 표시된다"), Contains(Container, Rect));
		Test.TestTrue(Label + TEXT(" 글자 높이가 잘리지 않는다"), Rect.GetSize().Y + Tolerance >= Desired.Y);
		Test.TestTrue(Label + TEXT(" 글자 폭이 잘리지 않는다"), Rect.GetSize().X + Tolerance >= Desired.X);
	}

	bool SaveTarget(FAutomationTestBase& Test, UTextureRenderTarget2D& Target,
		FIntPoint Size, const FString& Name)
	{
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!Test.TestTrue(Name + TEXT(" 픽셀 읽기"), Target.GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Size.X * Size.Y)) { return false; }
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PrepSkyCorrections/Layout"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, Name + TEXT(".png"));
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
		Test.TestTrue(Name + TEXT(" PNG 저장"), bSaved);
		Test.AddInfo(FString::Printf(TEXT("보정 화면 캡처: %s"), *FPaths::ConvertRelativePathToFull(Path)));
		return bSaved;
	}

	bool CaptureDialog(FAutomationTestBase& Test, UFF_ConfirmDialog& Dialog,
		const TSharedRef<SWidget>& Surface, FIntPoint Size, float ExpectedWidth, const FString& Variant)
	{
		const FString Context = FString::Printf(TEXT("Confirm_%s_%dx%d"), *Variant, Size.X, Size.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(
			FWidgetRenderer::CreateTargetFor(FVector2D(Size.X, Size.Y), TF_Bilinear, true));
		if (!Test.TestNotNull(Context + TEXT(" 렌더 타깃"), Target.Get())) { return false; }
		for (int32 Frame = 0; Frame < 5; ++Frame)
		{
			Dialog.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Surface, FVector2D(Size.X, Size.Y), 1.f / 60.f, false);
		}
		FlushRenderingCommands();
		UWidget* Border = Dialog.GetWidgetFromName(TEXT("Border_Dialog"));
		auto* Cancel = Cast<UFF_CommonButtonBase>(Dialog.GetWidgetFromName(TEXT("Button_Cancel")));
		auto* Confirm = Cast<UFF_CommonButtonBase>(Dialog.GetWidgetFromName(TEXT("Button_Confirm")));
		auto* Rows = Cast<UVerticalBox>(Dialog.GetWidgetFromName(TEXT("VerticalBox_Rows")));
		if (!Test.TestNotNull(Context + TEXT(" 창 배경"), Border) || !Test.TestNotNull(Context + TEXT(" 취소 버튼"), Cancel) ||
			!Test.TestNotNull(Context + TEXT(" 구매 버튼"), Confirm) || !Test.TestNotNull(Context + TEXT(" 수치 표"), Rows)) { return false; }
		const FSlateRect ViewRect(0.f, 0.f, static_cast<float>(Size.X), static_cast<float>(Size.Y));
		const FSlateRect Rect = Border->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Context + TEXT(" 창 전체가 화면 안에 들어온다"), Contains(ViewRect, Rect));
		Test.TestTrue(Context + TEXT(" 화면 중앙에 놓인다"),
			FMath::IsNearlyEqual((Rect.Left + Rect.Right) * .5f, Size.X * .5f, Tolerance) &&
			FMath::IsNearlyEqual((Rect.Top + Rect.Bottom) * .5f, Size.Y * .5f, Tolerance));
		Test.TestTrue(Context + TEXT(" Designer에서 지정한 폭을 사용한다"), FMath::IsNearlyEqual(Rect.GetSize().X, ExpectedWidth, Tolerance));
		Test.TestTrue(Context + TEXT(" 내용에 필요한 양의 높이를 확보한다"), Rect.GetSize().Y > 200.f);
		CheckText(Test, Dialog, TEXT("Text_Title"), Rect, Context);
		CheckText(Test, Dialog, TEXT("Text_Body"), Rect, Context);
		Test.TestEqual(Context + TEXT(" 실제 Configure가 두 수치 행을 생성한다"), Rows->GetChildrenCount(), 2);
		const TCHAR* ExpectedLabels[] = { TEXT("홍보비"), TEXT("구매 후 잔액") };
		const TCHAR* ExpectedValues[] = { TEXT("2,600 Gold"), TEXT("7,400 Gold") };
		for (int32 Index = 0; Index < Rows->GetChildrenCount(); ++Index)
		{
			auto* Row = Cast<UUserWidget>(Rows->GetChildAt(Index));
			if (!Test.TestNotNull(Context + TEXT(" 실제 수치 위젯"), Row)) { return false; }
			const FSlateRect RowRect = Row->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 수치 행이 창 안에 들어온다"), Contains(Rect, RowRect));
			Test.TestNull(Context + TEXT(" 보조 문구 placeholder를 만들지 않는다"), Row->GetWidgetFromName(TEXT("Text_Caption")));
			for (const TCHAR* Name : { TEXT("Text_Label"), TEXT("Text_Value") }) { CheckText(Test, *Row, Name, RowRect, Context); }
			if (Index < UE_ARRAY_COUNT(ExpectedLabels))
			{
				if (auto* Label = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Label"))))
				{ Test.TestEqual(Context + TEXT(" 행 라벨"), Label->GetText().ToString(), FString(ExpectedLabels[Index])); }
				if (auto* Value = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Text_Value"))))
				{ Test.TestEqual(Context + TEXT(" 행 수치"), Value->GetText().ToString(), FString(ExpectedValues[Index])); }
			}
		}
		for (UFF_CommonButtonBase* Button : { Cancel, Confirm })
		{
			const FSlateRect ButtonRect = Button->GetCachedGeometry().GetLayoutBoundingRect();
			Test.TestTrue(Context + TEXT(" 버튼이 창 안에 들어온다"), Contains(Rect, ButtonRect));
			Test.TestNull(Context + TEXT(" 비어 있는 아이콘 네모가 없다"), Button->GetWidgetFromName(TEXT("Image_Icon")));
			Test.TestNull(Context + TEXT(" 비어 있는 Chevron 네모가 없다"), Button->GetWidgetFromName(TEXT("Image_Chevron")));
			CheckText(Test, *Button, TEXT("Text_Label"), ButtonRect, Context);
		}
		Test.TestEqual(Context + TEXT(" 기본 포커스는 구매가 아니라 취소다"), Dialog.GetDesiredFocusTarget(), static_cast<UWidget*>(Cancel));
		return SaveTarget(Test, *Target, Size, Context);
	}

	bool CaptureClose(FAutomationTestBase& Test, UUserWidget& Panel, const FString& Name)
	{
		auto* Close = Cast<UButton>(Panel.GetWidgetFromName(TEXT("Button_Close")));
		auto* Icon = Cast<UImage>(Panel.GetWidgetFromName(TEXT("Image_Close")));
		if (!Test.TestNotNull(Name + TEXT(" 닫기 버튼"), Close) || !Test.TestNotNull(Name + TEXT(" 중앙 X 이미지"), Icon)) { return false; }
		Test.TestNull(Name + TEXT(" 글꼴 baseline에 영향을 받는 예전 닫기 글자가 없다"), Panel.GetWidgetFromName(TEXT("Text_Close")));
		const TSharedRef<SOverlay> Surface = SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Close->TakeWidget()];
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(36, 36), TF_Bilinear, true));
		if (!Test.TestNotNull(Name + TEXT(" 닫기 렌더 타깃"), Target.Get())) { return false; }
		FAssetCompilingManager::Get().FinishAllCompilation();
		for (int32 Frame = 0; Frame < 4; ++Frame) { Renderer.DrawWidget(Target.Get(), Surface, FVector2D(36, 36), 1.f / 60.f, false); }
		FlushRenderingCommands();
		const FSlateRect ButtonRect = Close->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect IconRect = Icon->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(Name + TEXT(" 36×36 버튼 크기"), FMath::IsNearlyEqual(ButtonRect.GetSize().X, 36.f, Tolerance) &&
			FMath::IsNearlyEqual(ButtonRect.GetSize().Y, 36.f, Tolerance));
		Test.TestTrue(Name + TEXT(" 18×18 아이콘 크기"), FMath::IsNearlyEqual(IconRect.GetSize().X, 18.f, Tolerance) &&
			FMath::IsNearlyEqual(IconRect.GetSize().Y, 18.f, Tolerance));
		Test.TestTrue(Name + TEXT(" X가 가로·세로 중앙에 놓인다"),
			FMath::IsNearlyEqual(IconRect.Left + IconRect.Right, ButtonRect.Left + ButtonRect.Right, Tolerance) &&
			FMath::IsNearlyEqual(IconRect.Top + IconRect.Bottom, ButtonRect.Top + ButtonRect.Bottom, Tolerance));
		Test.TestTrue(Name + TEXT(" X가 버튼 밖으로 나가지 않는다"), Contains(ButtonRect, IconRect));
		return SaveTarget(Test, *Target, FIntPoint(36, 36), TEXT("Close_") + Name);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepSkyCorrectionRenderedLayout,
	"FestaFiesta.UI.PrepSkyCorrections.RenderedLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)

bool FFF_PrepSkyCorrectionRenderedLayout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFPrepSkyCorrectionTest;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("검사용 에디터 월드"), World) ||
		!TestNull(TEXT("경제 명령을 실행하지 않는 GI 없는 표시 검사다"), World->GetGameInstance())) { return false; }
	struct FClassState { UWidgetBlueprintGeneratedClass* Class; bool bInitialize; bool bDirty; };
	TArray<FClassState> States;
	ON_SCOPE_EXIT
	{
		for (const FClassState& State : States)
		{
			TestEqual(TEXT("표시 검사는 원본 패키지를 수정하지 않는다"), State.Class->GetOutermost()->IsDirty(), State.bDirty);
			State.Class->bCanCallInitializedWithoutPlayerContext = State.bInitialize;
			State.Class->GetOutermost()->SetDirtyFlag(State.bDirty);
		}
	};
	auto LoadWidget = [&](const FString& Package) -> UWidgetBlueprintGeneratedClass*
	{
		const FString Path = Package + TEXT(".") + FPaths::GetCleanFilename(Package) + TEXT("_C");
		auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UUserWidget>(nullptr, *Path));
		if (!TestNotNull(Package, Class)) { return nullptr; }
		States.Add({Class, Class->bCanCallInitializedWithoutPlayerContext != 0, Class->GetOutermost()->IsDirty()});
		Class->bCanCallInitializedWithoutPlayerContext = true;
		return Class;
	};
	const FString PrepRoot = TEXT("/Game/FestaFiesta/UI/PrepHUD/");
	auto* DialogClass = LoadWidget(PrepRoot + TEXT("WBP_FF_PrepConfirmSky"));
	if (!DialogClass || !LoadWidget(PrepRoot + TEXT("WBP_FF_PrepConfirmSkyRow")) ||
		!LoadWidget(PrepRoot + TEXT("WBP_FF_PrepConfirmSkyCancel")) ||
		!LoadWidget(PrepRoot + TEXT("WBP_FF_PrepConfirmSkyAccept"))) { return false; }
	TStrongObjectPtr<UFF_ConfirmDialog> Dialog(CreateWidget<UFF_ConfirmDialog>(World, DialogClass));
	if (!TestNotNull(TEXT("실제 확인창 인스턴스"), Dialog.Get())) { return false; }
	// CommonUI 스택과 경제 시스템 없이 실제 Configure 및 초기화 바인딩을 검사한다.
	const TSharedRef<SOverlay> Surface = SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[Dialog->TakeWidget()];
	TArray<FFF_ConfirmDialogRow> Rows;
	Rows.Add({ FText::FromString(TEXT("홍보비")), FText::FromString(TEXT("2,600 Gold")) });
	Rows.Add({ FText::FromString(TEXT("구매 후 잔액")), FText::FromString(TEXT("7,400 Gold")) });
	const FText Title = GetDefault<UFF_PrepPanelPromotion>()->GetPurchaseConfirmationTitle(EFF_PromotionChoice::Influencer);
	Dialog->Configure(Title,
		FText::FromString(TEXT("홍보는 축제당 한 번뿐이고 되돌릴 수 없습니다. 3일 전체에 적용됩니다.\n홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다.")),
		Rows, FText::FromString(TEXT("구매하기")));
	if (auto* TitleText = Cast<UTextBlock>(Dialog->GetWidgetFromName(TEXT("Text_Title"))))
	{
		TestEqual(TEXT("Configure가 실제 등급명을 제목에 채운다"), TitleText->GetText().ToString(), Title.ToString());
		TestTrue(TEXT("구매 등급명이 보인다"), TitleText->GetText().ToString().Contains(TEXT("인플루언서")));
		TestFalse(TEXT("제목에 포맷 치환자가 남지 않는다"), TitleText->GetText().ToString().Contains(TEXT("{0}")));
	}
	FAssetCompilingManager::Get().FinishAllCompilation();
	FlushRenderingCommands();
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		if (!CaptureDialog(*this, *Dialog, Surface, Size, 640.f, TEXT("Saved"))) { return false; }
	}
	auto* DialogSize = Cast<USizeBox>(Dialog->GetWidgetFromName(TEXT("SizeBox_Dialog")));
	if (!TestNotNull(TEXT("편집 가능한 확인창 크기"), DialogSize)) { return false; }
	DialogSize->SetWidthOverride(520.f);
	if (!CaptureDialog(*this, *Dialog, Surface, FIntPoint(1280, 720), 520.f, TEXT("Narrow"))) { return false; }
	for (const TCHAR* Name : { TEXT("Program"), TEXT("Staff"), TEXT("Promotion") })
	{
		auto* PanelClass = LoadWidget(PrepRoot + TEXT("Panels/WBP_FF_PrepPanel") + Name);
		if (!PanelClass) { return false; }
		TStrongObjectPtr<UUserWidget> Panel(CreateWidget<UUserWidget>(World, PanelClass));
		if (!TestNotNull(FString(Name) + TEXT(" 패널 인스턴스"), Panel.Get()) || !CaptureClose(*this, *Panel, Name)) { return false; }
	}
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
