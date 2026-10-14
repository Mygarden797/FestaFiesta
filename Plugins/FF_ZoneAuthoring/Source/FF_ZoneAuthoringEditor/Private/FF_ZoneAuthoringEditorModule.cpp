// FF_ZoneAuthoringEditorModule.cpp
// 역할: 에디터 전용 기능 등록(FF Authored Zone 디테일 패널 정리). 모듈 종료 시 해제한다.
#include "FFZAAuthoredZoneCustomization.h"
#include "FFZAAuthoredZone.h"
#include "FF_ZoneAuthoringModule.h"
#include "Weld/FFZASelectedWeldTool.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "UObject/UObjectGlobals.h"

class FFFZoneAuthoringEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyEditor.RegisterCustomClassLayout(
            AFFZAAuthoredZone::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FFFZAAuthoredZoneCustomization::MakeInstance));
        PropertyEditor.NotifyCustomizationModuleChanged();

        FFZAGetSelectedWeldRequest().BindStatic(&FFZASelectedWeldTool::Open);
        UndoRedoHandle = FEditorDelegates::PostUndoRedo.AddStatic(&FFZASelectedWeldTool::HandleUndoRedo);
        ObjectTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddStatic(&FFZASelectedWeldTool::HandleObjectTransacted);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FFFZoneAuthoringEditorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        if (UToolMenus* Menus = UToolMenus::TryGet()) { Menus->UnregisterOwner(this); }
        FEditorDelegates::PostUndoRedo.Remove(UndoRedoHandle);
        FCoreUObjectDelegates::OnObjectTransacted.Remove(ObjectTransactedHandle);
        FFZAGetSelectedWeldRequest().Unbind();
        FFZASelectedWeldTool::Shutdown();
        if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        {
            FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
            PropertyEditor.UnregisterCustomClassLayout(AFFZAAuthoredZone::StaticClass()->GetFName());
            PropertyEditor.NotifyCustomizationModuleChanged();
        }
    }

private:
    void RegisterMenus() { FFZASelectedWeldTool::RegisterMenus(this); }
    FDelegateHandle UndoRedoHandle;
    FDelegateHandle ObjectTransactedHandle;
};

IMPLEMENT_MODULE(FFFZoneAuthoringEditorModule, FF_ZoneAuthoringEditor)
