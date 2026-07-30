using UnrealBuildTool;

public class UrbanModes : ModuleRules
{
    public UrbanModes(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}