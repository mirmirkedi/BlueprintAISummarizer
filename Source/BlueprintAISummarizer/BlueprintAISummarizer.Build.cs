using UnrealBuildTool;

public class BlueprintAISummarizer : ModuleRules
{
	public BlueprintAISummarizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"GraphEditor",
				"UnrealEd",
				"Kismet"
			}
		);
	}
}
