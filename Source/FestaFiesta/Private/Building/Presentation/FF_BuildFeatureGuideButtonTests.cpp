#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildFeatureGuideButton.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildFeatureGuideButtonGuardsTest,
	"FestaFiesta.Building.FeatureGuide.ButtonGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFeatureGuideButtonGuardsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UFF_BuildFeatureGuideButton> Button(NewObject<UFF_BuildFeatureGuideButton>());
	if (!TestNotNull(TEXT("Guide button"), Button.Get())) return false;
	Button->FeatureTopicId = NAME_None;
	TestFalse(TEXT("Empty topic never opens a guide"), Button->TryOpenGuide());
	Button->FeatureTopicId = TEXT("NotAFeatureTopic");
	TestFalse(TEXT("Unknown topic never opens a guide"), Button->TryOpenGuide());
	Button->FeatureTopicId = TEXT("BuildGrid");
	TestFalse(TEXT("Known topic without a same-player enclosing tools widget is refused"), Button->TryOpenGuide());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildFeatureGuideAtlasTest,
	"FestaFiesta.Building.FeatureGuide.IconAtlasRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFeatureGuideAtlasTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UFF_BuildFeatureGuideButton> Button(NewObject<UFF_BuildFeatureGuideButton>());
	UTexture2D* Texture = NewObject<UTexture2D>(Button.Get());
	Button->IconBrush.SetResourceObject(Texture);
	const FBox2f Authored(FVector2f(.1f, .2f), FVector2f(.3f, .4f));
	Button->IconBrush.SetUVRegion(Authored);
	TestTrue(TEXT("Standalone icons retain the designer-authored UV region"),
		static_cast<FBox2f>(Button->ResolveIconBrush().GetUVRegion()) == Authored);
	Button->bUseIconAtlas = true;
	Button->IconAtlasColumns = 5;
	Button->IconAtlasRows = 5;
	for (int32 Cell : {0, 4, 5, 21, 24})
	{
		Button->IconAtlasCell = Cell;
		const FSlateBrush Resolved = Button->ResolveIconBrush();
		const FBox2f UV = Resolved.GetUVRegion();
		const int32 Column = Cell % 5;
		const int32 Row = Cell / 5;
		TestTrue(TEXT("Each cell addresses the exact row-major atlas region"),
			UV.bIsValid && UV.Min.Equals(FVector2f(Column / 5.f, Row / 5.f), .00001f) &&
			UV.Max.Equals(FVector2f((Column + 1) / 5.f, (Row + 1) / 5.f), .00001f));
		TestTrue(TEXT("Atlas region retains the designer's texture"), Resolved.GetResourceObject() == Texture);
	}
	Button->IconAtlasCell = 25;
	TestNull(TEXT("Invalid cells do not display the entire atlas"), Button->ResolveIconBrush().GetResourceObject());
	Button->bUseIconAtlas = false;
	TestTrue(TEXT("Applying an atlas cell never mutates the authored brush"),
		static_cast<FBox2f>(Button->ResolveIconBrush().GetUVRegion()) == Authored);
	return true;
}

#endif
