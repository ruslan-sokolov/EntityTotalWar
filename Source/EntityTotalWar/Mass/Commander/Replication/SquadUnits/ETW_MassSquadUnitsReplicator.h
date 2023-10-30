// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationProcessor.h"

#include "ETW_MassSquadUnitsReplicator.generated.h"



/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadUnitsReplicator : public UMassReplicatorBase
{
	GENERATED_BODY()
	
public:
	/**
	 * Overridden to add specific entity query requirements for replication.
	 * Usually we add replication processor handler requirements.
	 */
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;

	/**
	 * Overridden to process the client replication.
	 * This methods should call CalculateClientReplication with the appropriate callback implementation.
	 */
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
