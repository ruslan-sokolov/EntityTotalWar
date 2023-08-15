// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadReplication.h"
#include "Net/UnrealNetwork.h"

namespace UE::Mass::Crowd
{

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
	// @todo provide a better way of selecting agents to debug
	constexpr int32 MaxAgentsDraw = 300;

	void DebugDrawReplicatedAgent(FMassEntityHandle Entity, const FMassEntityManager& EntityManager, const float Radius = 50.f)
	{
		static const FVector DebugCylinderHeight = FVector(0.f, 0.f, 200.f);

		const FMassEntityView EntityView(EntityManager, Entity);

		const FTransformFragment& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
		const FMassNetworkIDFragment& NetworkIDFragment = EntityView.GetFragmentData<FMassNetworkIDFragment>();

		const FVector& Pos = TransformFragment.GetTransform().GetLocation();
		const uint32 NetworkID = NetworkIDFragment.NetID.GetValue();

		// Multiply by a largeish number that is not a multiple of 256 to separate out the color shades a bit
		const uint32 InitialColor = NetworkID * 20001;

		const uint8 NetworkIdMod3 = NetworkID % 3;
		FColor DebugCylinderColor;

		// Make a deterministic color from the Mod by 3 to vary how we create it
		if (NetworkIdMod3 == 0)
		{
			DebugCylinderColor = FColor(InitialColor % 256, InitialColor / 256 % 256, InitialColor / 256 / 256 % 256);
		}
		else if (NetworkIdMod3 == 1)
		{
			DebugCylinderColor = FColor(InitialColor / 256 / 256 % 256, InitialColor % 256, InitialColor / 256 % 256);
		}
		else
		{
			DebugCylinderColor = FColor(InitialColor / 256 % 256, InitialColor / 256 / 256 % 256, InitialColor % 256);
		}

		const UWorld* World = EntityManager.GetWorld();
		if (World != nullptr && World->GetNetMode() == NM_Client)
		{
			DrawDebugCylinder(World, Pos, Pos + 0.5f * DebugCylinderHeight, Radius, /*segments = */24, DebugCylinderColor);
		}
		else
		{
			DrawDebugCylinder(World, Pos + 0.5f * DebugCylinderHeight, Pos + DebugCylinderHeight, Radius, /*segments = */24, DebugCylinderColor);
		}
	}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
}


#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
void FETW_MassSquadClientBubbleHandler::DebugValidateBubbleOnServer()
{
	Super::DebugValidateBubbleOnServer();


#if UE_REPLICATION_COMPILE_SERVER_CODE
	if (UE::Mass::Crowd::bDebugReplicationPositions)
	{
		const FMassEntityManager& EntityManager = Serializer->GetEntityManagerChecked();

		// @todo cap at MaxAgentsDraw for now
		const int32 MaxAgentsDraw = FMath::Min(UE::Mass::Crowd::MaxAgentsDraw, (*Agents).Num());

		for (int32 Idx = 0; Idx < MaxAgentsDraw; ++Idx)
		{
			const FETW_MassSquadFastArrayItem& CrowdItem = (*Agents)[Idx];

			const FMassAgentLookupData& LookupData = AgentLookupArray[CrowdItem.GetHandle().GetIndex()];

			check(LookupData.Entity.IsSet());

			UE::Mass::Crowd::DebugDrawReplicatedAgent(LookupData.Entity, EntityManager);
		}
	}
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR


#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
void FETW_MassSquadClientBubbleHandler::DebugValidateBubbleOnClient()
{
	Super::DebugValidateBubbleOnClient();

	if (UE::Mass::Crowd::bDebugReplicationPositions)
	{
		const FMassEntityManager& EntityManager = Serializer->GetEntityManagerChecked();

		UMassReplicationSubsystem* ReplicationSubsystem = Serializer->GetReplicationSubsystem();
		check(ReplicationSubsystem);

		// @todo cap at MaxAgentsDraw for now
		const int32 MaxAgentsDraw = FMath::Min(UE::Mass::Crowd::MaxAgentsDraw, (*Agents).Num());

		for (int32 Idx = 0; Idx < MaxAgentsDraw; ++Idx)
		{
			const FETW_MassSquadFastArrayItem& CrowdItem = (*Agents)[Idx];

			const FMassReplicationEntityInfo* EntityInfo = ReplicationSubsystem->FindMassEntityInfo(CrowdItem.Agent.GetNetID());

			check(EntityInfo->Entity.IsSet());

			UE::Mass::Crowd::DebugDrawReplicatedAgent(EntityInfo->Entity, EntityManager, 35.f);
		}
	}
}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR



void FETW_MassSquadClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(InQuery);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassSquadReplicatedAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());
	};

	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassSquadReplicatedAgent& Item)
	{
		PostReplicatedChangeEntity(EntityView, Item);
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
}

