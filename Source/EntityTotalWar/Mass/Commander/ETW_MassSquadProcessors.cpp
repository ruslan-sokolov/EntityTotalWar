// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadProcessors.h"
#include "ETW_MassSquadSubsystem.h"
#include "MassCommanderComponent.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "VisualLogger/VisualLogger.h"

struct FMassNetworkIDFragment;

namespace UE::Mass::Squad
{
#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR

	bool bDebugSquadUnits_Client = false;
	bool bDebugSquadUnits_Server = false;
	bool bDebugSquads_Client = false;
	bool bDebugSquads_Server = false;
	FAutoConsoleVariableRef CVarbDebugSquadUnits_Client(TEXT("etw.debug.SquadUnits.Client"), bDebugSquadUnits_Client, TEXT("Debug squad units client"), ECVF_Cheat);
	FAutoConsoleVariableRef CVarbDebugSquadUnits_Server(TEXT("etw.debug.SquadUnits.Server"), bDebugSquadUnits_Server, TEXT("Debug squad units server"), ECVF_Cheat);
	FAutoConsoleVariableRef CVarbDebugSquads_Client(TEXT("etw.debug.Squads.Client"), bDebugSquads_Client, TEXT("Debug squads client"), ECVF_Cheat);
	FAutoConsoleVariableRef CVarbDebugSquads_Server(TEXT("etw.debug.Squads.Server"), bDebugSquads_Server, TEXT("Debug squads server"), ECVF_Cheat);

	// @todo provide a better way of selecting agents to debug
	constexpr int32 MaxAgentsDraw = 30;

	void DebugDrawSquadUnits(FMassEntityHandle Entity, const FMassEntityManager& EntityManager, const float Radius = 50.f)
	{
		const UWorld* World = EntityManager.GetWorld();
		if (World == nullptr)
		{
			return;
		}
		
		static const FVector DebugCylinderHeight = FVector(0.f, 0.f, 200.f);

		const FMassEntityView EntityView(EntityManager, Entity);

		const FTransformFragment& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
		const FMassNetworkIDFragment* NetworkIDFragment = EntityView.GetFragmentDataPtr<FMassNetworkIDFragment>();
		
		const FMassTargetLocationFragment& TargetLocationFragment = EntityView.GetFragmentData<FMassTargetLocationFragment>();
		const FETW_MassUnitFragment& UnitFragment = EntityView.GetFragmentData<FETW_MassUnitFragment>();
		const FETW_MassTeamFragment& TeamFragment = EntityView.GetFragmentData<FETW_MassTeamFragment>();
		const FETW_MassSquadSharedFragment& SquadSharedFragment = EntityView.GetSharedFragmentData<FETW_MassSquadSharedFragment>();


		const FVector& Pos = TransformFragment.GetTransform().GetLocation();
		const uint32 NetworkID = NetworkIDFragment ? NetworkIDFragment->NetID.GetValue() : -1;

		// Multiply by a largeish number that is not a multiple of 256 to separate out the color shades a bit
		const uint32 InitialColor = NetworkID == -1 ? EntityView.GetEntity().SerialNumber * 20001 : NetworkID * 20001;

		const uint8 NetworkIdMod3 = NetworkID % 3;
		FColor DebugColor;

		// Make a deterministic color from the Mod by 3 to vary how we create it
		if (NetworkIdMod3 == 0)
		{
			DebugColor = FColor(InitialColor % 256, InitialColor / 256 % 256, InitialColor / 256 / 256 % 256);
		}
		else if (NetworkIdMod3 == 1)
		{
			DebugColor = FColor(InitialColor / 256 / 256 % 256, InitialColor % 256, InitialColor / 256 % 256);
		}
		else
		{
			DebugColor = FColor(InitialColor / 256 % 256, InitialColor / 256 / 256 % 256, InitialColor % 256);
		}

		const FString TargetLocation = TargetLocationFragment.Target.ToString();
		
		FString DbgString = FString::Printf(TEXT("NetId: %d \n Team %d: \t Unit %d, \t Squad %d: \t TargetSquad: %d \n Target Location: %s"),
			NetworkID, TeamFragment.TeamIndex, UnitFragment.UnitIndex, SquadSharedFragment.SquadIndex, SquadSharedFragment.TargetSquadIndex, *TargetLocation);
		
		if ((World->GetNetMode() == NM_Client && UE::Mass::Squad::bDebugSquadUnits_Client) || (World->GetNetMode() == NM_Standalone && (UE::Mass::Squad::bDebugSquadUnits_Client || UE::Mass::Squad::bDebugSquadUnits_Server)))
		{
			DrawDebugString(World, Pos, DbgString, nullptr, DebugColor, World->GetDeltaSeconds(), true);
		}

		if ((World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_ListenServer) && UE::Mass::Squad::bDebugSquads_Server)
		{
			DrawDebugString(World, Pos + DebugCylinderHeight, DbgString, nullptr, DebugColor, World->GetDeltaSeconds(), true);
		}
	}

