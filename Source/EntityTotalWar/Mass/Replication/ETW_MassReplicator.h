// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationProcessor.h"
#include "MassReplicationTrait.h"
#include "ETW_MassReplicator.generated.h"


UCLASS(meta = (DisplayName = "ETW Replication"))
class ENTITYTOTALWAR_API UETW_MassReplicationTrait : public UMassReplicationTrait
{
	GENERATED_BODY()

public:
	UETW_MassReplicationTrait();

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassReplicator : public UMassReplicatorBase
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
