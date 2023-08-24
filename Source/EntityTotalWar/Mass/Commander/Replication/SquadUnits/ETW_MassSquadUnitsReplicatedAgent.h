// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationTransformHandlers.h"
#include "ETW_MassSquadUnitsReplicationHandlers.h"

#include "ETW_MassSquadUnitsReplicatedAgent.generated.h"


/** The data that is replicated specific to each Crowd agent */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassReplicatedSquadUnitsAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()


	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

	/** This function is required to be provided in FReplicatedAgentBase derived classes that use FReplicatedAgentPositionYawData */
	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }

	const FETW_ReplicatedSquadUnitAgentData& GetReplicatedSquadUnitData() const { return UnitData; }

	/** This function is required to be provided in FReplicatedAgentBase derived classes that use FETW_ReplicatedSquadUnitAgentData */
	FETW_ReplicatedSquadUnitAgentData& GetReplicatedSquadUnitDataMutable() { return UnitData; }

private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;

	UPROPERTY(Transient)
	FETW_ReplicatedSquadUnitAgentData UnitData;
};

/** Fast array item for efficient agent replication. Remember to make this dirty if any FReplicatedCrowdAgent member variables are modified */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadUnitsFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FETW_MassSquadUnitsFastArrayItem() = default;
	FETW_MassSquadUnitsFastArrayItem(const FETW_MassReplicatedSquadUnitsAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{}

	/** This typedef is required to be provided in FMassFastArrayItemBase derived classes (with the associated FReplicatedAgentBase derived class) */
	typedef FETW_MassReplicatedSquadUnitsAgent FReplicatedAgentType;

	UPROPERTY()
	FETW_MassReplicatedSquadUnitsAgent Agent;
};

