// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationPathHandlers.h"
#include "MassReplicationTransformHandlers.h"
#include "MassCrowdReplicatedAgent.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "ETW_MassClientBubbleInfo.generated.h"

struct FETW_MassReplicatedAgent;
struct FETW_MassFastArrayItem;
class FETW_MassClientBubbleHandler;
struct FETW_MassClientBubbleSerializer;
class AETW_MassClientBubbleInfo;

/** The data that is replicated specific to each Crowd agent */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassReplicatedAgent : public FReplicatedAgentBase
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
struct ENTITYTOTALWAR_API FETW_MassFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FETW_MassFastArrayItem() = default;
	FETW_MassFastArrayItem(const FETW_MassReplicatedAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** This typedef is required to be provided in FMassFastArrayItemBase derived classes (with the associated FReplicatedAgentBase derived class) */
	typedef FETW_MassReplicatedAgent FReplicatedAgentType;

	UPROPERTY()
	FETW_MassReplicatedAgent Agent;
};

class ENTITYTOTALWAR_API FETW_MassClientBubbleHandler : public TClientBubbleHandlerBase<FETW_MassFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FETW_MassFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FETW_MassFastArrayItem> FMassClientBubbleTransformHandler;

	FETW_MassClientBubbleHandler()
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

	void PostReplicatedChangeEntity(const FMassEntityView& EntityView, const FETW_MassReplicatedAgent& Item) const;
#endif //UE_REPLICATION_COMPILE_CLIENT_CODE

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
	virtual void DebugValidateBubbleOnServer() override;
	virtual void DebugValidateBubbleOnClient() override;
#endif // WITH_MASSGAMEPLAY_DEBUG

	FMassClientBubbleTransformHandler TransformHandler;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FETW_MassClientBubbleSerializer()
	{
		Bubble.Initialize(Agents, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FETW_MassFastArrayItem, FETW_MassClientBubbleSerializer>(Agents, DeltaParams, *this);
	}

	FETW_MassClientBubbleHandler Bubble;

protected:
	/** Fast Array of Agents for efficient replication. Maintained as a freelist on the server, to keep index consistency as indexes are used as Handles into the Array
	 *  Note array order is not guaranteed between server and client so handles will not be consistent between them, FMassNetworkID will be.
	 */
	UPROPERTY(Transient)
	TArray<FETW_MassFastArrayItem> Agents;
};

template<>
struct TStructOpsTypeTraits<FETW_MassClientBubbleSerializer> : public TStructOpsTypeTraitsBase2<FETW_MassClientBubbleSerializer>
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
class ENTITYTOTALWAR_API AETW_MassClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()
	
public:
	AETW_MassClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FETW_MassClientBubbleSerializer& GetSerializer() { return Serializer; }

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, Transient)
	FETW_MassClientBubbleSerializer Serializer;

};
