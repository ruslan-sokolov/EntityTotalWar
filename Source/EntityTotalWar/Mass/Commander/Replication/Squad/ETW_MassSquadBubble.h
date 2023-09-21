// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationTypes.h"
#include "MassEntityView.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassReplicationTransformHandlers.h"
#include "ETW_MassSquadReplicationHandlers.h"
#include "ETW_MassSquadReplicatedAgent.h"

#include "ETW_MassSquadBubble.generated.h"


class FETW_MassSquadsClientBubbleHandler;

class ENTITYTOTALWAR_API FETW_MassSquadsClientBubbleHandler : public TClientBubbleHandlerBase<FETW_MassSquadsFastArrayItem>
{
public:
	
	typedef TClientBubbleHandlerBase<FETW_MassSquadsFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FETW_MassSquadsFastArrayItem> FMassClientBubbleTransformHandler;

	FETW_MassSquadsClientBubbleHandler()
		: TransformHandler(*this),
		SquadsHandler(*this)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE

	const FMassClientBubbleTransformHandler& GetTransformHandler() const { return TransformHandler; }
	FMassClientBubbleTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }

	const FETW_MassClientBubbleSquadHandler& GetSquadsHandler() const { return SquadsHandler; }
	FETW_MassClientBubbleSquadHandler& GetSquadsHandlerMutable() { return SquadsHandler; }

	// protected inherited member accessor
	FMassReplicatedAgentHandleManager& GetAgentHandleManager() { return AgentHandleManager; }
	// protected inherited member accessor
	TArray<FMassAgentLookupData> GetAgentLookuoArray() { return AgentLookupArray; }
	
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

	// protected inherited member accessor
	FMassClientBubbleSerializerBase* GetSerializer() { return Serializer; }
	// protected inherited member accessor
	TArray<FETW_MassSquadsFastArrayItem>* GetAgents() { return Agents; }
	
protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;

	void PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassReplicatedSquadsAgent& Item) const;
#endif //UE_REPLICATION_COMPILE_CLIENT_CODE

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
	virtual void DebugValidateBubbleOnServer() override;
	virtual void DebugValidateBubbleOnClient() override;
#endif // WITH_MASSGAMEPLAY_DEBUG

	FMassClientBubbleTransformHandler TransformHandler;
	FETW_MassClientBubbleSquadHandler SquadsHandler;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadsClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FETW_MassSquadsClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FETW_MassSquadsFastArrayItem, FETW_MassSquadsClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

	FETW_MassSquadsClientBubbleHandler Bubble;

protected:
	/** Fast Array of Agents for efficient replication. Maintained as a freelist on the server, to keep index consistency as indexes are used as Handles into the Array
	 *  Note array order is not guaranteed between server and client so handles will not be consistent between them, FMassNetworkID will be.
	 */
	UPROPERTY(Transient)
	TArray<FETW_MassSquadsFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FETW_MassSquadsClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FETW_MassSquadsClientBubbleSerializer>
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

	FETW_MassSquadsClientBubbleSerializer& GetSerializer() { return Serializer; }

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FETW_MassSquadsClientBubbleSerializer Serializer;


};