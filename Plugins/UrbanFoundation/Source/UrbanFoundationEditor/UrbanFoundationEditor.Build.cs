using UnrealBuildTool;

public class UrbanFoundationEditor : ModuleRules
{
    public UrbanFoundationEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd",
        });
    }
}
