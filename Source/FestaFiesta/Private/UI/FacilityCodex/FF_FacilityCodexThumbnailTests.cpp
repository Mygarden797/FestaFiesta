// FF_FacilityCodexThumbnailTests.cpp
// 역할: 시설 도감의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexThumbnailTest,
	"FestaFiesta.FacilityCodex.UI.ThumbnailDiscoveryStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexThumbnailTest::RunTest(const FString& Parameters)
{
	using EState = EFF_FacilityCodexDiscoveryVisualState;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FFBuildUIAssetPaths::M_FF_Codex_DiscoveryThumbnailObject);
	if (!TestNotNull(TEXT("Authored discovery UI shader"), Material)) return false;
	TestTrue(TEXT("Discovery material is authored for the UI domain"), Material->IsUIMaterial());

	// Audit actual production art as well as the synthetic render fixture. A silhouette
	// shader over opaque catalog backdrops would turn every recipe into a rectangle.
	for (const TCHAR* Name : { TEXT("T_FoodTruck_Burger_Image"), TEXT("T_Seating_Image") })
	{
		UTexture2D* Source = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT(
			"/Game/Asset/FacilitiesAsset_HKH/Images/facilities/%s/%s.%s"), Name, Name, Name));
		if (!TestNotNull(TEXT("Production catalog thumbnail"), Source)) return false;
		TArray64<uint8> Bytes;
		if (!TestTrue(TEXT("Production thumbnail exposes RGBA source pixels"),
			Source->Source.GetFormat() == TSF_BGRA8 && Source->Source.GetMipData(Bytes, 0))) return false;
		int64 Transparent = 0, Opaque = 0;
		for (int64 Index = 3; Index < Bytes.Num(); Index += 4)
		{
			Transparent += Bytes[Index] == 0;
			Opaque += Bytes[Index] == 255;
		}
		TestTrue(FString::Printf(TEXT("%s has both a transparent backdrop and solid facility pixels"), Name),
			Transparent > 0 && Opaque > 0);
	}

	constexpr int32 Side = 64;
	TStrongObjectPtr<UTexture2D> Texture(UTexture2D::CreateTransient(Side, Side, PF_B8G8R8A8));
	if (!TestNotNull(TEXT("Two-color alpha test texture"), Texture.Get())) return false;
	FColor* Texels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	for (int32 Y = 0; Y < Side; ++Y)
		for (int32 X = 0; X < Side; ++X)
		{
			const bool bInside = X >= 8 && X < 56 && Y >= 8 && Y < 56;
			const uint8 Alpha = bInside ? (Y == 8 ? 128 : 255) : 0;
			Texels[Y * Side + X] = X < 32 ? FColor(225, 50, 35, Alpha) : FColor(30, 100, 225, Alpha);
		}
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->SRGB = true;
	Texture->Filter = TF_Nearest;
	Texture->UpdateResource();
	TStrongObjectPtr<UFF_FacilityCodexImage> Image(NewObject<UFF_FacilityCodexImage>());
	TStrongObjectPtr<UTextBlock> Fallback(NewObject<UTextBlock>());
	Image->DiscoveryMaterial = Material;
	Image->SetDiscoveryVisualState(EState::Silhouette);
	Image->ShowThumbnail(TSoftObjectPtr<UTexture2D>(Texture.Get()), Fallback.Get());
	TestEqual(TEXT("A texture arriving after state assignment uses the current silhouette state"),
		Image->GetDiscoveryVisualState(), EState::Silhouette);
	UMaterialInstanceDynamic* FirstMID = Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject());
	if (!TestNotNull(TEXT("Unknown thumbnail uses an instance of the discovery shader"), FirstMID)) return false;
	TestEqual(TEXT("Shader resource retains the original source dimensions"), FVector2D(Image->GetBrush().ImageSize), FVector2D(Side, Side));
	TestEqual(TEXT("Source identity remains available behind the material"), Image->GetLoadedThumbnail(), Texture.Get());
	Image->SetDiscoveryVisualState(EState::Grayscale);
	TestTrue(TEXT("Same-source state changes reuse the instance without a new load"), Image->GetBrush().GetResourceObject() == FirstMID);
	Image->SetDiscoveryVisualState(EState::Color);
	TestTrue(TEXT("Discovery restores the original colored texture directly"), Image->GetBrush().GetResourceObject() == Texture.Get());
	Image->ShowThumbnail({}, Fallback.Get());
	Image->SetDiscoveryVisualState(EState::Grayscale);
	TestTrue(TEXT("Releasing a pooled thumbnail clears its source and material"),
		!Image->GetLoadedThumbnail() && !Image->GetBrush().GetResourceObject() && Image->GetVisibility() == ESlateVisibility::Hidden);
	TestEqual(TEXT("A released thumbnail restores its authored placeholder"), Fallback->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Image->ShowThumbnail(TSoftObjectPtr<UTexture2D>(Texture.Get()), Fallback.Get());
	TestTrue(TEXT("Rebinding after pool release uses the new grayscale state"), Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()) != nullptr);
	TestEqual(TEXT("Loaded thumbnails hide the placeholder"), Fallback->GetVisibility(), ESlateVisibility::Collapsed);

	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
	Material->EnsureIsComplete();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FlushRenderingCommands();
	FWidgetRenderer Renderer(false, true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Side, Side), TF_Nearest, true));
	const TSharedRef<SWidget> Slate = Image->TakeWidget();
	TArray<FColor> Pixels[3];
	for (int32 State = 0; State < 3; ++State)
	{
		Image->SetDiscoveryVisualState(static_cast<EState>(State));
		for (int32 Frame = 0; Frame < 4; ++Frame)
			Renderer.DrawWidget(Target.Get(), Slate, FVector2D(Side, Side), .016f, false);
		FlushRenderingCommands();
		FReadSurfaceDataFlags Flags;
		Flags.SetLinearToGamma(false);
		if (!TestTrue(TEXT("Discovery state rendered to pixels"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels[State], Flags))) return false;
	}
	const int32 Left = 32 * Side + 16, Right = 32 * Side + 48;
	TestTrue(TEXT("Color state retains the red and blue source artwork"),
		Pixels[0][Left].R > Pixels[0][Left].B + 40 && Pixels[0][Right].B > Pixels[0][Right].R + 40);
	for (const int32 Index : { Left, Right })
		TestTrue(TEXT("Grayscale has equal rendered RGB channels"),
			FMath::Abs(int32(Pixels[1][Index].R) - int32(Pixels[1][Index].G)) <= 2 &&
			FMath::Abs(int32(Pixels[1][Index].G) - int32(Pixels[1][Index].B)) <= 2);
	TestTrue(TEXT("Silhouette hides all source color differences in its solid body"), Pixels[2][Left] == Pixels[2][Right]);
	TestTrue(TEXT("Silhouette body has the game's dark teal identity"), Pixels[2][Left].G > Pixels[2][Left].R && Pixels[2][Left].A > 250);
	int32 AlphaMismatches = 0;
	for (int32 Index = 0; Index < Side * Side; ++Index)
		AlphaMismatches += FMath::Abs(int32(Pixels[0][Index].A) - int32(Pixels[1][Index].A)) > 1 ||
			FMath::Abs(int32(Pixels[0][Index].A) - int32(Pixels[2][Index].A)) > 1;
	TestEqual(TEXT("All three states preserve source alpha including the translucent contour"), AlphaMismatches, 0);
	TestEqual(TEXT("Silhouette canvas remains transparent"), int32(Pixels[2][0].A), 0);

	TArray<FColor> Strip;
	Strip.SetNum(Side * Side * 3);
	for (int32 Y = 0; Y < Side; ++Y)
		for (int32 State = 0; State < 3; ++State)
			for (int32 X = 0; X < Side; ++X)
				Strip[Y * Side * 3 + State * Side + X] = Pixels[State][Y * Side + X];
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Side * 3, Side, Strip, Png);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("Shader-state evidence saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / TEXT("FacilityCodexThumbnailShaderStates.png"))));
	return true;
}
#endif
