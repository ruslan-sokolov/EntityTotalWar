// Fill out your copyright notice in the Description page of Project Settings.

#include "ETW_MassSquadUnitsReplicationHandlers.h"
#include "ETW_MassSquadUnitsBubble.h"

#include "Commander/ETW_MassSquadSubsystem.h"
#include "VisualLogger/VisualLogger.h"

FETW_ReplicatedSquadUnitAgentData::FETW_ReplicatedSquadUnitAgentData(const FETW_MassUnitFragment& UnitFragment,
                                                                     const FETW_MassTeamFragment& TeamFragment,
                                                                     const FMassTargetLocationFragment& TargetLocationFragment
                                                                     )
		: TargetLocation(TargetLocationFragment.Target),
		UnitIndex(UnitFragment.UnitIndex),
		TeamIndex(TeamFragment.TeamIndex)
{
}

void FETW_ReplicatedSquadUnitAgentData::InitEntity(const UWorld& InWorld, const FMassEntityView& InEntityView,
	FETW_MassUnitFragment& OutUnitFragment, FETW_MassTeamFragment& OutTeamFragment,
	FMassTargetLocationFragment& OutTargetLocationFragment) const
{
	// setup fragments
	OutUnitFragment.UnitIndex = UnitIndex;
	OutTeamFragment.TeamIndex = TeamIndex;
	OutTargetLocationFragment.Target = TargetLocation;

	ApplyToEntity(InWorld, InEntityView);
}

void FETW_ReplicatedSquadUnitAgentData::ApplyToEntity(const UWorld& InWorld, const FMassEntityView& InEntityView) const
{
	const FMassEntityHandle Entity = InEntityView.GetEntity();

	FETW_MassUnitFragment& UnitFragment = InEntityView.GetFragmentData<FETW_MassUnitFragment>();
	UnitFragment.UnitIndex = UnitIndex;
	
	FETW_MassTeamFragment& TeamFragment = InEntityView.GetFragmentData<FETW_MassTeamFragment>();
	TeamFragment.TeamIndex = TeamIndex;
	
	FMassTargetLocationFragment& TargetLocationFragment = InEntityView.GetFragmentData<FMassTargetLocationFragment>();
	TargetLocationFragment.Target = TargetLocation;
}


