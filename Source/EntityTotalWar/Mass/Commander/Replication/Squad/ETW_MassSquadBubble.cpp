// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadBubble.h"

#include "ETW_MassSquadReplicatedAgent.h"
#include "Commander/Replication/SquadUnits/ETW_MassSquadUnitsBubble.h"
#include "Net/UnrealNetwork.h"


namespace UE::Mass::Squad
{
	static int32 MaxAgentsDraw = 300;

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR

	static void DebugDrawReplicatedAgent(FMassEntityHandle Entity, const FMassEntityManager& EntityManager, const float Radius = 50.f)
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
void FETW_MassSquadsClientBubbleHandler::DebugValidateBubbleOnServer()
{
	Super::DebugValidateBubbleOnServer();


#if UE_REPLICATION_COMPILE_SERVER_CODE
	if (UE::Mass::Crowd::bDebugReplicationPositions)
	{
		const FMassEntityManager& EntityManager = Serializer->GetEntityManagerChecked();

		// @todo cap at MaxAgentsDraw for now
		static int32 MaxAgentsDraw = FMath::Min(UE::Mass::Squad::MaxAgentsDraw, (*Agents).Num());

		for (int32 Idx = 0; Idx < MaxAgentsDraw; ++Idx)
		{
			const FETW_MassSquadsFastArrayItem& CrowdItem = (*Agents)[Idx];

			const FMassAgentLookupData& LookupData = AgentLookupArray[CrowdItem.GetHandle().GetIndex()];

			check(LookupData.Entity.IsSet());

			UE::Mass::Squad::DebugDrawReplicatedAgent(LookupData.Entity, EntityManager);
		}
	}
#endif // UE_REPLICATION_COMPILE_SERVER_CODE
}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR


#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
void FETW_MassSquadsClientBubbleHandler::DebugValidateBubbleOnClient()
{
	Super::DebugValidateBubbleOnClient();

	if (UE::Mass::Crowd::bDebugReplicationPositions)
	{
		const FMassEntityManager& EntityManager = Serializer->GetEntityManagerChecked();

		UMassReplicationSubsystem* ReplicationSubsystem = Serializer->GetReplicationSubsystem();
		check(ReplicationSubsystem);

		// @todo cap at MaxAgentsDraw for now
		static int32 MaxAgentsDraw = FMath::Min(UE::Mass::Squad::MaxAgentsDraw, (*Agents).Num());

		for (int32 Idx = 0; Idx < MaxAgentsDraw; ++Idx)
		{
			const FETW_MassSquadsFastArrayItem& CrowdItem = (*Agents)[Idx];

			const FMassReplicationEntityInfo* EntityInfo = ReplicationSubsystem->FindMassEntityInfo(CrowdItem.Agent.GetNetID());

			check(EntityInfo->Entity.IsSet());

			UE::Mass::Squad::DebugDrawReplicatedAgent(EntityInfo->Entity, EntityManager, 35.f);
		}
	}
}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR



void FETW_MassSquadsClientBubbleHandler::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	auto AddRequirementsForSpawnQuery = [this](FMassEntityQuery& InQuery)
	{
		TransformHandler.AddRequirementsForSpawnQuery(InQuery);
		SquadsHandler.AddRequirementsForSpawnQuery(InQuery);
	};

	auto CacheFragmentViewsForSpawnQuery = [this](FMassExecutionContext& InExecContext)
	{
		TransformHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
		SquadsHandler.CacheFragmentViewsForSpawnQuery(InExecContext);
	};

	auto SetSpawnedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassReplicatedSquadsAgent& ReplicatedEntity, const int32 EntityIdx)
	{
		TransformHandler.SetSpawnedEntityData(EntityIdx, ReplicatedEntity.GetReplicatedPositionYawData());
		SquadsHandler.SetSpawnedEntityData(EntityView, ReplicatedEntity.GetReplicatedSquadData(), EntityIdx);
	};

	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassReplicatedSquadsAgent& Item)
	{
		PostReplicatedChangeEntity(EntityView, Item);
	};

	PostReplicatedAddHelper(AddedIndices, AddRequirementsForSpawnQuery, CacheFragmentViewsForSpawnQuery, SetSpawnedEntityData, SetModifiedEntityData);

	TransformHandler.ClearFragmentViewsForSpawnQuery();
	SquadsHandler.ClearFragmentViewsForSpawnQuery();
}

void FETW_MassSquadsClientBubbleHandler::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	auto SetModifiedEntityData = [this](const FMassEntityView& EntityView, const FETW_MassReplicatedSquadsAgent& Item)
	{
		PostReplicatedChangeEntity(EntityView, Item);
	};

	PostReplicatedChangeHelper(ChangedIndices, SetModifiedEntityData);
}

void FETW_MassSquadsClientBubbleHandler::PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassReplicatedSquadsAgent& Item) const
{
	TransformHandler.SetModifiedEntityData(EntityView, Item.GetReplicatedPositionYawData());
	SquadsHandler.SetModifiedEntityData(EntityView, Item.GetReplicatedSquadData());
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
