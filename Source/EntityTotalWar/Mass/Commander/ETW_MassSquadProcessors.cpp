// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadProcessors.h"
#include "ETW_MassSquadSubsystem.h"

#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

void UETW_MassSquadTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassTargetLocationFragment>();
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

	EntityQuery_Unit.AddRequirement<FETW_MassUnitFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery_Unit.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Unit.AddConstSharedRequirement<FETW_MassSquadParams>();
}

void UETW_MassSquadProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// for each shared fragment

	// squad logic

	// unit logic

	// draw debug
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
	FMassSquadSpawnAuxData& AuxData = Context.GetMutableAuxData().GetMutable<FMassSquadSpawnAuxData>();

	UETW_MassSquadSubsystem& SquadSubsystem = Context.GetMutableSubsystemChecked<UETW_MassSquadSubsystem>();
	FMassSquadManager& SquadManager = SquadSubsystem.GetMutablSquadManager();
	
	EntityQuery_Unit.ForEachEntityChunk(EntityManager, Context, [&AuxData, &SquadManager](FMassExecutionContext& Context)
	{
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
		FInstancedStruct& ListAddedInstancedStructRef = FragmentInstanceList.AddZeroed_GetRef();
		ListAddedInstancedStructRef.InitializeAs<FETW_MassSquadCommanderFragment>(CommanderFrag);
		
		FMassTargetLocationFragment TargetLocationFragment;
		TargetLocationFragment.Target = AuxData.SquadInitialTransform.GetLocation();
		ListAddedInstancedStructRef = FragmentInstanceList.AddZeroed_GetRef();
		ListAddedInstancedStructRef.InitializeAs<FMassTargetLocationFragment>(TargetLocationFragment);

		FTransformFragment TransformFragment;
		TransformFragment.SetTransform(AuxData.SquadInitialTransform);
		ListAddedInstancedStructRef = FragmentInstanceList.AddZeroed_GetRef();
		ListAddedInstancedStructRef.InitializeAs<FTransformFragment>(TransformFragment);

		FETW_MassTeamFragment TeamFragment;
		TeamFragment.TeamIndex = FMath::Clamp<int8>(AuxData.TeamIndex, INT8_MIN, INT8_MAX);
		ListAddedInstancedStructRef = FragmentInstanceList.AddZeroed_GetRef();
		ListAddedInstancedStructRef.InitializeAs<FETW_MassTeamFragment>(TeamFragment);

		// shared fragments
		FMassArchetypeSharedFragmentValues SharedFragments;
		SharedFragments.AddSharedFragment(FSharedStruct::Make(SquadSharedFragment));
		SharedFragments.AddConstSharedFragment(FSharedStruct::Make(SquadParams));

		EntityManager.CreateEntity(FragmentInstanceList, SharedFragments, FName(FString::Printf(TEXT("Squad_%d"), SquadSharedFragment.SquadIndex)));

		// --- END create squad entity for units//
	});
	
}