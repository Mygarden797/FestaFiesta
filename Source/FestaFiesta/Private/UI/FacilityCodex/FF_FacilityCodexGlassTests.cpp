// FF_FacilityCodexGlassTests.cpp
// 역할: 시설 도감의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexGlassMaterialTest,
    "FestaFiesta.FacilityCodex.UI.GlassMaterialRendering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexGlassMaterialTest::RunTest(const FString& Parameters)
{
    struct FRole
    {
        const TCHAR* Name;
        float Opacity;
        float Motion;
    };
    const FRole Roles[] = {
        {TEXT("Screen"), .96f, 1.f}, {TEXT("Header"), .96f, 1.f},
        {TEXT("Card"), .90f, 0.f}, {TEXT("Selection"), 0.f, 1.f},
        {TEXT("Detail"), .94f, 1.f}, {TEXT("Inset"), .82f, 0.f}
    };
    TArray<TStrongObjectPtr<UMaterialInterface>> Materials;
    for (const FRole& Role : Roles)
    {
        const FString Path = FString::Printf(TEXT(
            "/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/MI_FF_CodexGlass_%s.MI_FF_CodexGlass_%s"),
            Role.Name, Role.Name);
        auto* Material = LoadObject<UMaterialInterface>(nullptr, *Path);
        if (!TestNotNull(FString::Printf(TEXT("%s uses its saved glass material"), Role.Name), Material)) return false;
        Materials.Emplace(Material);
        TestTrue(FString::Printf(TEXT("%s is a UI-domain translucent material"), Role.Name),
            Material->IsUIMaterial() && Material->GetBlendMode() == BLEND_Translucent);
        float Opacity = -1.f, Motion = -1.f;
        if (TestTrue(TEXT("Glass opacity parameter is authored"), Material->GetScalarParameterValue(
            FHashedMaterialParameterInfo(TEXT("SurfaceOpacity")), Opacity)))
            TestTrue(FString::Printf(TEXT("%s retains the agreed glass opacity"), Role.Name), FMath::IsNearlyEqual(Opacity, Role.Opacity));
        if (TestTrue(TEXT("Glass motion parameter is authored"), Material->GetScalarParameterValue(
            FHashedMaterialParameterInfo(TEXT("MotionStrength")), Motion)))
            TestTrue(FString::Printf(TEXT("%s motion matches its role; cards and insets remain still"), Role.Name),
                FMath::IsNearlyEqual(Motion, Role.Motion));
    }

    // NullRHI can audit saved assets but cannot establish shader rendering evidence.
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
    for (const auto& Material : Materials) Material->EnsureIsComplete();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();

    FWidgetRenderer Renderer(false, true);
    auto Render = [&](UMaterialInterface* Material, FIntPoint Size, TArray<FColor>& Pixels)
    {
        TStrongObjectPtr<UImage> Image(NewObject<UImage>());
        Image->SetBrushFromMaterial(Material);
        Image->SetColorAndOpacity(FLinearColor::White);
        Image->SetDesiredSizeOverride(FVector2D(Size.X, Size.Y));
        const TSharedRef<SWidget> Slate = Image->TakeWidget();
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(
            FVector2D(Size.X, Size.Y), TF_Nearest, true));
        if (!TestNotNull(TEXT("Transparent glass render target"), Target.Get())) return false;
        // Clear on every frame; repeated draws must not accumulate opacity.
        for (int32 Frame = 0; Frame < 4; ++Frame)
            Renderer.DrawWidget(Target.Get(), Slate, FVector2D(Size.X, Size.Y), .016f, false);
        FlushRenderingCommands();
        FReadSurfaceDataFlags Flags;
        Flags.SetLinearToGamma(false);
        return TestTrue(TEXT("Glass UI shader produces actual pixels"),
            Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) &&
            TestEqual(TEXT("Glass readback has the complete target extent"), Pixels.Num(), Size.X * Size.Y);
    };

    constexpr int32 Width = 240, Height = 160;
    TArray<FColor> Samples[UE_ARRAY_COUNT(Roles)];
    FString AlphaEvidence;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Roles); ++Index)
    {
        if (!Render(Materials[Index].Get(), FIntPoint(Width, Height), Samples[Index])) return false;
        const FColor Center = Samples[Index][Height / 2 * Width + Width / 2];
        AlphaEvidence += FString::Printf(TEXT("%s=%d/255 "), Roles[Index].Name, int32(Center.A));
        const bool bSelection = Index == 3;
        if (bSelection)
        {
            TestEqual(TEXT("Selected frame does not fog or cover its content"), int32(Center.A), 0);
            TestTrue(TEXT("Selected frame still draws a visible pearl rim"), Samples[Index][Height / 2 * Width + 1].A > 100);
        }
        else
        {
            TestTrue(FString::Printf(TEXT("%s center is actually translucent, not an opaque pastel"), Roles[Index].Name),
                Center.A > 0 && Center.A < 255);
            TestTrue(FString::Printf(TEXT("%s rendered alpha follows the saved opacity"), Roles[Index].Name),
                FMath::Abs(int32(Center.A) - FMath::RoundToInt(Roles[Index].Opacity * 255.f)) <= 3);
        }
        TestEqual(FString::Printf(TEXT("%s rounded corner remains transparent"), Roles[Index].Name), int32(Samples[Index][0].A), 0);
    }
    AddInfo(TEXT("Rendered glass center alpha: ") + AlphaEvidence);

    // A fixed UV bevel would stretch by five times here. Read real rim pixels in
    // both directions to protect derivative-based sizing across fullscreen layouts.
    auto RimThickness = [](const TArray<FColor>& Pixels, FIntPoint Size, bool bVertical)
    {
        int32 Count = 0;
        for (int32 Offset = 0; Offset < 12; ++Offset)
        {
            const int32 Index = bVertical ? Offset * Size.X + Size.X / 2 : Size.Y / 2 * Size.X + Offset;
            Count += Pixels[Index].A > 96;
        }
        return Count;
    };
    const FIntPoint WideSize(480, 96), TallSize(96, 480);
    TArray<FColor> Wide, Tall;
    if (!Render(Materials[3].Get(), WideSize, Wide) || !Render(Materials[3].Get(), TallSize, Tall)) return false;
    const int32 Thickness[] = {
        RimThickness(Wide, WideSize, false), RimThickness(Wide, WideSize, true),
        RimThickness(Tall, TallSize, false), RimThickness(Tall, TallSize, true)
    };
    int32 Minimum = 12, Maximum = 0;
    for (int32 Value : Thickness)
    {
        Minimum = FMath::Min(Minimum, Value);
        Maximum = FMath::Max(Maximum, Value);
    }
    TestTrue(TEXT("Iridescent selection rim remains thin and visible at both aspect ratios"), Minimum >= 1 && Maximum <= 5);
    TestTrue(TEXT("Iridescent rim thickness stays within one pixel across horizontal and vertical stretch"), Maximum - Minimum <= 1);

    // Retain the actual RGBA outputs, including their transparency, for inspection.
    TArray<FColor> Strip;
    Strip.SetNum(Width * Height * UE_ARRAY_COUNT(Roles));
    for (int32 Y = 0; Y < Height; ++Y)
        for (int32 RoleIndex = 0; RoleIndex < UE_ARRAY_COUNT(Roles); ++RoleIndex)
            for (int32 X = 0; X < Width; ++X)
                Strip[Y * Width * UE_ARRAY_COUNT(Roles) + RoleIndex * Width + X] = Samples[RoleIndex][Y * Width + X];
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width * UE_ARRAY_COUNT(Roles), Height, Strip, Png);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
    IFileManager::Get().MakeDirectory(*Directory, true);
    TestTrue(TEXT("Actual translucent material samples saved"), FFileHelper::SaveArrayToFile(
        Png, *(Directory / TEXT("FacilityCodexGlassMaterialSamples.png"))));
    return true;
}
#endif