	void DebugDrawSquads(FMassEntityHandle Entity, const FMassEntityManager& EntityManager, const float Radius = 50.f)
	{
		const UWorld* World = EntityManager.GetWorld();
		if (World == nullptr)
		{
			return;
		}
		
		static const FVector DebugCylinderHeight = FVector(0.f, 0.f, 200.f);

		const FMassEntityView EntityView(EntityManager, Entity);

		const FTransformFragment& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
		const FETW_MassTeamFragment& TeamFragment = EntityView.GetFragmentData<FETW_MassTeamFragment>();
		const FMassNetworkIDFragment& NetworkIDFragment = EntityView.GetFragmentData<FMassNetworkIDFragment>();
		const FMassTargetLocationFragment& TargetLocationFragment = EntityView.GetFragmentData<FMassTargetLocationFragment>();
		const FETW_MassSquadCommanderFragment& CommanderFragment = EntityView.GetFragmentData<FETW_MassSquadCommanderFragment>();
		const FETW_MassSquadSharedFragment& SquadSharedFragment = EntityView.GetSharedFragmentData<FETW_MassSquadSharedFragment>();

		const FVector& Pos = TransformFragment.GetTransform().GetLocation();
		const uint32 NetworkID = NetworkIDFragment.NetID.GetValue();

		// Multiply by a largeish number that is not a multiple of 256 to separate out the color shades a bit
		const uint32 InitialColor = NetworkID == -1 ? EntityView.GetEntity().SerialNumber * 20001 : NetworkID * 20001;

		const uint8 NetworkIdMod3 = NetworkID % 3;
		FColor DebugColor;

		// Make a deterministic color from the Mod by 3 to vary how we create it
		if (NetworkIdMod3 == 0)
		{
			DebugColor = FColor(InitialColor % 256, InitialColor / 256 % 256, InitialColor / 256 / 256 % 256);
		}
		else if (NetworkIdMod3 == 1)
		{
			DebugColor = FColor(InitialColor / 256 / 256 % 256, InitialColor % 256, InitialColor / 256 % 256);
		}
		else
		{
			DebugColor = FColor(InitialColor / 256 % 256, InitialColor / 256 / 256 % 256, InitialColor % 256);
		}

		const FString CommanderName = CommanderFragment.CommanderComp.Get()->GetOwner()->GetName();
		const FString TargetLocation = TargetLocationFragment.Target.ToString();
		
		FString DbgString = FString::Printf(TEXT("NetId: %d \n Team %d: \t Squad %d: \t TargetSquad: %d \n Commander: %s \n Target Location: %s"),
			NetworkID, TeamFragment.TeamIndex, SquadSharedFragment.SquadIndex, SquadSharedFragment.TargetSquadIndex, *CommanderName, *TargetLocation);
		
		if ((World->GetNetMode() == NM_Client && UE::Mass::Squad::bDebugSquads_Client) || (World->GetNetMode() == NM_Standalone && (UE::Mass::Squad::bDebugSquads_Client || UE::Mass::Squad::bDebugSquads_Server)))
		{
			//DrawDebugCylinder(World, Pos, Pos + 0.5f * DebugCylinderHeight, Radius, /*segments = */24, DebugColor);
			DrawDebugString(World, Pos, DbgString, nullptr, DebugColor, World->GetDeltaSeconds(), true);
		}

		if ((World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_ListenServer) && UE::Mass::Squad::bDebugSquads_Server)
		{
			//DrawDebugCylinder(World, Pos + 0.5f * DebugCylinderHeight, Pos + DebugCylinderHeight, Radius, /*segments = */24, DebugColor);
			DrawDebugString(World, Pos, DbgString, nullptr, DebugColor, World->GetDeltaSeconds(), true);
		}
	}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
}



