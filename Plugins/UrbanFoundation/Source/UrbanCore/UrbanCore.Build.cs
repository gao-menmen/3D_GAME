using UnrealBuildTool;

public class UrbanCore : ModuleRules
{
    public UrbanCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "ModularGameplay",
            "LyraGame",
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "GameplayAbilities",
            "NetCore",
            "EnhancedInput",
        });
    }
}
