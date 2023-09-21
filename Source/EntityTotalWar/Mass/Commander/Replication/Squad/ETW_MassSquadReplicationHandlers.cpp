// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadReplicationHandlers.h"
#include "ETW_MassSquadBubble.h"

#include "Net/UnrealNetwork.h"
#include "ETW_MassSquadReplicatedAgent.h"

FETW_ReplicatedSquadAgentData::FETW_ReplicatedSquadAgentData(
	const FETW_MassSquadCommanderFragment& SquadCommanderFragment, const FETW_MassTeamFragment& TeamFragment,
	const FMassTargetLocationFragment& TargetLocationFragment, const FETW_MassSquadSharedFragment& SquadSharedFragment)
		: CommanderComp(SquadCommanderFragment.CommanderComp),
		TeamIndex(TeamFragment.TeamIndex),
		TargetLocation(TargetLocationFragment.Target),
		SquadIndex(SquadSharedFragment.SquadIndex),
		TargetSquadIndex(SquadSharedFragment.TargetSquadIndex),
		Formation(SquadSharedFragment.Formation)
{
}

void FETW_ReplicatedSquadAgentData::InitEntity(const UWorld& InWorld, const FMassEntityView& InEntityView,
	FETW_MassSquadCommanderFragment& OutSquadCommanderFragment, FETW_MassTeamFragment& OutTeamFragment,
	FMassTargetLocationFragment& OutTargetLocationFragment, FETW_MassSquadSharedFragment& OutSquadSharedFragment) const
{
	OutSquadCommanderFragment.CommanderComp = CommanderComp;
	OutTeamFragment.TeamIndex = TeamIndex;
	OutTargetLocationFragment.Target = TargetLocation;
	OutSquadSharedFragment.SquadIndex = SquadIndex;
	OutSquadSharedFragment.TargetSquadIndex = TargetSquadIndex;
	OutSquadSharedFragment.Formation = Formation;
}

void FETW_ReplicatedSquadAgentData::ApplyToEntity(const UWorld& InWorld, const FMassEntityView& InEntityView) const
{
	FETW_MassSquadCommanderFragment& CommanderFragment = InEntityView.GetFragmentData<FETW_MassSquadCommanderFragment>();
	CommanderFragment.CommanderComp = CommanderComp;

	FETW_MassTeamFragment& TeamFragment = InEntityView.GetFragmentData<FETW_MassTeamFragment>();
	TeamFragment.TeamIndex = TeamIndex;
	
	FMassTargetLocationFragment& TargetLocationFragment = InEntityView.GetFragmentData<FMassTargetLocationFragment>();
	TargetLocationFragment.Target = TargetLocation;

	FETW_MassSquadSharedFragment& SquadSharedFragment = InEntityView.GetSharedFragmentData<FETW_MassSquadSharedFragment>();
	SquadSharedFragment.Formation = Formation;
	SquadSharedFragment.SquadIndex = SquadIndex;
	SquadSharedFragment.TargetSquadIndex = TargetSquadIndex;
}

#if UE_REPLICATION_COMPILE_SERVER_CODE
void FETW_MassClientBubbleSquadHandler::SetBubbleSquadData(const FMassReplicatedAgentHandle Handle,
	const FETW_MassSquadCommanderFragment& SquadCommanderFragment, const FETW_MassTeamFragment& TeamFragment,
	const FMassTargetLocationFragment& TargetLocationFragment, const FETW_MassSquadSharedFragment& SquadSharedFragment)
{
	check(OwnerHandler.GetAgentHandleManager().IsValidHandle(Handle));

	const int32 AgentsIdx = OwnerHandler.GetAgentLookuoArray()[Handle.GetIndex()].AgentsIdx;
	FETW_MassSquadsFastArrayItem& Item = (*OwnerHandler.GetAgents())[AgentsIdx];

	checkf(Item.Agent.GetNetID().IsValid(), TEXT("Pos should not be updated on FCrowdFastArrayItem's that have an Invalid ID! First Add the Agent!"));

	bool bMarkDirty = false;
	
	// GetReplicatedSquadUnitDataMutable() must be defined in your FReplicatedAgentBase derived class
	FETW_ReplicatedSquadAgentData& ReplicatedSquadUnit = Item.Agent.GetReplicatedSquadDataMutable();

	if (!TargetLocationFragment.Target.Equals(ReplicatedSquadUnit.TargetLocation, UE::Mass::Replication::PositionReplicateTolerance))
	{
		ReplicatedSquadUnit.TargetLocation = TargetLocationFragment.Target;
		bMarkDirty = true;
	}

	if (SquadCommanderFragment.CommanderComp != ReplicatedSquadUnit.CommanderComp)
	{
		ReplicatedSquadUnit.CommanderComp = SquadCommanderFragment.CommanderComp;
		bMarkDirty = true;
	}

	if (TeamFragment.TeamIndex != ReplicatedSquadUnit.TeamIndex)
	{
		ReplicatedSquadUnit.TeamIndex = TeamFragment.TeamIndex;
		bMarkDirty = true;
	}

	if (SquadSharedFragment.SquadIndex != ReplicatedSquadUnit.SquadIndex)
	{
		ReplicatedSquadUnit.SquadIndex = SquadSharedFragment.SquadIndex;
		bMarkDirty = true;
	}

	if (SquadSharedFragment.TargetSquadIndex != ReplicatedSquadUnit.TargetSquadIndex)
	{
		ReplicatedSquadUnit.TargetSquadIndex = SquadSharedFragment.TargetSquadIndex;
		bMarkDirty = true;
	}

	if (SquadSharedFragment.Formation != ReplicatedSquadUnit.Formation)
	{
		ReplicatedSquadUnit.Formation = SquadSharedFragment.Formation;
		bMarkDirty = true;
	}
	
	if (bMarkDirty)
	{
		OwnerHandler.GetSerializer()->MarkItemDirty(Item);
	}
}
#endif //UE_REPLICATION_COMPILE_SERVER_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
void FETW_MassClientBubbleSquadHandler::AddRequirementsForSpawnQuery(FMassEntityQuery& InQuery)
{
	InQuery.AddRequirement<FETW_MassSquadCommanderFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadWrite);
}

