using UnrealBuildTool;

public class FF_MapOverviewEditor : ModuleRules
{
    public FF_MapOverviewEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "FF_MapOverview", "UMG", "UMGEditor", "UnrealEd",
            "AssetTools", "AssetRegistry", "Slate", "SlateCore", "RenderCore"
        });
    }
}
