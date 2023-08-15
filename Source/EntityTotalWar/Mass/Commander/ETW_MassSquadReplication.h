// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationProcessor.h"
#include "MassReplicationTrait.h"
#include "MassReplicationPathHandlers.h"
#include "MassReplicationTransformHandlers.h"
#include "MassCrowdReplicatedAgent.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "ETW_MassSquadReplication.generated.h"

namespace UE::Mass::Crowd
{
	extern bool bDebugReplicationPositions;
}

struct FETW_MassSquadReplicatedAgent;
struct FETW_MassSquadFastArrayItem;
class FETW_MassSquadClientBubbleHandler;
struct FETW_MassSquadClientBubbleSerializer;
class AETW_MassSquadClientBubbleInfo;

/** The data that is replicated specific to each Crowd agent */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadReplicatedAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()


	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

	/** This function is required to be provided in FReplicatedAgentBase derived classes that use FReplicatedAgentPositionYawData */
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }

private:

	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;
};

/** Fast array item for efficient agent replication. Remember to make this dirty if any FReplicatedCrowdAgent member variables are modified */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FETW_MassSquadFastArrayItem() = default;
	FETW_MassSquadFastArrayItem(const FETW_MassSquadReplicatedAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** This typedef is required to be provided in FMassFastArrayItemBase derived classes (with the associated FReplicatedAgentBase derived class) */
	typedef FETW_MassSquadReplicatedAgent FReplicatedAgentType;

	UPROPERTY()
	FETW_MassSquadReplicatedAgent Agent;
};

class ENTITYTOTALWAR_API FETW_MassSquadClientBubbleHandler : public TClientBubbleHandlerBase<FETW_MassSquadFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FETW_MassSquadFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FETW_MassSquadFastArrayItem> FMassClientBubbleTransformHandler;

	FETW_MassSquadClientBubbleHandler()
		: TransformHandler(*this)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE

	const FMassClientBubbleTransformHandler& GetTransformHandler() const { return TransformHandler; }
	FMassClientBubbleTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }
#endif // UE_REPLICATION_COMPILE_SERVER_CODE


protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;

	void PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassSquadReplicatedAgent& Item) const;
#endif //UE_REPLICATION_COMPILE_CLIENT_CODE

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
	virtual void DebugValidateBubbleOnServer() override;
	virtual void DebugValidateBubbleOnClient() override;
#endif // WITH_MASSGAMEPLAY_DEBUG

	FMassClientBubbleTransformHandler TransformHandler;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FETW_MassSquadClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FETW_MassSquadFastArrayItem, FETW_MassSquadClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

	FETW_MassSquadClientBubbleHandler Bubble;

protected:
	/** Fast Array of Agents for efficient replication. Maintained as a freelist on the server, to keep index consistency as indexes are used as Handles into the Array
	 *  Note array order is not guaranteed between server and client so handles will not be consistent between them, FMassNetworkID will be.
	 */
	UPROPERTY(Transient)
	TArray<FETW_MassSquadFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FETW_MassSquadClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FETW_MassSquadClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API AETW_MassSquadClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
	
public:
	AETW_MassSquadClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FETW_MassSquadClientBubbleSerializer& GetSerializer() { return Serializer; }

	UFUNCTION()
	void OnRep_Serializer();
protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Serializer, Transient)
	FETW_MassSquadClientBubbleSerializer Serializer;


};

UCLASS(meta = (DisplayName = "ETW Squad Replication"))
class ENTITYTOTALWAR_API UETW_MassSquadReplicationTrait : public UMassReplicationTrait
{
	GENERATED_BODY()

public:
	UETW_MassSquadReplicationTrait();
};

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()
	
public:
	/**
	 * Overridden to add specific entity query requirements for replication.
	 * Usually we add replication processor handler requirements.
	 */
	void AddRequirements(FMassEntityQuery& EntityQuery) override;

	/**
	 * Overridden to process the client replication.
	 * This methods should call CalculateClientReplication with the appropriate callback implementation.
	 */
	void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
