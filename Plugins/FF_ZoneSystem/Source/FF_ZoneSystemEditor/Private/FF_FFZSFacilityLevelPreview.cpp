#include "FF_FFZSFacilityLevelPreview.h"

#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSVisualGeometry.h"
#include "Framework/Application/SlateApplication.h"
#include "PreviewScene.h"
#include "SEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FFZSFacilityLevelPreview"

namespace
{
    class SFFZSFacilityLevelViewport final : public SEditorViewport
    {
    public:
        SLATE_BEGIN_ARGS(SFFZSFacilityLevelViewport) {}
            SLATE_ARGUMENT(UFFZSFacilityDefinition*, Definition)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Definition = Args._Definition;
            RebuildLevelOptions();
            Scene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues());
            Mesh = NewObject<UStaticMeshComponent>();
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Scene->AddComponent(Mesh, FTransform::Identity);
            SEditorViewport::Construct(SEditorViewport::FArguments());
            SetLevel(1);
        }

        virtual ~SFFZSFacilityLevelViewport() override
        {
            // SEditorViewport also owns the client. Detach it before our preview scene dies.
            if (Client)
            {
                Client->Viewport = nullptr;
                if (SceneViewport)
                {
                    SceneViewport->SetViewportClient(nullptr);
                }
                Client.Reset();
            }
        }

        int32 GetLevel() const { return Level; }
        const TArray<TSharedPtr<int32>>& GetLevelOptions() const { return LevelOptions; }
        TSharedPtr<int32> GetSelectedLevelOption() const
        {
            for (const TSharedPtr<int32>& Option : LevelOptions)
            {
                if (*Option == Level) return Option;
            }
            return LevelOptions[0];
        }
        FText GetSummary() const { return Summary; }
        const UStaticMeshComponent* GetPreviewMesh() const { return Mesh; }

        void RefreshPreview()
        {
            RebuildLevelOptions();
            SetLevel(Level);
            if (Client)
            {
                FrameAllLevels(*Client);
            }
            Invalidate();
        }

        void SetLevel(int32 NewLevel)
        {
            const UFFZSFacilityDefinition* Asset = Definition.Get();
            if (!Asset)
            {
                Mesh->SetStaticMesh(nullptr);
                Summary = LOCTEXT("MissingDefinition", "시설 데이터 에셋을 다시 열어 주세요.");
                Invalidate();
                return;
            }
            Level = Asset->Spec.bUsesFacilityLevels &&
                FFZSFacilityUpgradeRules::FindTier(Asset->Spec.UpgradeTiers, NewLevel) ? NewLevel : 1;
            FText ConfigurationError;
            if (Asset->Spec.bUsesFacilityLevels &&
                !FFZSFacilityUpgradeRules::ValidateTiers(Asset->Spec.UpgradeTiers, ConfigurationError))
            {
                Mesh->SetStaticMesh(nullptr);
                Summary = ConfigurationError;
                Invalidate();
                return;
            }
            const FFFZSFacilityUpgradeTier* Tier = Asset->Spec.bUsesFacilityLevels
                ? FFZSFacilityUpgradeRules::FindTier(Asset->Spec.UpgradeTiers, Level) : nullptr;
            const float Size = Tier ? Tier->SizeMultiplier : 1.f;
            const FFFZSFacilitySpec Effective = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Asset->Spec, Level, Size);
            if (Effective.FootprintSizeCells.X <= 0 || Effective.FootprintSizeCells.Y <= 0 ||
                !FMath::IsFinite(Effective.PlacementObstacleHeightCm) || Effective.PlacementObstacleHeightCm <= 0.f)
            {
                Mesh->SetStaticMesh(nullptr);
                Summary = LOCTEXT("UnrepresentableGeometry", "이 단계의 크기를 계산할 수 없습니다. 건물 크기와 배율을 확인해 주세요.");
                Invalidate();
                return;
            }
            // This explicit editor action can load the selected mesh; gameplay uses the async path.
            UStaticMesh* LoadedMesh = Effective.VisualMesh.LoadSynchronous();
            if (!LoadedMesh && Effective.VisualMesh != Asset->Spec.VisualMesh)
            {
                LoadedMesh = Asset->Spec.VisualMesh.LoadSynchronous();
            }
            Mesh->SetStaticMesh(LoadedMesh);
            FTransform Transform = FFZSVisualGeometry::ResolveVisualMeshTransform(Effective, LoadedMesh);
            if (Effective.VisualSizingMode == EFFZSVisualSizingMode::SourceMeshSize)
            {
                Transform.SetScale3D(Transform.GetScale3D() * Size);
            }
            Mesh->SetRelativeTransform(Transform);
            Summary = FText::Format(LOCTEXT("LevelSummary",
                "{0} · {1} × {2} × {3} cm · {4}\n데이터 수정 후 Refresh Preview를 누르세요. 미지정 외형은 Lv.1 기본 메시를 사용합니다."),
                Asset->Spec.bUsesFacilityLevels
                    ? FText::Format(LOCTEXT("Level", "Lv.{0}"), Level) : LOCTEXT("NoLevels", "레벨 없는 시설"),
                FText::AsNumber(Effective.FootprintSizeCells.X),
                FText::AsNumber(Effective.FootprintSizeCells.Y),
                FText::AsNumber(Effective.PlacementObstacleHeightCm),
                LoadedMesh ? FText::FromString(LoadedMesh->GetName()) : LOCTEXT("NoMesh", "표시할 메시 없음"));
            Invalidate();
        }

    protected:
        virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
        {
            TSharedRef<FEditorViewportClient> Result = MakeShared<FEditorViewportClient>(
                nullptr, Scene.Get(), SharedThis(this));
            Result->SetViewMode(VMI_Lit);
            Result->SetRealtime(false);
            Result->EngineShowFlags.SetGrid(true);
            Result->EngineShowFlags.SetSelectionOutline(false);
            FrameAllLevels(*Result);
            return Result;
        }

    private:
        void RebuildLevelOptions()
        {
            LevelOptions.Reset();
            LevelOptions.Add(MakeShared<int32>(1));
            const UFFZSFacilityDefinition* Asset = Definition.Get();
            FText Error;
            if (Asset && Asset->Spec.bUsesFacilityLevels &&
                FFZSFacilityUpgradeRules::ValidateTiers(Asset->Spec.UpgradeTiers, Error))
            {
                for (const FFFZSFacilityUpgradeTier& Tier : Asset->Spec.UpgradeTiers)
                {
                    LevelOptions.Add(MakeShared<int32>(Tier.TargetLevel));
                }
                LevelOptions.Sort([](const TSharedPtr<int32>& A, const TSharedPtr<int32>& B) { return *A < *B; });
            }
        }

        void FrameAllLevels(FEditorViewportClient& ViewClient)
        {
            const UFFZSFacilityDefinition* Asset = Definition.Get();
            FBox Bounds(ForceInit);
            if (Asset)
            {
                // Geometry-only input avoids copying/scanning the entire tier array for every level.
                FFFZSFacilitySpec GeometryBase = Asset->Spec;
                GeometryBase.UpgradeTiers.Reset();
                GeometryBase.bUsesFacilityLevels = false;
                const auto AddLevelBounds = [&](const FFFZSFacilityUpgradeTier* Tier)
                {
                    const float Scale = Tier ? Tier->SizeMultiplier : 1.f;
                    FFFZSFacilitySpec Spec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(GeometryBase, 1, Scale);
                    if (Tier && !Tier->VisualMesh.IsNull()) Spec.VisualMesh = Tier->VisualMesh;
                    if (Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0 ||
                        !FMath::IsFinite(Spec.PlacementObstacleHeightCm) || Spec.PlacementObstacleHeightCm <= 0.f)
                    {
                        return;
                    }
                    UStaticMesh* LevelMesh = Spec.VisualMesh.LoadSynchronous();
                    if (!LevelMesh && Spec.VisualMesh != Asset->Spec.VisualMesh)
                    {
                        LevelMesh = Asset->Spec.VisualMesh.LoadSynchronous();
                    }
                    if (LevelMesh)
                    {
                        FTransform Transform = FFZSVisualGeometry::ResolveVisualMeshTransform(Spec, LevelMesh);
                        if (Spec.VisualSizingMode == EFFZSVisualSizingMode::SourceMeshSize)
                        {
                            Transform.SetScale3D(Transform.GetScale3D() * Scale);
                        }
                        Bounds += FFZSVisualGeometry::TransformBounds(LevelMesh->GetBoundingBox(), Transform);
                    }
                    else
                    {
                        Bounds += FBox(FVector(-Spec.FootprintSizeCells.X * .5, -Spec.FootprintSizeCells.Y * .5, 0),
                            FVector(Spec.FootprintSizeCells.X * .5, Spec.FootprintSizeCells.Y * .5, Spec.PlacementObstacleHeightCm));
                    }
                };
                AddLevelBounds(nullptr);
                FText Error;
                if (Asset->Spec.bUsesFacilityLevels &&
                    FFZSFacilityUpgradeRules::ValidateTiers(Asset->Spec.UpgradeTiers, Error))
                {
                    for (const FFFZSFacilityUpgradeTier& Tier : Asset->Spec.UpgradeTiers) AddLevelBounds(&Tier);
                }
            }
            const FVector Center = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
            const double Radius = Bounds.IsValid ? FMath::Max(100., Bounds.GetExtent().Size()) : 500.;
            const FVector Location = Center + FVector(-Radius * 1.4, -Radius * 1.4, Radius);
            // Reframe only on creation/explicit refresh; level changes retain this comparison view.
            ViewClient.SetViewLocation(Location);
            ViewClient.SetViewRotation((Center - Location).Rotation());
        }

        TWeakObjectPtr<UFFZSFacilityDefinition> Definition;
        TUniquePtr<FPreviewScene> Scene;
        TArray<TSharedPtr<int32>> LevelOptions;
        UStaticMeshComponent* Mesh = nullptr; // FPreviewScene owns the GC reference.
        int32 Level = 1;
        FText Summary;
    };
}