void FETW_MassSquadClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassSquadReplicatedAgent& Item)
	{
		PostReplicatedChangeEntity(EntityView, Item);
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}

void FETW_MassSquadClientBubbleHandler::PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassSquadReplicatedAgent& Item) const
{
	TransformHandler.SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
}

AETW_MassSquadClientBubbleInfo::AETW_MassSquadClientBubbleInfo(const FObjectInitializer& ObjectInitializer)
	: AMassClientBubbleInfoBase(ObjectInitializer)
{
	Serializers.Add(&Serializer);
}

void AETW_MassSquadClientBubbleInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	// Technically, this doesn't need to be PushModel based because it's a FastArray and they ignore it.
	DOREPLIFETIME_WITH_PARAMS_FAST(AETW_MassSquadClientBubbleInfo, Serializer, SharedParams);
}

void AETW_MassSquadClientBubbleInfo::OnRep_Serializer()
{
	// hack fix query when only one arhcheotype and no updating valid archetypes cos last archeotype version in manager is same
	//FMassEntityManager& EntityManager = Serializer.GetEntityManagerChecked();
	//EntityManager.DebugForceArchetypeDataVersionBump();
}


void UETW_MassSquadReplicator::AddRequirements(FMassEntityQuery& EntityQuery)
{
	FMassReplicationProcessorPositionYawHandler::AddRequirements(EntityQuery);
}

void UETW_MassSquadReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
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

	auto AddEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, FETW_MassSquadReplicatedAgent& InReplicatedAgent, const FMassClientHandle ClientHandle)->FMassReplicatedAgentHandle
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		PositionYawHandler.AddEntity(EntityIdx, InReplicatedAgent.GetReplicatedPositionYawDataMutable());

		return BubbleInfo.GetSerializer().Bubble.AddAgent(Context.GetEntity(EntityIdx), InReplicatedAgent);
	};

	auto ModifyEntityCallback = [&RepSharedFrag, &PositionYawHandler](FMassExecutionContext& Context, const int32 EntityIdx, const EMassLOD::Type LOD, const float Time, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		FETW_MassSquadClientBubbleHandler& Bubble = BubbleInfo.GetSerializer().Bubble;

		const bool bLastClient = RepSharedFrag->CachedClientHandles.Last() == ClientHandle;

		PositionYawHandler.ModifyEntity<FETW_MassSquadFastArrayItem>(Handle, EntityIdx, Bubble.GetTransformHandlerMutable());
	};

	auto RemoveEntityCallback = [&RepSharedFrag](FMassExecutionContext& Context, const FMassReplicatedAgentHandle Handle, const FMassClientHandle ClientHandle)
	{
		AETW_MassSquadClientBubbleInfo& BubbleInfo = RepSharedFrag->GetTypedClientBubbleInfoChecked<AETW_MassSquadClientBubbleInfo>(ClientHandle);

		BubbleInfo.GetSerializer().Bubble.RemoveAgentChecked(Handle);
	};

	CalculateClientReplication<FETW_MassSquadFastArrayItem>(Context, ReplicationContext, CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);

	// hack fix query when only one arhcheotype and no updating valid archetypes cos last archeotype version in manager is same
	//Context.GetEntityManagerChecked().DebugForceArchetypeDataVersionBump();
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}



UETW_MassSquadReplicationTrait::UETW_MassSquadReplicationTrait()
{
	Params.BubbleInfoClass = AETW_MassSquadClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UETW_MassSquadReplicator::StaticClass();
}
