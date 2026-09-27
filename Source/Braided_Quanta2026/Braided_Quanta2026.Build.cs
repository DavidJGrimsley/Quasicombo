// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Braided_Quanta2026 : ModuleRules
{
	public Braided_Quanta2026(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"QuantumApi"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Braided_Quanta2026",
			"Braided_Quanta2026/Variant_Platforming",
			"Braided_Quanta2026/Variant_Platforming/Animation",
			"Braided_Quanta2026/Variant_Combat",
			"Braided_Quanta2026/Variant_Combat/AI",
			"Braided_Quanta2026/Variant_Combat/Animation",
			"Braided_Quanta2026/Variant_Combat/Gameplay",
			"Braided_Quanta2026/Variant_Combat/Interfaces",
			"Braided_Quanta2026/Variant_Combat/UI",
			"Braided_Quanta2026/Variant_SideScrolling",
			"Braided_Quanta2026/Variant_SideScrolling/AI",
			"Braided_Quanta2026/Variant_SideScrolling/Gameplay",
			"Braided_Quanta2026/Variant_SideScrolling/Interfaces",
			"Braided_Quanta2026/Variant_SideScrolling/UI",
			"Braided_Quanta2026/Quasicombo/Route",
			"Braided_Quanta2026/Quasicombo/Quantum",
			"Braided_Quanta2026/Quasicombo/Boss",
			"Braided_Quanta2026/Quasicombo/QTE",
			"Braided_Quanta2026/Quasicombo/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
