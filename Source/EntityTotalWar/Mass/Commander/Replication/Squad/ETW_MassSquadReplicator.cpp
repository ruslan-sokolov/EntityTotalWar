// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadReplicator.h"

#include "ETW_MassSquadBubble.h"
#include "MassReplicationTransformHandlers.h"
#include "ETW_MassSquadReplicationHandlers.h"
#include "ETW_MassSquadReplicatedAgent.h"

void UETW_MassSquadsReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
	FMassReplicationProcessorSquadHandler::AddRequirements(EntityQuery);
}

void UETW_MassSquadsReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE

	FMassReplicationProcessorPositionYawHandler PositionYawHandler;
	FMassReplicationProcessorSquadHandler SquadHandler;
	FMassReplicationSharedFragment* RepSharedFrag = nullptr;

	auto CacheViewsCallback = [&RepSharedFrag, &PositionYawHandler, &SquadHandler](FMassExecutionContext& Context)
	{
		PositionYawHandler.CacheFragmentViews(Context);
		SquadHandler.CacheFragmentViews(Context);
		RepSharedFrag = &Context.GetMutableSharedFragment<FMassReplicationSharedFragment>();
		check(RepSharedFrag);
	};

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler, &SquadHandler](FMassExecutionContext& Context, const int32 EntityIdx, FETW_MassReplicatedSquadsAgent& InReplicatedAgent, const FMassClientHandle ClientHandle)->FMassReplicatedAgentHandle
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());
		SquadHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedSquadDataMutable());

		return BubbleInfo.GetSerializer().Bubble.AddAgent(Context.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler, &SquadHandler](FMassExecutionContext& Context, const int32 EntityIdx, const EMassLOD::Type LOD, const float Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		FETW_MassSquadsClientBubbleHandler& Bubble = BubbleInfo.GetSerializer().Bubble;

		const bool bLastClient = RepSharedFrag->CachedClientHandles.Last() == ClientHandle;

		PositionYawHandler.ModifyEntity<FETW_MassSquadsFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
		SquadHandler.ModifyEntity(Handle, EntityIdx, Bubble.GetSquadsHandlerMutable(), bLastClient);
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& Context, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		BubbleInfo.GetSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FETW_MassSquadsFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);

#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
