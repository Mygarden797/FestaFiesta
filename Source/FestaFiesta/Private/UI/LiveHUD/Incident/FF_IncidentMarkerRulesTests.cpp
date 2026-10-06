#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/Incident/FF_IncidentMarkerRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentMarkerRulesGlyphAndColorTest,
	"FestaFiesta.UI.Incident.MarkerRules.GlyphFollowsToneAndRingColorFollowsUrgency",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentMarkerRulesGlyphAndColorTest::RunTest(const FString& Parameters)
{
	const FFF_UIPaletteInGame Colors;
	FFF_IncidentView View;

	View.Tone = EFF_IncidentTone::Problem;
	TestTrue(TEXT("Problem shows !"), FFIncidentMarkerRules::Resolve(View, false, 0.f, Colors).Glyph == EFF_IncidentMarkerGlyph::Quest);
	View.Tone = EFF_IncidentTone::Opportunity;
	TestTrue(TEXT("Opportunity shows !"), FFIncidentMarkerRules::Resolve(View, false, 0.f, Colors).Glyph == EFF_IncidentMarkerGlyph::Quest);
	View.Tone = EFF_IncidentTone::Notice;
	TestTrue(TEXT("Notice shows ?"), FFIncidentMarkerRules::Resolve(View, false, 0.f, Colors).Glyph == EFF_IncidentMarkerGlyph::Info);

	TestEqual(TEXT("Low"), FFIncidentMarkerRules::GetUrgencyColor(EFF_IncidentUrgency::Low, Colors), Colors.IncidentUrgencyLow);
	TestEqual(TEXT("Medium"), FFIncidentMarkerRules::GetUrgencyColor(EFF_IncidentUrgency::Medium, Colors), Colors.IncidentUrgencyMedium);
	TestEqual(TEXT("High reuses Amber"), FFIncidentMarkerRules::GetUrgencyColor(EFF_IncidentUrgency::High, Colors), Colors.Amber);
	TestEqual(TEXT("Immediate reuses Danger"), FFIncidentMarkerRules::GetUrgencyColor(EFF_IncidentUrgency::Immediate, Colors), Colors.Danger);

	TestTrue(TEXT("Low does not move"), FFIncidentMarkerRules::GetUrgencyMotion(EFF_IncidentUrgency::Low) == EFF_IncidentMarkerMotion::None);
	TestTrue(TEXT("Medium does not move"), FFIncidentMarkerRules::GetUrgencyMotion(EFF_IncidentUrgency::Medium) == EFF_IncidentMarkerMotion::None);
	TestTrue(TEXT("High pulses so color is not the only cue"), FFIncidentMarkerRules::GetUrgencyMotion(EFF_IncidentUrgency::High) == EFF_IncidentMarkerMotion::Pulse);
	TestTrue(TEXT("Immediate blinks so color is not the only cue"), FFIncidentMarkerRules::GetUrgencyMotion(EFF_IncidentUrgency::Immediate) == EFF_IncidentMarkerMotion::Blink);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentMarkerRulesLapseRingTest,
	"FestaFiesta.UI.Incident.MarkerRules.RingShrinksWithRemainingTimeBeforeDispatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentMarkerRulesLapseRingTest::RunTest(const FString& Parameters)
{
	const FFF_UIPaletteInGame Colors;
	FFF_IncidentView View;
	View.OccurredAtMinutes = 10.f;
	View.MinuteUntileLapse = 60.f;

	TestEqual(TEXT("Just raised is full"), FFIncidentMarkerRules::Resolve(View, true, 10.f, Colors).Progress, 1.f);
	TestEqual(TEXT("Half the time gone is half"), FFIncidentMarkerRules::Resolve(View, true, 40.f, Colors).Progress, 0.5f);
	TestEqual(TEXT("Past the limit clamps to empty"), FFIncidentMarkerRules::Resolve(View, true, 90.f, Colors).Progress, 0.f);
	TestEqual(TEXT("Tooltip rounds the remaining minutes up"),
		FFIncidentMarkerRules::Resolve(View, true, 40.5f, Colors).Tooltip.ToString(), FString(TEXT("남은 시간 약 30분")));
	TestEqual(TEXT("Without a festival clock the ring stays full"), FFIncidentMarkerRules::Resolve(View, false, 40.f, Colors).Progress, 1.f);

	View.MinuteUntileLapse = 0.f;
	TestEqual(TEXT("No time limit keeps the ring full"), FFIncidentMarkerRules::Resolve(View, true, 500.f, Colors).Progress, 1.f);
	TestTrue(TEXT("No time limit has no countdown tooltip"), FFIncidentMarkerRules::Resolve(View, true, 500.f, Colors).Tooltip.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentMarkerRulesDispatchRingTest,
	"FestaFiesta.UI.Incident.MarkerRules.RingFillsWithWorkAfterDispatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentMarkerRulesDispatchRingTest::RunTest(const FString& Parameters)
{
	const FFF_UIPaletteInGame Colors;
	FFF_IncidentView View;
	View.Urgency = EFF_IncidentUrgency::Immediate;
	View.OccurredAtMinutes = 0.f;
	View.MinuteUntileLapse = 60.f;

	View.ResponseStage = EFF_IncidentResponseStage::Travelling;
	FFF_IncidentMarkerLook Look = FFIncidentMarkerRules::Resolve(View, true, 30.f, Colors);
	TestTrue(TEXT("Travelling swaps to the dispatched glyph"), Look.Glyph == EFF_IncidentMarkerGlyph::Dispatched);
	TestEqual(TEXT("Dispatched ring is the dispatch color, not urgency"), Look.RingColor, Colors.IncidentDispatch);
	TestEqual(TEXT("Travelling has no work done yet"), Look.Progress, 0.f);
	TestTrue(TEXT("Dispatched markers stop the urgency motion"), Look.Motion == EFF_IncidentMarkerMotion::None);

	View.ResponseStage = EFF_IncidentResponseStage::Working;
	View.TotalWorkMinutes = 10.f;
	View.RemainingWorkMinutes = 4.f;
	TestEqual(TEXT("Working fills by work done"), FFIncidentMarkerRules::Resolve(View, true, 30.f, Colors).Progress, 0.6f);

	View.TotalWorkMinutes = 0.f;
	TestEqual(TEXT("Zero total work never divides by zero"), FFIncidentMarkerRules::Resolve(View, true, 30.f, Colors).Progress, 0.f);

	View.ResponseStage = EFF_IncidentResponseStage::None;
	Look = FFIncidentMarkerRules::Resolve(View, true, 30.f, Colors);
	TestTrue(TEXT("Cancelling returns to the incident glyph"), Look.Glyph == EFF_IncidentMarkerGlyph::Quest);
	TestEqual(TEXT("Cancelling returns to the urgency color"), Look.RingColor, Colors.Danger);
	TestEqual(TEXT("Cancelling returns to the remaining-time ring"), Look.Progress, 0.5f);
	return true;
}

#endif