void UETW_MassSquadTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassTargetLocationFragment>();
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FETW_MassUnitFragment>();  
	BuildContext.AddFragment<FETW_MassTeamFragment>(); 

	UETW_MassSquadSubsystem* SquadSubsystem = UWorld::GetSubsystem<UETW_MassSquadSubsystem>(&World);

	if (SquadSubsystem == nullptr)
	{
		ensureMsgf(false, TEXT("UETW_MassSquadSubsystem subsystem not found in %s"), __FUNCTION__);
		return;
	}
	FMassSquadManager& SquadManager = SquadSubsystem->GetMutablSquadManager();
	
	
	FETW_MassSquadSharedFragment SquadSharedFragment;
	SquadSharedFragment.Formation = Params.DefaultFormation;
	SquadSharedFragment.SquadIndex = SquadManager.GetSquadId();
	
	uint32 SquadSharedFragmentHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(SquadSharedFragment));
	FSharedStruct SquadSharedFragmentStruct = EntityManager.GetOrCreateSharedFragmentByHash<FETW_MassSquadSharedFragment>(SquadSharedFragmentHash, SquadSharedFragment);
	BuildContext.AddSharedFragment(SquadSharedFragmentStruct);
	
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

UETW_MassSquadProcessor::UETW_MassSquadProcessor()
	: EntityQuery_Squad(*this), EntityQuery_Unit(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	//ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void UETW_MassSquadProcessor::ConfigureQueries()
{
	EntityQuery_Squad.AddRequirement<FETW_MassSquadCommanderFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Squad.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Squad.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery_Squad.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Squad.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Squad.AddConstSharedRequirement<FETW_MassSquadParams>();

	EntityQuery_Squad.AddSubsystemRequirement<UETW_MassSquadSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery_Unit.AddRequirement<FETW_MassUnitFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Unit.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery_Unit.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddConstSharedRequirement<FETW_MassSquadParams>();

	EntityQuery_Unit.AddSubsystemRequirement<UETW_MassSquadSubsystem>(EMassFragmentAccess::ReadWrite);

}

void UETW_MassSquadProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	UWorld* World = Owner.GetWorld();
	SquadSubsystem = UWorld::GetSubsystem<UETW_MassSquadSubsystem>(World);

	check(SquadSubsystem);
}

void UETW_MassSquadProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	check(World);
	check(SquadSubsystem);
	FMassSquadManager& SquadManager = SquadSubsystem->GetMutablSquadManager();

	
	// for each shared fragment
	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassSquadProcessor_ForEachSquadSharedFragment);
		EntityManager.ForEachSharedFragment<FETW_MassSquadSharedFragment>([&SquadManager](FETW_MassSquadSharedFragment& RepSharedFragment)
		{
		
		});
	}
	
	// squad logic
	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassSquadProcessor_EntityQuery_Squad);
		EntityQuery_Squad.ForEachEntityChunk(EntityManager, Context, [&SquadManager](FMassExecutionContext& Context)
		{
			const FETW_MassSquadParams& SquadParams = Context.GetConstSharedFragment<FETW_MassSquadParams>();
			FETW_MassSquadSharedFragment& SquadSharedFragment = Context.GetMutableSharedFragment<FETW_MassSquadSharedFragment>();

			const TConstArrayView<FETW_MassSquadCommanderFragment> CommanderFragments = Context.GetFragmentView<FETW_MassSquadCommanderFragment>();
			const TConstArrayView<FETW_MassTeamFragment> TeamFragments = Context.GetFragmentView<FETW_MassTeamFragment>();

			const TArrayView<FMassTargetLocationFragment> TargetLocationFragments = Context.GetMutableFragmentView<FMassTargetLocationFragment>();
			const TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();

			for (int32 EntityIdx = 0; EntityIdx < Context.GetNumEntities(); EntityIdx++)
			{
				// draw debug
				#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
				if (UE::Mass::Squad::bDebugSquads_Client || UE::Mass::Squad::bDebugSquads_Server)
				{
					UE::Mass::Squad::DebugDrawSquads(Context.GetEntity(EntityIdx), Context.GetEntityManagerChecked());
				}
				#endif
			}
			
		});
	}

	// unit logic
	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassSquadProcessor_EntityQuery_Unit);
		EntityQuery_Unit.ForEachEntityChunk(EntityManager, Context, [&SquadManager](FMassExecutionContext& Context)
		{
			
			const FETW_MassSquadParams& SquadParams = Context.GetConstSharedFragment<FETW_MassSquadParams>();
			FETW_MassSquadSharedFragment& SquadSharedFragment = Context.GetMutableSharedFragment<FETW_MassSquadSharedFragment>();
			
			const TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FETW_MassTeamFragment> TeamFragments = Context.GetFragmentView<FETW_MassTeamFragment>();
			const TConstArrayView<FETW_MassUnitFragment> UnitFragments = Context.GetFragmentView<FETW_MassUnitFragment>();

			const TArrayView<FMassTargetLocationFragment> TargetLocationFragments = Context.GetMutableFragmentView<FMassTargetLocationFragment>();
			
			for (int32 EntityIdx = 0; EntityIdx < Context.GetNumEntities(); EntityIdx++)
			{

				// draw debug
				#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
				if (UE::Mass::Squad::bDebugSquadUnits_Client || UE::Mass::Squad::bDebugSquadUnits_Server)
				{
					UE::Mass::Squad::DebugDrawSquadUnits(Context.GetEntity(EntityIdx), Context.GetEntityManagerChecked());
				}
				#endif
			}

		});
	}

}


