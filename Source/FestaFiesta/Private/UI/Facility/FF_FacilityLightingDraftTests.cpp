// FF_FacilityLightingDraftTests.cpp
// 역할: 조명 임시 편집값의 적용·취소와 화면 종료 시 복구를 검사한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/Facility/FF_FacilityLightingControls.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityColorPicker.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFAuroraColorPickerLifetimeTest,
    "FestaFiesta.UI.FacilityInfo.AuroraColorPickerLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFFAuroraColorPickerLifetimeTest::RunTest(const FString&)
{
    auto* Template=NewObject<UFF_FacilityColorPicker>();
    Template->SetColor(FLinearColor::Green);
    for(int32 Index=0;Index<3;++Index)
    {
        auto* Copy=DuplicateObject<UFF_FacilityColorPicker>(Template,GetTransientPackage());
        if(!TestNotNull(TEXT("Duplicated native leaf"),Copy))return false;
        {
            const TSharedRef<SWidget> Slate=Copy->TakeWidget();
            Copy->SetColor(FLinearColor::Red);
            TestEqual(TEXT("A duplicated WBP leaf builds its own Slate control"),Slate->GetType(),FName(TEXT("SFFFacilityColorPicker")));
            Copy->ReleaseSlateResources(true);
        }
        TestFalse(TEXT("Leaf resources are released with their last external reference"),Copy->GetCachedWidget().IsValid());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFAuroraLightingDraftTest,
    "FestaFiesta.UI.FacilityInfo.AuroraLightingDraft",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFFAuroraLightingDraftTest::RunTest(const FString&)
{
    UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
    auto* Class=Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityLightingControls>(nullptr,
        FFBuildUIAssetPaths::FacilityLightingControlsClass));
    if(!TestNotNull(TEXT("Lighting WBP"),Class)||!TestNotNull(TEXT("Editor world"),World)) return false;
    struct FInitializationScope
    {
        UWidgetBlueprintGeneratedClass& Class; bool Previous;
        explicit FInitializationScope(UWidgetBlueprintGeneratedClass& In):Class(In),Previous(In.bCanCallInitializedWithoutPlayerContext){Class.bCanCallInitializedWithoutPlayerContext=true;}
        ~FInitializationScope(){Class.bCanCallInitializedWithoutPlayerContext=Previous;}
    } Initialization(*Class);
    auto* Controls=CreateWidget<UFF_FacilityLightingControls>(World,Class);
    if(!TestNotNull(TEXT("Runtime controls"),Controls))return false;
    Controls->TakeWidget();
    auto* Apply=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingApply")));
    auto* Cancel=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingCancel")));
    auto* Teal=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingTeal")));
    auto* Reset=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingReset")));
    auto* Slider=Cast<USlider>(Controls->GetWidgetFromName(TEXT("Slider_LightingBrightness")));
    auto* Hex=Cast<UEditableTextBox>(Controls->GetWidgetFromName(TEXT("Input_LightingColor")));
    auto* Picker=Cast<UFF_FacilityColorPicker>(Controls->GetWidgetFromName(TEXT("ColorPicker_Lighting")));
    if(!TestNotNull(TEXT("Apply"),Apply)||!TestNotNull(TEXT("Cancel"),Cancel)||!TestNotNull(TEXT("Teal"),Teal)||
        !TestNotNull(TEXT("Reset"),Reset)||!TestNotNull(TEXT("Slider"),Slider)||!TestNotNull(TEXT("HEX"),Hex)||!TestNotNull(TEXT("Hue/SV picker"),Picker)) return false;
    FFF_FacilityInfoViewData View;
    View.FacilityInstanceId=FGuid::NewGuid(); View.bHasLightingRig=true; View.bCanEditLighting=true;
    View.LightingState.bEnabled=false; View.LightingState.BrightnessMultiplier=1.25f;
    const FGuid OriginalId=View.FacilityInstanceId;
    const FFFZSFacilityLightingState Original=View.LightingState;
    int32 Writes=0,Rollbacks=0;
    FGuid RolledBackId;
    FFFZSFacilityLightingState RolledBackState;
    Controls->OnLightingRequested.AddLambda([&](FGuid Id,const FFFZSFacilityLightingState& State)
    { ++Writes; TestEqual(TEXT("Preview command is identity scoped"),Id,View.FacilityInstanceId); View.LightingState=State; Controls->ApplyViewData(View); });
    Controls->OnLightingRollbackRequested.AddLambda([&](FGuid Id,const FFFZSFacilityLightingState& State)
    { ++Rollbacks; RolledBackId=Id; RolledBackState=State; });
    Controls->OnLightingResetRequested.AddLambda([&](FGuid)
    { View.LightingState=FFFZSFacilityLightingState(); View.LightingState.BrightnessMultiplier=.7f; Controls->ApplyViewData(View); });
    Controls->ApplyViewData(View); Controls->BeginEdit();
    if(auto* Precision=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingPrecision"))))
    {
        UWidget* Basic=Controls->GetWidgetFromName(TEXT("Panel_LightingBasic"));
        UWidget* Fields=Controls->GetWidgetFromName(TEXT("Panel_LightingPrecision"));
        if(!TestNotNull(TEXT("Simple color controls"),Basic)||!TestNotNull(TEXT("Precise channels"),Fields))return false;
        Precision->OnClicked.Broadcast();
        TestEqual(TEXT("Precision opens directly"),Fields->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
        TestEqual(TEXT("Wheel does not overlap precise fields"),Basic->GetVisibility(),ESlateVisibility::Collapsed);
        TestEqual(TEXT("Changing control views does not modify lighting"),Writes,0);
        TestTrue(TEXT("Control navigation preserves the current draft"),Controls->IsEditing());
        Precision->OnClicked.Broadcast();
        TestEqual(TEXT("Simple view returns directly"),Basic->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
    }
    Teal->OnClicked.Broadcast(); Slider->OnValueChanged.Broadcast(2.527f);
    TestTrue(TEXT("Live preview updates selected color"),View.LightingState.bOverrideColor);
    TestEqual(TEXT("Brightness uses five percent steps"),View.LightingState.BrightnessMultiplier,2.55f);
    if(auto* Low=Cast<UButton>(Controls->GetWidgetFromName(TEXT("Button_LightingBrightnessLow"))))
    {
        Low->OnClicked.Broadcast();
        TestEqual(TEXT("Simple brightness preset previews its exact level"),View.LightingState.BrightnessMultiplier,.5f);
        TestTrue(TEXT("Brightness preset stays in the cancellable lighting draft"),Controls->IsEditing());
    }
    Reset->OnClicked.Broadcast(); TestEqual(TEXT("Reset previews authored defaults"),View.LightingState.BrightnessMultiplier,.7f);
    Cancel->OnClicked.Broadcast();
    TestEqual(TEXT("Cancel rolls back once"),Rollbacks,1);
    TestEqual(TEXT("Cancel retains original target"),RolledBackId,OriginalId);
    TestEqual(TEXT("Cancel restores original brightness rather than reset defaults"),RolledBackState.BrightnessMultiplier,Original.BrightnessMultiplier);
    TestEqual(TEXT("Cancel restores original on/off"),RolledBackState.bEnabled,Original.bEnabled);
    TestEqual(TEXT("Cancel restores original color override flag"),RolledBackState.bOverrideColor,Original.bOverrideColor);
    View.LightingState=Original; Controls->ApplyViewData(View); Controls->BeginEdit();
    Teal->OnClicked.Broadcast(); Apply->OnClicked.Broadcast(); Controls->CancelEdit();
    TestEqual(TEXT("Committed preview does not roll back on later close"),Rollbacks,1);
    TestFalse(TEXT("Apply closes draft transaction"),Controls->IsEditing());
    Controls->BeginEdit(); Slider->OnValueChanged.Broadcast(3.f);
    Picker->OnInteractionStarted.Broadcast();
    View.FacilityInstanceId=FGuid::NewGuid(); Controls->ApplyViewData(View);
    TestEqual(TEXT("Selection change cancels old transaction"),Rollbacks,2);
    TestEqual(TEXT("Selection change never rolls back the newly selected facility"),RolledBackId,OriginalId);
    const int32 BeforeStalePicker=Writes;
    Picker->OnColorPicked.Broadcast(FLinearColor::Blue);
    TestEqual(TEXT("Captured color wheel cannot edit a different selection"),Writes,BeforeStalePicker);
    Picker->OnInteractionStarted.Broadcast(); Picker->OnColorPicked.Broadcast(FLinearColor::Green);
    TestEqual(TEXT("Fresh color wheel interaction targets the current facility"),Writes,BeforeStalePicker+1);
    Controls->BeginEdit(); Hex->SetText(FText::FromString(TEXT("#GGFFFF")));
    // SetText updates the property; keyboard editing separately emits this user event.
    Hex->OnTextChanged.Broadcast(Hex->GetText());
    const int32 BeforeInvalid=Writes;
    Apply->OnClicked.Broadcast();
    TestEqual(TEXT("Invalid HEX cannot issue a command"),Writes,BeforeInvalid);
    TestTrue(TEXT("Invalid uncommitted edit keeps the draft open"),Controls->IsEditing());
    Hex->SetText(FText::FromString(TEXT("#56DCE7"))); Hex->OnTextChanged.Broadcast(Hex->GetText()); Apply->OnClicked.Broadcast();
    TestEqual(TEXT("Apply commits a valid pending HEX edit"),View.LightingState.Color.ToFColorSRGB(),FColor(0x56,0xDC,0xE7));
    TestFalse(TEXT("Valid pending edit can be applied"),Controls->IsEditing());
    Controls->OnLightingRequested.Clear(); Controls->OnLightingRollbackRequested.Clear(); Controls->OnLightingResetRequested.Clear();
    Controls->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFAuroraLightingPanelTest,
    "FestaFiesta.UI.FacilityInfo.AuroraLightingNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFFAuroraLightingPanelTest::RunTest(const FString&)
{
    UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
    auto* Class=Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_FacilityInfoPanel>(nullptr,
        FFBuildUIAssetPaths::FacilityInfoPanelClass));
    if(!TestNotNull(TEXT("Inspector WBP"),Class)||!TestNotNull(TEXT("Editor world"),World))return false;
    const bool Previous=Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext=true;
    auto* Panel=CreateWidget<UFF_FacilityInfoPanel>(World,Class);
    Class->bCanCallInitializedWithoutPlayerContext=Previous;
    if(!TestNotNull(TEXT("Inspector"),Panel))return false;
    Panel->TakeWidget();
    auto* Tab=Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_TabLighting")));
    auto* Fold=Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_LightingMinimize")));
    UWidget* Compact=Panel->GetWidgetFromName(TEXT("Panel_LightingCompact"));
    UWidget* Body=Panel->GetWidgetFromName(TEXT("Body_Surface"));
    auto* Size=Cast<USizeBox>(Panel->GetWidgetFromName(TEXT("Panel_Size")));
    if(!TestNotNull(TEXT("Integrated lighting tab"),Tab)||!TestNotNull(TEXT("Fold control"),Fold)||
        !TestNotNull(TEXT("Compact summary"),Compact)||!TestNotNull(TEXT("Details surface"),Body)||!TestNotNull(TEXT("Panel size"),Size))return false;
    FFF_FacilityInfoViewData View;
    View.FacilityInstanceId=FGuid::NewGuid(); View.Context=EFF_FacilityInfoContext::Live;
    View.bHasLightingRig=true; View.bCanEditLighting=true;
    Panel->ApplyViewData(View); Panel->SetPanelOpen(true); Tab->OnClicked.Broadcast();
    TestTrue(TEXT("Active lighting selection keeps enabled text contrast"),Tab->GetIsEnabled());
    const bool bClarityV2=Panel->GetWidgetFromName(TEXT("ClarityV2_Info"))!=nullptr;
    TestEqual(TEXT("Lighting uses authored compact width"),Size->GetWidthOverride(),bClarityV2?480.f:690.f);
    if(bClarityV2)
    {
        TestEqual(TEXT("Lighting keeps the regular inspector height"),Size->GetHeightOverride(),620.f);
        TestEqual(TEXT("Lighting gives its controls room without stacked navigation"),
            Panel->GetWidgetFromName(TEXT("Panel_InspectorNavigation"))->GetVisibility(),ESlateVisibility::Collapsed);
    }
    Fold->OnClicked.Broadcast();
    TestEqual(TEXT("Observe folds detail controls"),Body->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("Observe retains facility lighting summary"),Compact->GetVisibility(),ESlateVisibility::Visible);
    Fold->OnClicked.Broadcast();
    TestEqual(TEXT("Unfold restores detail controls"),Body->GetVisibility(),ESlateVisibility::Visible);
    Panel->SetInfoPage(EFF_FacilityInfoPage::Operation);
    if(bClarityV2)TestEqual(TEXT("Leaving lighting restores direct section tabs"),
        Panel->GetWidgetFromName(TEXT("Panel_InspectorNavigation"))->GetVisibility(),ESlateVisibility::SelfHitTestInvisible);
    TestEqual(TEXT("Leaving lighting restores regular inspector width"),Size->GetWidthOverride(),
        Panel->GetWidgetFromName(TEXT("Clarity_InfoV1"))?456.f:592.f);
    View.bHasLightingRig=false; View.bCanEditLighting=false; Panel->ApplyViewData(View);
    TestEqual(TEXT("Facilities without lights hide lighting tab"),Tab->GetVisibility(),ESlateVisibility::Collapsed);
    Panel->ReleaseSlateResources(true); return true;
}
#endif
