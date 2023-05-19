// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassReplicator.h"
#include "ETW_MassClientBubbleInfo.h"

void UETW_MassReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
}

void UETW_MassReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
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

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, FETW_MassReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle)->FMassReplicatedAgentHandle
	{
		AETW_MassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassClientBubbleInfo>(ClientHandle);

		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

		return BubbleInfo.GetSerializer().Bubble.AddAgent(Context.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, const EMassLOD::Type LOD, const float Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassClientBubbleInfo>(ClientHandle);

		FETW_MassClientBubbleHandler& Bubble = BubbleInfo.GetSerializer().Bubble;

		const bool bLastClient = RepSharedFrag->CachedClientHandles.Last() == ClientHandle;

		PositionYawHandler.ModifyEntity<FETW_MassFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& Context, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassClientBubbleInfo>(ClientHandle);

		BubbleInfo.GetSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FETW_MassFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}



UETW_MassReplicationTrait::UETW_MassReplicationTrait()
{
	Params.BubbleInfoClass = AETW_MassClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UETW_MassReplicator::StaticClass();
}

void UETW_MassReplicationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	//UMassReplicationSubsystem* ReplicationSubsystem = UWorld::GetSubsystem<UMassReplicationSubsystem>(&World);
	//check(ReplicationSubsystem);
	//
	//if (!ReplicationSubsystem->GetBubbleInfoClassHandle(Params.BubbleInfoClass).IsValid())
	//{
	//	ReplicationSubsystem->RegisterBubbleInfoClass(Params.BubbleInfoClass);
	//} // MOVED TO SUBSYSTEM coz it should be called before UMassReplicationSubsystem::AddClient and UMassReplicationSubsystem::SynchronizeClientsAndViewers

	Super::BuildTemplate(BuildContext, World);
}
