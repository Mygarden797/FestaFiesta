// FF_ZoneAuthoringEditorModule.cpp
// 역할: 에디터 전용 기능 등록(FF Authored Zone 디테일 패널 정리). 모듈 종료 시 해제한다.
#include "FFZAAuthoredZoneCustomization.h"
#include "FFZAAuthoredZone.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

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
    }

    virtual void ShutdownModule() override
    {
        if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        {
            FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
            PropertyEditor.UnregisterCustomClassLayout(AFFZAAuthoredZone::StaticClass()->GetFName());
            PropertyEditor.NotifyCustomizationModuleChanged();
        }
    }
};

IMPLEMENT_MODULE(FFFZoneAuthoringEditorModule, FF_ZoneAuthoringEditor)
