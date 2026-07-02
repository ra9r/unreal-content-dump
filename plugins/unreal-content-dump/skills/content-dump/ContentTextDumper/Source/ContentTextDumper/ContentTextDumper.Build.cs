using UnrealBuildTool;

public class ContentTextDumper : ModuleRules
{
	public ContentTextDumper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",        // UCommandlet, FEdGraphUtilities::ExportNodesToText
			"AssetRegistry",   // enumerate assets without loading everything
			"BlueprintGraph",  // K2 node types referenced by graph export
		});
	}
}
