using System.IO;
using UnrealBuildTool;

public class FF_MapOverview : ModuleRules
{
    public FF_MapOverview(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "FF_ZoneSystem", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore", "RenderCore", "RHI", "GeometryCore", "ImageCore" });
        // Only a new, engine-only modular-feature contract. No dependency on project game symbols.
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory,
            "../../../../Source/FestaFiesta/Public/MapOverview")));
    }
}
