using UnrealBuildTool;

public class UrbanMission : ModuleRules
{
    public UrbanMission(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}