#if UE_REPLICATION_COMPILE_SERVER_CODE
void FETW_MassClientBubbleSquadUnitHandler::SetBubbleSquadUnitData(const FMassReplicatedAgentHandle Handle,
	const FETW_MassUnitFragment& UnitFragment,
	const FETW_MassTeamFragment& TeamFragment,
	const FMassTargetLocationFragment& TargetLocationFragment)
{
	check(OwnerHandler.GetAgentHandleManager().IsValidHandle(Handle));

	const int32 AgentsIdx = OwnerHandler.GetAgentLookuoArray()[Handle.GetIndex()].AgentsIdx;
	FETW_MassSquadUnitsFastArrayItem& Item = (*OwnerHandler.GetAgents())[AgentsIdx];

	checkf(Item.Agent.GetNetID().IsValid(), TEXT("Pos should not be updated on FCrowdFastArrayItem's that have an Invalid ID! First Add the Agent!"));

	bool bMarkDirty = false;
	
	// GetReplicatedSquadUnitDataMutable() must be defined in your FReplicatedAgentBase derived class
	FETW_ReplicatedSquadUnitAgentData& ReplicatedSquadUnit = Item.Agent.GetReplicatedSquadUnitDataMutable();

	if (!TargetLocationFragment.Target.Equals(ReplicatedSquadUnit.TargetLocation, UE::Mass::Replication::PositionReplicateTolerance))
	{
		ReplicatedSquadUnit.TargetLocation = TargetLocationFragment.Target;
		bMarkDirty = true;
	}

	if (UnitFragment.UnitIndex != ReplicatedSquadUnit.UnitIndex)
	{
		ReplicatedSquadUnit.UnitIndex = UnitFragment.UnitIndex;
		bMarkDirty = true;
	}

	if (TeamFragment.TeamIndex != ReplicatedSquadUnit.TeamIndex)
	{
		ReplicatedSquadUnit.TeamIndex = TeamFragment.TeamIndex;
		bMarkDirty = true;
	}

	if (bMarkDirty)
	{
		OwnerHandler.GetSerializer()->MarkItemDirty(Item);
	}
}
#endif //UE_REPLICATION_COMPILE_SERVER_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadUnitHandler::AddRequirementsForSpawnQuery(FMassEntityQuery& InQuery)
{
	InQuery.AddRequirement<FETW_MassUnitFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadUnitHandler::CacheFragmentViewsForSpawnQuery(FMassExecutionContext& InExecContext)
{
	UnitFragmentList = InExecContext.GetMutableFragmentView<FETW_MassUnitFragment>();
	TeamFragmentList = InExecContext.GetMutableFragmentView<FETW_MassTeamFragment>();
	TargetFragmentList = InExecContext.GetMutableFragmentView<FMassTargetLocationFragment>();
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadUnitHandler::ClearFragmentViewsForSpawnQuery()
{
	UnitFragmentList = TArrayView<FETW_MassUnitFragment>();
	TeamFragmentList = TArrayView<FETW_MassTeamFragment>();
	TargetFragmentList = TArrayView<FMassTargetLocationFragment>();
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadUnitHandler::SetSpawnedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadUnitAgentData& ReplicatedUnitAgentData, const int32 EntityIdx) const
{
	UWorld* World = OwnerHandler.GetSerializer()->GetWorld();
	check(World);
	ReplicatedUnitAgentData.InitEntity(
		*World, EntityView, UnitFragmentList[EntityIdx], TeamFragmentList[EntityIdx], TargetFragmentList[EntityIdx]);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadUnitHandler::SetModifiedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadUnitAgentData& ReplicatedUnitAgentData) const
{
	UWorld* World = OwnerHandler.GetSerializer()->GetWorld();
	check(World);
	ReplicatedUnitAgentData.ApplyToEntity(*World, EntityView);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE


void FMassReplicationProcessorSquadUnitHandler::AddRequirements(FMassEntityQuery& InQuery)
{
	InQuery.AddRequirement<FETW_MassUnitFragment>(EMassFragmentAccess::ReadOnly);
	InQuery.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadOnly);
	InQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadOnly);
}

void FMassReplicationProcessorSquadUnitHandler::CacheFragmentViews(FMassExecutionContext& ExecContext)
{
	UnitFragmentList = ExecContext.GetMutableFragmentView<FETW_MassUnitFragment>();
	TeamFragmentList = ExecContext.GetMutableFragmentView<FETW_MassTeamFragment>();
	TargetFragmentList = ExecContext.GetMutableFragmentView<FMassTargetLocationFragment>();
}

void FMassReplicationProcessorSquadUnitHandler::AddEntity(const int32 EntityIdx,
	FETW_ReplicatedSquadUnitAgentData& InOutReplicatedSquadUnitData) const
{
	const FETW_MassUnitFragment& UnitFragment = UnitFragmentList[EntityIdx];
	const FETW_MassTeamFragment& TeamFragment = TeamFragmentList[EntityIdx];
	const FMassTargetLocationFragment& TargetLocationFragment = TargetFragmentList[EntityIdx];

	InOutReplicatedSquadUnitData = FETW_ReplicatedSquadUnitAgentData(UnitFragment, TeamFragment, TargetLocationFragment);
}

void FMassReplicationProcessorSquadUnitHandler::ModifyEntity(const FMassReplicatedAgentHandle Handle, const int32 EntityIdx, FETW_MassClientBubbleSquadUnitHandler& BubbleSquadUnitHandler, bool bLastClient)
{
	const FETW_MassUnitFragment& UnitFragment = UnitFragmentList[EntityIdx];
	const FETW_MassTeamFragment& TeamFragment= TeamFragmentList[EntityIdx];
	const FMassTargetLocationFragment& TargetLocationFragment = TargetFragmentList[EntityIdx];

	BubbleSquadUnitHandler.SetBubbleSquadUnitData(Handle, UnitFragment, TeamFragment, TargetLocationFragment);
}