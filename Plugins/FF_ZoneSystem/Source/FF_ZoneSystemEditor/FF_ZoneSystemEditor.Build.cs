using UnrealBuildTool;

public class FF_ZoneSystemEditor : ModuleRules
{
    public FF_ZoneSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "PropertyEditor",
            "AssetRegistry",
            "DataValidation",
            "FF_ZoneSystem"
        });
    }
}
