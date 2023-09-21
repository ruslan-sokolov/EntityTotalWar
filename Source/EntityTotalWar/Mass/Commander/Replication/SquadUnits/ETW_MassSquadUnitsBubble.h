// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassEntityView.h"
#include "ETW_MassSquadUnitsReplicatedAgent.h"
#include "ETW_MassSquadUnitsReplicationHandlers.h"

#include "ETW_MassSquadUnitsBubble.generated.h"

namespace UE::Mass::Crowd
{
	extern bool bDebugReplicationPositions;
}

class FETW_MassSquadUnitsClientBubbleHandler;

class ENTITYTOTALWAR_API FETW_MassSquadUnitsClientBubbleHandler : public TClientBubbleHandlerBase<FETW_MassSquadUnitsFastArrayItem>
{
public:
	//template<typename T>
	//friend class FETW_MassClientBubbleSquadUnitHandler<FETW_MassSquadUnitsFastArrayItem>;
	
	typedef TClientBubbleHandlerBase<FETW_MassSquadUnitsFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FETW_MassSquadUnitsFastArrayItem> FMassClientBubbleTransformHandler;
	//typedef FETW_MassClientBubbleSquadUnitHandler<FETW_MassSquadUnitsFastArrayItem> FETW_MassClientBubbleSquadUnitHandler;

	FETW_MassSquadUnitsClientBubbleHandler()
		: TransformHandler(*this),
		SquadUnitsHandler(*this)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE

	const FMassClientBubbleTransformHandler& GetTransformHandler() const { return TransformHandler; }
	FMassClientBubbleTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }

	const FETW_MassClientBubbleSquadUnitHandler& GetSquadUnitsHandler() const { return SquadUnitsHandler; }
	FETW_MassClientBubbleSquadUnitHandler& GetSquadUnitsHandlerMutable() { return SquadUnitsHandler; }

	// protected inherited member accessor
	FMassReplicatedAgentHandleManager& GetAgentHandleManager() { return AgentHandleManager; }
	// protected inherited member accessor
	TArray<FMassAgentLookupData> GetAgentLookuoArray() { return AgentLookupArray; }
	
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

	// protected inherited member accessor
	FMassClientBubbleSerializerBase* GetSerializer() { return Serializer; }
	// protected inherited member accessor
	TArray<FETW_MassSquadUnitsFastArrayItem>* GetAgents() { return Agents; }
	
protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;

	void PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassReplicatedSquadUnitsAgent& Item) const;
#endif //UE_REPLICATION_COMPILE_CLIENT_CODE

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
	virtual void DebugValidateBubbleOnServer() override;
	virtual void DebugValidateBubbleOnClient() override;
#endif // WITH_MASSGAMEPLAY_DEBUG

	FMassClientBubbleTransformHandler TransformHandler;
	FETW_MassClientBubbleSquadUnitHandler SquadUnitsHandler;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadUnitsClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FETW_MassSquadUnitsClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FETW_MassSquadUnitsFastArrayItem, FETW_MassSquadUnitsClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

	FETW_MassSquadUnitsClientBubbleHandler Bubble;

protected:
	/** Fast Array of Agents for efficient replication. Maintained as a freelist on the server, to keep index consistency as indexes are used as Handles into the Array
	 *  Note array order is not guaranteed between server and client so handles will not be consistent between them, FMassNetworkID will be.
	 */
	UPROPERTY(Transient)
	TArray<FETW_MassSquadUnitsFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FETW_MassSquadUnitsClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FETW_MassSquadUnitsClientBubbleSerializer>
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
class ENTITYTOTALWAR_API AETW_MassSquadUnitClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
	
public:
	AETW_MassSquadUnitClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FETW_MassSquadUnitsClientBubbleSerializer& GetSerializer() { return Serializer; }

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FETW_MassSquadUnitsClientBubbleSerializer Serializer;

};