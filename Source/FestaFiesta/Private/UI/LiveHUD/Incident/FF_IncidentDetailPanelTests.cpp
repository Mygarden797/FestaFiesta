#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Incident/FF_IncidentDetailPanel.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Staff/FF_StaffDispatchSubsystem.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentDetailPanelStatusTest,
	"FestaFiesta.UI.Incident.DetailPanelResponseStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentDetailPanelStatusTest::RunTest(const FString& Parameters)
{
	using EStage = EFF_IncidentResponseStage;

	// 패널은 틱에서 다시 읽지 않는다. 남은 시간 표시가 바뀌는 순간만 출동 담당이 알린다.
	TestTrue(TEXT("Crossing a displayed minute is announced"), UFF_StaffDispatchSubsystem::ShouldAnnounceWorkMinute(2.1f, 1.9f));
	TestFalse(TEXT("Time passing inside the same displayed minute is not announced"),
		UFF_StaffDispatchSubsystem::ShouldAnnounceWorkMinute(2.9f, 2.5f));
	TestFalse(TEXT("Finishing is not announced; the incident closes instead"),
		UFF_StaffDispatchSubsystem::ShouldAnnounceWorkMinute(0.5f, -0.1f));
	TestFalse(TEXT("A paused clock announces nothing"), UFF_StaffDispatchSubsystem::ShouldAnnounceWorkMinute(1.5f, 1.5f));

	TestTrue(TEXT("No dispatch means no status line"),
		UFF_IncidentDetailPanel::BuildResponseStatusText(EStage::None, 0.f).IsEmpty());
	TestEqual(TEXT("Travelling staff are described in words"),
		UFF_IncidentDetailPanel::BuildResponseStatusText(EStage::Travelling, 0.f).ToString(),
		FString(TEXT("스태프 출동 중 · 현장으로 가는 중")));
	TestEqual(TEXT("Working staff show the remaining time rounded up"),
		UFF_IncidentDetailPanel::BuildResponseStatusText(EStage::Working, 2.2f).ToString(),
		FString(TEXT("스태프 출동 중 · 현장 작업 중 · 남은 시간 약 3분")));
	TestEqual(TEXT("Almost-finished work still shows at least one minute"),
		UFF_IncidentDetailPanel::BuildResponseStatusText(EStage::Working, 0.1f).ToString(),
		FString(TEXT("스태프 출동 중 · 현장 작업 중 · 남은 시간 약 1분")));

	TestEqual(TEXT("A disabled send button explains a staff shortage"),
		UFF_IncidentDetailPanel::BuildSendBlockedReason(EStage::None, true, 0, 1).ToString(),
		UFF_UIDisplayTextLibrary::GetIncidentResolveResultText(EFF_IncidentResolveResult::NotEnoughStaff).ToString());
	TestTrue(TEXT("While staff are out the status line already explains the disabled button"),
		UFF_IncidentDetailPanel::BuildSendBlockedReason(EStage::Travelling, true, 0, 1).IsEmpty());
	TestTrue(TEXT("Enough staff means no reason line"),
		UFF_IncidentDetailPanel::BuildSendBlockedReason(EStage::None, true, 2, 1).IsEmpty());
	TestTrue(TEXT("An acknowledge-only incident has no staff reason"),
		UFF_IncidentDetailPanel::BuildSendBlockedReason(EStage::None, false, 0, 1).IsEmpty());

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor fixture world"), World))
	{
		return false;
	}

	TStrongObjectPtr<UFF_IncidentDetailPanel> Panel(CreateWidget<UFF_IncidentDetailPanel>(World, UFF_IncidentDetailPanel::StaticClass()));
	if (!TestNotNull(TEXT("Detail panel instance"), Panel.Get()))
	{
		return false;
	}
	Panel->TakeWidget();
	const UBorder* Card = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Border_Card")));
	const UTextBlock* Title = Cast<UTextBlock>(Panel->GetWidgetFromName(TEXT("Text_Title")));
	const UButton* Send = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Send")));
	if (!TestNotNull(TEXT("Card"), Card) || !TestNotNull(TEXT("Title"), Title) || !TestNotNull(TEXT("Send button"), Send))
	{
		return false;
	}
	TestEqual(TEXT("The card is a rounded LightTeal box"), Card->Background.DrawAs, ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("Panels use the 8px panel radius"),
		static_cast<float>(Card->Background.OutlineSettings.CornerRadii.X), FFIncidentUiStyle::PanelRadius);
	TestEqual(TEXT("The send button uses the rounded primary style"),
		Send->GetStyle().Normal.DrawAs, ESlateBrushDrawType::RoundedBox);
	TestEqual(TEXT("Buttons use the 6px card/button radius"),
		static_cast<float>(Send->GetStyle().Normal.OutlineSettings.CornerRadii.X), FFIncidentUiStyle::CardRadius);
	const USizeBox* SendSize = Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Button_Send_Size")));
	if (TestNotNull(TEXT("The send button sits in a hit-size box"), SendSize))
	{
		TestTrue(TEXT("Buttons are at least 44px tall"), SendSize->GetMinDesiredHeight() >= FFIncidentUiStyle::MinInteractiveSize);
	}
	const UObject* TitleFont = Title->GetFont().FontObject;
	TestTrue(TEXT("Titles use the project Pretendard font"),
		TitleFont && TitleFont->GetPathName() == FString(FFIncidentUiStyle::UiFontPath));
	TestEqual(TEXT("Panel titles are 20px"), static_cast<int32>(Title->GetFont().Size), FFIncidentUiStyle::PanelTitleFontSize);
	const UBorder* Status = Cast<UBorder>(Panel->GetWidgetFromName(TEXT("Border_Status")));
	if (TestNotNull(TEXT("The panel has a status line"), Status))
	{
		TestEqual(TEXT("The status line is a 4px chip"),
			static_cast<float>(Status->Background.OutlineSettings.CornerRadii.X), FFIncidentUiStyle::ChipRadius);
	}
	TestNotNull(TEXT("The panel has a reason line"), Panel->GetWidgetFromName(TEXT("Text_Reason")));

	TStrongObjectPtr<UFF_IncidentMarkerButton> Marker(CreateWidget<UFF_IncidentMarkerButton>(World, UFF_IncidentMarkerButton::StaticClass()));
	if (!TestNotNull(TEXT("Marker instance"), Marker.Get()))
	{
		return false;
	}
	Marker->TakeWidget();
	TestFalse(TEXT("An idle marker has no ring"), Marker->IsResponseRingVisible());
	Marker->SetResponseStage(EStage::Working);
	TestTrue(TEXT("A marker with staff out shows a ring"), Marker->IsResponseRingVisible());
	TestEqual(TEXT("The ring comes with words in the tooltip"),
		Marker->GetToolTipText().ToString(), FString(TEXT("스태프 출동 중 · 현장 작업 중")));
	Marker->SetResponseStage(EStage::None);
	TestFalse(TEXT("The ring leaves when the response ends"), Marker->IsResponseRingVisible());
	return true;
}

#endif
