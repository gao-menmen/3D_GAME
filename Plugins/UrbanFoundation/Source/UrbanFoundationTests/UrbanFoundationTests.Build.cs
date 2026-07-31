using UnrealBuildTool;

public class UrbanFoundationTests : ModuleRules
{
    public UrbanFoundationTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayAbilities",
            "GameplayTags",
            "LyraGame",
            "UrbanCore",
            "UrbanCombat",
            "UrbanAI",
            "UrbanMission",
            "UrbanModes",
            "UrbanUI",
            "UrbanOnline",
        });
    }
}