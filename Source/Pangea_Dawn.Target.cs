using UnrealBuildTool;
using System.Collections.Generic;

public class Pangea_DawnTarget : TargetRules
{
	public Pangea_DawnTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.Add("Pangea_Dawn");
		RegisterModulesCreatedByRider();
	}

	private void RegisterModulesCreatedByRider()
	{
		ExtraModuleNames.AddRange(new string[]
		{
			"PangeaBaseUpgradeSystem",
			"PangeaCore"
		});
	}
}