void FETW_MassClientBubbleSquadHandler::CacheFragmentViewsForSpawnQuery(FMassExecutionContext& InExecContext)
{
	CommanderFragmentList = InExecContext.GetMutableFragmentView<FETW_MassSquadCommanderFragment>();
	TeamFragmentList = InExecContext.GetMutableFragmentView<FETW_MassTeamFragment>();
	TargetFragmentList = InExecContext.GetMutableFragmentView<FMassTargetLocationFragment>();
	SquadSharedFragmentList = MakeArrayView<FETW_MassSquadSharedFragment>(&InExecContext.GetMutableSharedFragment<FETW_MassSquadSharedFragment>(), 1);
}

void FETW_MassClientBubbleSquadHandler::ClearFragmentViewsForSpawnQuery()
{
	CommanderFragmentList = TArrayView<FETW_MassSquadCommanderFragment>();
	TeamFragmentList = TArrayView<FETW_MassTeamFragment>();
	TargetFragmentList = TArrayView<FMassTargetLocationFragment>();
	SquadSharedFragmentList = TArrayView<FETW_MassSquadSharedFragment>();
}

void FETW_MassClientBubbleSquadHandler::SetSpawnedEntityData(const FMassEntityView& EntityView,
	const FETW_ReplicatedSquadAgentData& ReplicatedUnitAgentData, const int32 EntityIdx) const
{
	UWorld* World = OwnerHandler.GetSerializer()->GetWorld();
	check(World);
	ReplicatedUnitAgentData.InitEntity(
		*World, EntityView, CommanderFragmentList[EntityIdx], TeamFragmentList[EntityIdx], TargetFragmentList[EntityIdx], SquadSharedFragmentList[EntityIdx]);
}

void FETW_MassClientBubbleSquadHandler::SetModifiedEntityData(const FMassEntityView& EntityView,
	const FETW_ReplicatedSquadAgentData& ReplicatedUnitAgentData) const
{
	UWorld* World = OwnerHandler.GetSerializer()->GetWorld();
	check(World);
	ReplicatedUnitAgentData.ApplyToEntity(*World, EntityView);
}
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

void FMassReplicationProcessorSquadHandler::AddRequirements(FMassEntityQuery& InQuery)
{
	InQuery.AddRequirement<FETW_MassSquadCommanderFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FETW_MassTeamFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	InQuery.AddSharedRequirement<FETW_MassSquadSharedFragment>(EMassFragmentAccess::ReadWrite);
}

void FMassReplicationProcessorSquadHandler::CacheFragmentViews(FMassExecutionContext& InExecContext)
{
	CommanderFragmentList = InExecContext.GetMutableFragmentView<FETW_MassSquadCommanderFragment>();
	TeamFragmentList = InExecContext.GetMutableFragmentView<FETW_MassTeamFragment>();
	TargetFragmentList = InExecContext.GetMutableFragmentView<FMassTargetLocationFragment>();
	SquadSharedFragmentList = MakeArrayView<FETW_MassSquadSharedFragment>(&InExecContext.GetMutableSharedFragment<FETW_MassSquadSharedFragment>(), 1);
}

void FMassReplicationProcessorSquadHandler::AddEntity(const int32 EntityIdx,
	FETW_ReplicatedSquadAgentData& InOutReplicatedSquadData) const
{
	const FETW_MassSquadCommanderFragment& SquadCommanderFragment = CommanderFragmentList[EntityIdx];
	const FETW_MassTeamFragment& TeamFragment = TeamFragmentList[EntityIdx];
	const FMassTargetLocationFragment& TargetLocationFragment = TargetFragmentList[EntityIdx];
	const FETW_MassSquadSharedFragment& SquadSharedFragment = SquadSharedFragmentList[EntityIdx];

	InOutReplicatedSquadData = FETW_ReplicatedSquadAgentData(SquadCommanderFragment, TeamFragment, TargetLocationFragment, SquadSharedFragment);
}

void FMassReplicationProcessorSquadHandler::ModifyEntity(const FMassReplicatedAgentHandle Handle, const int32 EntityIdx,
	FETW_MassClientBubbleSquadHandler& BubbleSquadHandler, bool bLastClient)
{
	const FETW_MassSquadCommanderFragment& SquadCommanderFragment = CommanderFragmentList[EntityIdx];
	const FETW_MassTeamFragment& TeamFragment = TeamFragmentList[EntityIdx];
	const FMassTargetLocationFragment& TargetLocationFragment = TargetFragmentList[EntityIdx];
	const FETW_MassSquadSharedFragment& SquadSharedFragment = SquadSharedFragmentList[EntityIdx];

	BubbleSquadHandler.SetBubbleSquadData(Handle, SquadCommanderFragment, TeamFragment, TargetLocationFragment, SquadSharedFragment);
}
