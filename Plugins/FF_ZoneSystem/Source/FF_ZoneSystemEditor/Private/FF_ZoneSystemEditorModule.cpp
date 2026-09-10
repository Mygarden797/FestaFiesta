#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSAudienceCapacityEditor.h"
#include "FF_FFZSFacilityDefinitionCustomization.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

class FFFZoneSystemEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FFFZSAudienceCapacityEditor::Register();
        FPropertyEditorModule& PropertyEditorModule =
            FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyEditorModule.RegisterCustomClassLayout(
            UFFZSFacilityDefinition::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(
                &FFFZSFacilityDefinitionCustomization::MakeInstance));
        PropertyEditorModule.NotifyCustomizationModuleChanged();
    }

    virtual void ShutdownModule() override
    {
        FFFZSAudienceCapacityEditor::Unregister();
        if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
        {
            FPropertyEditorModule& PropertyEditorModule =
                FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
            PropertyEditorModule.UnregisterCustomClassLayout(
                UFFZSFacilityDefinition::StaticClass()->GetFName());
            PropertyEditorModule.NotifyCustomizationModuleChanged();
        }
    }
};

IMPLEMENT_MODULE(FFFZoneSystemEditorModule, FF_ZoneSystemEditor)
