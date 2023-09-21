// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassSquadReplicationHandlers.h"
#include "MassReplicationProcessor.h"
#include "MassReplicationTransformHandlers.h"
#include "MassClientBubbleHandler.h"
#include "ETW_MassSquadReplicatedAgent.generated.h"

struct FMassEntityQuery;
class FETW_MassSquadsClientBubbleHandler;


/** The data that is replicated specific to each Crowd agent */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassReplicatedSquadsAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()


	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

	/** This function is required to be provided in FReplicatedAgentBase derived classes that use FReplicatedAgentPositionYawData */
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }

	const FETW_ReplicatedSquadAgentData& GetReplicatedSquadData() const { return UnitData; }

	/** This function is required to be provided in FReplicatedAgentBase derived classes that use FETW_ReplicatedSquadAgentData */
	FETW_ReplicatedSquadAgentData& GetReplicatedSquadDataMutable() { return UnitData; }

private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;

	UPROPERTY(Transient)
	FETW_ReplicatedSquadAgentData UnitData;
};

/** Fast array item for efficient agent replication. Remember to make this dirty if any FReplicatedCrowdAgent member variables are modified */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadsFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FETW_MassSquadsFastArrayItem() = default;
	FETW_MassSquadsFastArrayItem(const FETW_MassReplicatedSquadsAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** This typedef is required to be provided in FMassFastArrayItemBase derived classes (with the associated FReplicatedAgentBase derived class) */
	typedef FETW_MassReplicatedSquadsAgent FReplicatedAgentType;

	UPROPERTY()
	FETW_MassReplicatedSquadsAgent Agent;
};