void OpenFFZSFacilityLevelPreview(UFFZSFacilityDefinition* Definition)
{
    if (!Definition || !FSlateApplication::IsInitialized())
    {
        return;
    }
    TSharedRef<SFFZSFacilityLevelViewport> Preview = SNew(SFFZSFacilityLevelViewport).Definition(Definition);
    const TSharedRef<SComboBox<TSharedPtr<int32>>> LevelPicker = SNew(SComboBox<TSharedPtr<int32>>)
        .OptionsSource(&Preview->GetLevelOptions())
        .InitiallySelectedItem(Preview->GetSelectedLevelOption())
        .OnGenerateWidget_Lambda([](TSharedPtr<int32> Item)
        {
            return SNew(STextBlock).Text(FText::AsNumber(*Item));
        })
        .OnSelectionChanged_Lambda([Preview](TSharedPtr<int32> Item, ESelectInfo::Type)
        {
            if (Item) Preview->SetLevel(*Item);
        })
        [ SNew(STextBlock).Text_Lambda([Preview] { return FText::AsNumber(Preview->GetLevel()); }) ];
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::Format(LOCTEXT("WindowTitle", "Facility Level Preview — {0}"), Definition->Spec.DisplayName))
        .ClientSize(FVector2D(720, 580));
    Window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(10)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ SNew(STextBlock).Text(LOCTEXT("PreviewLevel", "Preview Level")) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(10, 0)
            [
                SNew(SBox).MinDesiredWidth(100)[ LevelPicker ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton).Text(LOCTEXT("Refresh", "Refresh Preview"))
                .OnClicked_Lambda([Preview, LevelPicker]
                {
                    Preview->RefreshPreview();
                    LevelPicker->RefreshOptions();
                    LevelPicker->SetSelectedItem(Preview->GetSelectedLevelOption());
                    return FReply::Handled();
                })
            ]
        ]
        + SVerticalBox::Slot().FillHeight(1.f)[ Preview ]
        + SVerticalBox::Slot().AutoHeight().Padding(10)
        [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([Preview] { return Preview->GetSummary(); }) ]);
    FSlateApplication::Get().AddWindow(Window);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityLevelPreviewTest,
    "FF_ZoneSystem.Editor.FacilityLevelPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityLevelPreviewTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TStrongObjectPtr<UFFZSFacilityDefinition> Definition(NewObject<UFFZSFacilityDefinition>());
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!TestNotNull(TEXT("Base preview mesh"), Cube) || !TestNotNull(TEXT("Level preview mesh"), Sphere))
    {
        return false;
    }
    Definition->Spec.VisualMesh = Cube;
    Definition->Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    Definition->Spec.FootprintSizeCells = FIntPoint(100, 100);
    Definition->Spec.PlacementObstacleHeightCm = 100.f;
    Definition->Spec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Level2;
    Level2.TargetLevel = 2;
    Level2.SizeMultiplier = 2.f;
    Level2.VisualMesh = Sphere;
    FFFZSFacilityUpgradeTier Level3;
    Level3.TargetLevel = 3;
    Level3.ServiceDurationMultiplier = .65f;
    Level3.SizeMultiplier = 3.f;
    Definition->Spec.UpgradeTiers = {Level2, Level3};
    TSharedRef<SFFZSFacilityLevelViewport> Preview = SNew(SFFZSFacilityLevelViewport).Definition(Definition.Get());
    const FVector Camera = Preview->GetViewportClient()->GetViewLocation();
    Preview->SetLevel(2);
    TestTrue(TEXT("Authored level mesh is visible in preview"), Preview->GetPreviewMesh()->GetStaticMesh() == Sphere);
    Preview->SetLevel(3);
    TestTrue(TEXT("An unassigned mesh returns to level 1, not level 2"), Preview->GetPreviewMesh()->GetStaticMesh() == Cube);
    TestTrue(TEXT("Preview uses the configured physical size"),
        Preview->GetPreviewMesh()->Bounds.BoxExtent.Equals(FVector(150), .1));
    TestEqual(TEXT("Camera does not auto-fit away the level size difference"),
        Preview->GetViewportClient()->GetViewLocation(), Camera);
    TestEqual(TEXT("Preview leaves the authoring base size unchanged"),
        Definition->Spec.FootprintSizeCells, FIntPoint(100, 100));
    Definition->Spec.bUsesFacilityLevels = false;
    Preview->SetLevel(3);
    TestEqual(TEXT("Facilities without levels only preview the base"), Preview->GetLevel(), 1);
    TestTrue(TEXT("Level-less preview uses the base mesh"), Preview->GetPreviewMesh()->GetStaticMesh() == Cube);

    // A high label and unsorted sparse entries must not create billions of preview iterations.
    Definition->Spec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier HighLevel;
    HighLevel.TargetLevel = MAX_int32;
    HighLevel.SizeMultiplier = .5f;
    HighLevel.VisualMesh = Sphere;
    FFFZSFacilityUpgradeTier LowerLevel;
    LowerLevel.TargetLevel = 25;
    Definition->Spec.UpgradeTiers = {HighLevel, LowerLevel};
    Preview->RefreshPreview();
    TestEqual(TEXT("Preview lists only authored labels and the base"), Preview->GetLevelOptions().Num(), 3);
    TestEqual(TEXT("Preview orders sparse labels for selection"), *Preview->GetLevelOptions()[1], 25);
    Preview->SetLevel(MAX_int32);
    TestEqual(TEXT("Preview accepts labels above the old level cap"), Preview->GetLevel(), MAX_int32);
    TestTrue(TEXT("Preview applies a shrinking level mesh"), Preview->GetPreviewMesh()->GetStaticMesh() == Sphere);
    TestTrue(TEXT("Preview exposes the smaller physical shape"),
        Preview->GetPreviewMesh()->Bounds.BoxExtent.Equals(FVector(25), .1));
    return true;
}
#endif

#undef LOCTEXT_NAMESPACE
