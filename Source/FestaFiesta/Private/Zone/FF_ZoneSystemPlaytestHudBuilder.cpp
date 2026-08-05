#include "CoreMinimal.h"

#if WITH_EDITOR

// 에디터 전용 최초 생성 도구다. FF.BuildZonePlaytestHUD 명령으로 WBP가 없을 때만 만들며
// 이미 존재하는 자산과 디자이너의 수정 내용은 절대 덮어쓰지 않는다.
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Zone/FF_ZoneSystemPlaytestHUD.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFZoneHudBuilder, Log, All);

namespace
{
    // 런타임 subsystem의 soft class 경로와 반드시 같은 패키지를 가리켜야 한다.
    const FString HudPackageName(TEXT("/Game/FestaFiesta/Zone/UI/WBP_ZoneSystemPlaytestHUD"));

    template <typename TWidget>
    TWidget* MakeWidget(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name, const bool bVariable = false)
    {
        TWidget* Widget = Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
        if (Widget)
        {
            Widget->bIsVariable = bVariable;
            // WidgetBlueprint 컴파일러는 변수 여부와 관계없이 이름이 있는 모든 원본 위젯에
            // 안정적인 GUID를 요구하므로 라벨과 배경 패널도 변수 목록에 등록한다.
            Blueprint.OnVariableAdded(Name);
        }
        return Widget;
    }

    void Place(UCanvasPanel& Root, UWidget& Widget, const FVector2D Anchor, const FVector2D Position,
        const FVector2D Size, const FVector2D Alignment = FVector2D::ZeroVector, const int32 Z = 1)
    {
        if (UCanvasPanelSlot* Slot = Root.AddChildToCanvas(&Widget))
        {
            Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
            Slot->SetPosition(Position);
            Slot->SetSize(Size);
            Slot->SetAlignment(Alignment);
            Slot->SetZOrder(Z);
        }
    }

