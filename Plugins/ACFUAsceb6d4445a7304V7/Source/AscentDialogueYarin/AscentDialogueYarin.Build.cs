// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using UnrealBuildTool;

/**
 * Pays for dialogue voice generation from the signed-in Yarin credit. Editor only.
 *
 * It is a module of its own, not a part of AscentDialogueSystemEditor, on purpose: the dialogue system is also sold
 * as a plugin that carries only its own four modules, and a dependency from the dialogue editor to AscentAITools would
 * stop that plugin from compiling. This module depends on the dialogue editor, never the other way round; it plugs in
 * through IADSVoicePayer and the shared generation section registry.
 */
public class AscentDialogueYarin : ModuleRules
{
	public AscentDialogueYarin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AscentDialogueSystem",
				"AscentDialogueSystemEditor",
				"AscentAITools",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"InputCore",
				"HTTP",
				"Json"
			}
		);
	}
}
