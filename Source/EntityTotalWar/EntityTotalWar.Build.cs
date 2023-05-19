// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class EntityTotalWar : ModuleRules
{
	public EntityTotalWar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore"});

		PrivateDependencyModuleNames.AddRange(new string[] {"MassSpawner"});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"MassEntity",
				"StructUtils",
				"MassCommon",
				"MassMovement",
				"MassActors",
				"MassSpawner",
				"MassGameplayDebug",
				"MassSignals",
				"MassSimulation",
				"MassCrowd",
				"MassActors",
				"MassSpawner",
				"MassRepresentation",
				"MassReplication",
				"MassNavigation",
				"MassEntityDebugger",
				
				//needed for replication setup
				"NetCore",
				"AIModule",

				"ZoneGraph",
				"MassGameplayDebug",
				"MassZoneGraphNavigation", 
				//"Niagara",
				"DeveloperSettings",
				"GeometryCore",
				"MassAIBehavior",
				"StateTreeModule",
				"MassLOD",
				"NavigationSystem",
				//todo: maybe do thee editor only stuff on another module?
				
			}
		);
		
		PrivateIncludePaths.Add("EntityTotalWar/");
		PrivateIncludePaths.Add("EntityTotalWar/Mass/");
	}
}