    UTextBlock* MakeText(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name,
        const FString& Value, const int32 Size, const bool bVariable = false)
    {
        UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, Name, bVariable);
        if (Text)
        {
            Text->SetText(FText::FromString(Value));
            FSlateFontInfo Font = Text->GetFont();
            Font.Size = Size;
            Text->SetFont(Font);
        }
        return Text;
    }

    UButton* MakeButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name, const FString& Label)
    {
        UButton* Button = MakeWidget<UButton>(Blueprint, Tree, Name, true);
        if (Button)
        {
            Button->SetBackgroundColor(FLinearColor(0.08f, 0.18f, 0.23f, 0.98f));
            UTextBlock* Text = MakeText(Blueprint, Tree,
                FName(*(Name.ToString() + TEXT("_Label"))), Label, 15);
            Text->SetJustification(ETextJustify::Center);
            Button->AddChild(Text);
        }
        return Button;
    }

    bool SaveBlueprint(UWidgetBlueprint& Blueprint)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
        FKismetEditorUtilities::CompileBlueprint(&Blueprint);
        Blueprint.MarkPackageDirty();

        UPackage* Package = Blueprint.GetOutermost();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, &Blueprint, *Filename, Args);
    }

    void BuildZoneSystemPlaytestHud()
    {
        // 기존 자산이 있으면 즉시 종료해 사용자가 Designer에서 바꾼 배치와 스타일을 보호한다.
        if (FPackageName::DoesPackageExist(HudPackageName))
        {
            UE_LOG(LogFFZoneHudBuilder, Display,
                TEXT("FF_ZONE_HUD_BUILDER_SKIP: %s exists; designer changes were preserved."), *HudPackageName);
            return;
        }

        UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
        Factory->ParentClass = UFF_ZoneSystemPlaytestHUD::StaticClass();
        FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(AssetTools.Get().CreateAsset(
            TEXT("WBP_ZoneSystemPlaytestHUD"), TEXT("/Game/FestaFiesta/Zone/UI"),
            UWidgetBlueprint::StaticClass(), Factory));
        if (!Blueprint || !Blueprint->WidgetTree)
        {
            UE_LOG(LogFFZoneHudBuilder, Error, TEXT("FF_ZONE_HUD_BUILDER_FAIL: asset creation failed."));
            return;
        }

        UWidgetTree& Tree = *Blueprint->WidgetTree;
        UCanvasPanel* Root = MakeWidget<UCanvasPanel>(*Blueprint, Tree, TEXT("RootCanvas"));
        Tree.RootWidget = Root;

        auto AddBackground = [&](const FName Name, const FLinearColor Color, const FVector2D Anchor,
            const FVector2D Position, const FVector2D Size, const FVector2D Alignment)
        {
            UBorder* Border = MakeWidget<UBorder>(*Blueprint, Tree, Name);
            Border->SetBrushColor(Color);
            Place(*Root, *Border, Anchor, Position, Size, Alignment, 0);
        };

        AddBackground(TEXT("PANEL_Status"), FLinearColor(0.012f, 0.025f, 0.038f, 0.90f),
            FVector2D::ZeroVector, FVector2D(24.0f, 24.0f), FVector2D(540.0f, 238.0f), FVector2D::ZeroVector);
        UTextBlock* Title = MakeText(*Blueprint, Tree, TEXT("TXT_Title"),
            TEXT("FF ZONE SYSTEM  //  LIVE PLAYTEST"), 21);
        Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.18f, 0.86f, 1.0f, 1.0f)));
        Place(*Root, *Title, FVector2D::ZeroVector, FVector2D(44.0f, 40.0f), FVector2D(500.0f, 30.0f), FVector2D::ZeroVector, 2);

        struct FField { FName Name; const TCHAR* Text; int32 Size; float Y; };
        const FField Fields[] = {
            { TEXT("TXT_Zone"), TEXT("ZONE  loading..."), 16, 84.0f },
            { TEXT("TXT_Preset"), TEXT("PRESET  loading..."), 18, 116.0f },
            { TEXT("TXT_Placement"), TEXT("TARGET  loading..."), 15, 150.0f },
            { TEXT("TXT_Validation"), TEXT("PLACEMENT VALIDATION"), 18, 184.0f },
            { TEXT("TXT_Status"), TEXT("STATUS"), 14, 218.0f }
        };
        for (const FField& Field : Fields)
        {
            UTextBlock* Text = MakeText(*Blueprint, Tree, Field.Name, Field.Text, Field.Size, true);
            Place(*Root, *Text, FVector2D::ZeroVector, FVector2D(44.0f, Field.Y),
                FVector2D(500.0f, 28.0f), FVector2D::ZeroVector, 2);
        }

        AddBackground(TEXT("PANEL_Help"), FLinearColor(0.012f, 0.025f, 0.038f, 0.82f),
            FVector2D(1.0f, 0.0f), FVector2D(-24.0f, 24.0f), FVector2D(610.0f, 120.0f), FVector2D(1.0f, 0.0f));
        UTextBlock* Help = MakeText(*Blueprint, Tree, TEXT("TXT_Help"), TEXT("WASD Pan / Wheel Zoom / RMB Rotate / Tab HUD"), 14, true);
        Help->SetAutoWrapText(true);
        Place(*Root, *Help, FVector2D(1.0f, 0.0f), FVector2D(-44.0f, 42.0f),
            FVector2D(570.0f, 86.0f), FVector2D(1.0f, 0.0f), 2);

        UTextBlock* Crosshair = MakeText(*Blueprint, Tree, TEXT("TXT_Crosshair"), TEXT("+"), 30);
        Crosshair->SetJustification(ETextJustify::Center);
        Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor(0.18f, 0.86f, 1.0f, 0.92f)));
        Place(*Root, *Crosshair, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector,
            FVector2D(48.0f, 48.0f), FVector2D(0.5f, 0.5f), 3);

        AddBackground(TEXT("PANEL_Actions"), FLinearColor(0.012f, 0.025f, 0.038f, 0.90f),
            FVector2D(0.5f, 1.0f), FVector2D(0.0f, -18.0f), FVector2D(1120.0f, 172.0f), FVector2D(0.5f, 1.0f));

        // TXT_*와 BTN_* 이름은 네이티브 BindWidgetOptional 연결 계약이므로 생성 후에도 유지해야 한다.
        const TArray<TArray<TPair<FName, FString>>> Rows = {
            {
                { TEXT("BTN_PresetPrevious"), TEXT("PREVIOUS PRESET") }, { TEXT("BTN_PresetNext"), TEXT("NEXT PRESET") },
                { TEXT("BTN_Install"), TEXT("INSTALL") }, { TEXT("BTN_Move"), TEXT("MOVE LAST") },
                { TEXT("BTN_Remove"), TEXT("REMOVE") }, { TEXT("BTN_Reset"), TEXT("RESET ALL") }
            },
            {
                { TEXT("BTN_Open"), TEXT("OPEN") }, { TEXT("BTN_Close"), TEXT("CLOSE") },
                { TEXT("BTN_Emergency"), TEXT("EMERGENCY") }, { TEXT("BTN_QueueJoin"), TEXT("QUEUE JOIN") },
                { TEXT("BTN_QueueAdvance"), TEXT("QUEUE STEP") }, { TEXT("BTN_QueueBlock"), TEXT("QUEUE BLOCK") }
            },
            {
                { TEXT("BTN_SnapshotSave"), TEXT("SAVE SNAPSHOT") }, { TEXT("BTN_SnapshotRestore"), TEXT("RESTORE") },
                { TEXT("BTN_Debug"), TEXT("WORLD DEBUG") }, { TEXT("BTN_Help"), TEXT("HELP") },
                { TEXT("BTN_ReturnToGame"), TEXT("OVERVIEW (TAB)") }
            }
        };
        const float RowY[] = { -132.0f, -82.0f, -32.0f };
        constexpr float Spacing = 184.0f;
        for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
        {
            const float StartX = -Spacing * (Rows[RowIndex].Num() - 1) * 0.5f;
            for (int32 Column = 0; Column < Rows[RowIndex].Num(); ++Column)
            {
                UButton* Button = MakeButton(*Blueprint, Tree,
                    Rows[RowIndex][Column].Key, Rows[RowIndex][Column].Value);
                Place(*Root, *Button, FVector2D(0.5f, 1.0f),
                    FVector2D(StartX + Column * Spacing, RowY[RowIndex]), FVector2D(170.0f, 40.0f),
                    FVector2D(0.5f, 1.0f), 4);
            }
        }

        if (!SaveBlueprint(*Blueprint))
        {
            UE_LOG(LogFFZoneHudBuilder, Error, TEXT("FF_ZONE_HUD_BUILDER_FAIL: save failed."));
            return;
        }
        UE_LOG(LogFFZoneHudBuilder, Display,
            TEXT("FF_ZONE_HUD_BUILDER_PASS: created editable %s with 17 named action buttons."), *HudPackageName);
    }

    // 에디터 콘솔과 -ExecCmds 양쪽에서 같은 비파괴 생성 함수를 호출한다.
    FAutoConsoleCommand BuildHudCommand(
        TEXT("FF.BuildZonePlaytestHUD"),
        TEXT("Creates WBP_ZoneSystemPlaytestHUD only when it does not already exist."),
        FConsoleCommandDelegate::CreateStatic(&BuildZoneSystemPlaytestHud));
}

#endif


