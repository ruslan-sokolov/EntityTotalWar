// Fill out your copyright notice in the Description page of Project Settings.

#include "ETW_MassSquadUnitsReplicator.h"
#include "MassReplicationTransformHandlers.h"
#include "ETW_MassSquadUnitsReplicatedAgent.h"
#include "ETW_MassSquadUnitsBubble.h"


void UETW_MassSquadUnitsReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
}

void UETW_MassSquadUnitsReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE

	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context)
	{
		PositionYawHandler.CacheFragmentViews(Context);
		RepSharedFrag = &Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, FETW_MassReplicatedSquadUnitsAgent& InReplicatedAgent, const FMassClientHandle ClientHandle)->FMassReplicatedAgentHandle
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

		return BubbleInfo.GetSerializer().Bubble.AddAgent(Context.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, const EMassLOD::Type LOD, const float Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		FETW_MassSquadUnitsClientBubbleHandler& Bubble = BubbleInfo.GetSerializer().Bubble;

		const bool bLastClient = RepSharedFrag->CachedClientHandles.Last() == ClientHandle;

		PositionYawHandler.ModifyEntity<FETW_MassSquadUnitsFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& Context, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		BubbleInfo.GetSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FETW_MassSquadUnitsFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);

	// hack fix query when only one arhcheotype and no updating valid archetypes cos last archeotype version in manager is same
	//Context.GetEntityManagerChecked().DebugForceArchetypeDataVersionBump();
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}



UETW_MassSquadUnitsReplicationTrait::UETW_MassSquadUnitsReplicationTrait()
{
	Params.BubbleInfoClass = AETW_MassSquadClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UETW_MassSquadUnitsReplicator::StaticClass();
}