UMassSquadPostSpawnProcessor::UMassSquadPostSpawnProcessor()
	: EntityQuery_Unit(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void UMassSquadPostSpawnProcessor::ConfigureQueries()
{
	EntityQuery_Unit.AddRequirement<FETW_MassUnitFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Unit.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Unit.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Unit.AddConstSharedRequirement<FETW_MassSquadParams>();

	EntityQuery_Unit.AddSubsystemRequirement<UETW_MassSquadSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassSquadPostSpawnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!ensure(Context.ValidateAuxDataType<FMassSquadSpawnAuxData>()))
	{
		UE_VLOG_UELOG(this, LogMass, Log, TEXT("Execution context has invalid AuxData or it's not FMassSquadSpawnData. Entity transforms won't be initialized."));
		return;
	}

	EntityQuery_Unit.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
	{
		UETW_MassSquadSubsystem& SquadSubsystem = Context.GetMutableSubsystemChecked<UETW_MassSquadSubsystem>();
		FMassSquadManager& SquadManager = SquadSubsystem.GetMutablSquadManager();
		const FMassSquadSpawnAuxData& AuxData = Context.GetAuxData().Get<FMassSquadSpawnAuxData>();
		
		const FETW_MassSquadParams& SquadParams = Context.GetConstSharedFragment<FETW_MassSquadParams>();
		FETW_MassSquadSharedFragment& SquadSharedFragment = Context.GetMutableSharedFragment<FETW_MassSquadSharedFragment>();

		const TArrayView<FETW_MassTeamFragment> TeamFragments = Context.GetMutableFragmentView<FETW_MassTeamFragment>();
		const TArrayView<FETW_MassUnitFragment> UnitFragments = Context.GetMutableFragmentView<FETW_MassUnitFragment>();

		// initialize unit entities
		const int32 NumEntities = Context.GetNumEntities();
		for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			TeamFragments[EntityIdx].TeamIndex = AuxData.TeamIndex;
			UnitFragments[EntityIdx].UnitIndex = SquadManager.GetUnitId();
		}


		// --- create squad entity for units  //
		FMassEntityManager& EntityManager = Context.GetEntityManagerChecked();

		// fragments
		TArray<FInstancedStruct> FragmentInstanceList;
		FragmentInstanceList.Reserve(4);

		FETW_MassSquadCommanderFragment CommanderFrag;
		CommanderFrag.CommanderComp = AuxData.CommanderComp.Get();
		FragmentInstanceList.Emplace(FInstancedStruct::Make(CommanderFrag));
		
		FMassTargetLocationFragment TargetLocationFragment;
		TargetLocationFragment.Target = AuxData.SquadInitialTransform.GetLocation();
		FragmentInstanceList.Emplace(FInstancedStruct::Make(TargetLocationFragment));

		FTransformFragment TransformFragment;
		TransformFragment.SetTransform(AuxData.SquadInitialTransform);
		FragmentInstanceList.Emplace(FInstancedStruct::Make(TransformFragment));

		FETW_MassTeamFragment TeamFragment;
		TeamFragment.TeamIndex = FMath::Clamp<int8>(AuxData.TeamIndex, INT8_MIN, INT8_MAX);
		FragmentInstanceList.Emplace(FInstancedStruct::Make(TeamFragment));

		// shared fragments
		FMassArchetypeSharedFragmentValues SharedFragments;
		SharedFragments.AddSharedFragment(FSharedStruct::Make(SquadSharedFragment));
		SharedFragments.AddConstSharedFragment(FSharedStruct::Make(SquadParams));
		SharedFragments.Sort();
		
		EntityManager.CreateEntity(FragmentInstanceList, SharedFragments, FName(FString::Printf(TEXT("Squad_%d"), SquadSharedFragment.SquadIndex)));

		// --- END create squad entity for units//
	});
	
}