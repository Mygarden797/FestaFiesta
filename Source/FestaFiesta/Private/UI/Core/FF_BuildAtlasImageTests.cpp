#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Core/FF_BuildAtlasImage.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildAtlasImageTest,
	"FestaFiesta.UI.BuildAtlasImage.CellAndState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildAtlasImageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UFF_BuildAtlasImage> Image(NewObject<UFF_BuildAtlasImage>());
	TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>());
	if (!TestNotNull(TEXT("Atlas image"), Image.Get()) || !TestNotNull(TEXT("Atlas texture"), Texture.Get())) return false;
	FSlateBrush Authored = Image->GetBrush();
	Authored.SetImageSize(FVector2f(24.f, 20.f));
	Authored.TintColor = FSlateColor(FLinearColor(0.3f, 0.6f, 0.9f, 0.7f));
	Image->SetBrush(Authored);
	Image->SetAtlasSource(Texture.Get(), 5, 5, 22);
	const FSlateBrush& Required = Image->GetBrush();
	TestTrue(TEXT("Image uses the original texture"), Required.GetResourceObject() == Texture.Get());
	const FBox2f RequiredUV = Required.GetUVRegion();
	TestTrue(TEXT("Required badge uses the exact approved fifth-row third-column cell"),
		RequiredUV.bIsValid && RequiredUV.Min.Equals(FVector2f(0.4f, 0.8f), 0.0001f) &&
		RequiredUV.Max.Equals(FVector2f(0.6f, 1.0f), 0.0001f));
	const FVector2f ActualSize = Required.GetImageSize();
	TestTrue(TEXT("Designer-authored size is preserved"), ActualSize.Equals(FVector2f(24.f, 20.f)));
	TestTrue(TEXT("Designer-authored tint is preserved"), Required.TintColor == Authored.TintColor);
	Image->SetAtlasIndex(23);
	const FBox2f LightingUV = Image->GetBrush().GetUVRegion();
	TestTrue(TEXT("Cell changes without replacing the source texture"), Image->GetBrush().GetResourceObject() == Texture.Get() &&
		LightingUV.Min.Equals(FVector2f(0.6f, 0.8f), 0.0001f) && LightingUV.Max.Equals(FVector2f(0.8f, 1.f), 0.0001f));
	const FSlateBrush Previous = Image->GetBrush();
	TestFalse(TEXT("Negative index is rejected"), Image->SetAtlasSource(Texture.Get(), 4, 3, -99));
	TestFalse(TEXT("Overflow index is rejected"), Image->SetAtlasIndex(999));
	TestFalse(TEXT("Null and invalid grid are rejected"), Image->SetAtlasSource(nullptr, 0, 0, 2));
	TestTrue(TEXT("Invalid input leaves the brush unchanged"), Image->GetBrush() == Previous);
	FSlateBrush SharedBrush = Previous;
	TestFalse(TEXT("The shared helper rejects invalid cells"), FFBuildAtlas::TryApply(SharedBrush, Texture.Get(), 4, 3, 12));
	TestTrue(TEXT("The shared helper preserves the brush on failure"), SharedBrush == Previous);
	TestTrue(TEXT("A valid utility atlas cell applies"), FFBuildAtlas::TryApply(SharedBrush, Texture.Get(), 4, 3, 1));
	const FBox2f FavoriteUV = SharedBrush.GetUVRegion();
	TestTrue(TEXT("Utility index 1 is the second cell in the first row"),
		FavoriteUV.Min.Equals(FVector2f(.25f, 0.f), .0001f) && FavoriteUV.Max.Equals(FVector2f(.5f, 1.f / 3.f), .0001f));
	return true;
}

#endif
