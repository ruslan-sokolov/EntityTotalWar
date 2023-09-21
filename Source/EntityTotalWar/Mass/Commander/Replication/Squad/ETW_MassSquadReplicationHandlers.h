// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationTypes.h"
#include "MassEntityView.h"
#include "MassClientBubbleHandler.h"
#include "Commander/ETW_MassSquadFragments.h"

#include "ETW_MassSquadReplicationHandlers.generated.h"

class FETW_MassSquadsClientBubbleHandler;

//////////////////////////////////////////////////////////////////////////// FETW_ReplicatedSquadAgentData ////////////////////////////////////////////////////////////////////////////
/**
 * To replicate squad unit make a member variable of this class in your FReplicatedAgentBase derived class. In the FReplicatedAgentBase derived class you must also provide an accessor function
 * FETW_ReplicatedSquadAgentData& GetReplicatedSquadDataMutable().
 */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_ReplicatedSquadAgentData
{
	GENERATED_BODY()

	FETW_ReplicatedSquadAgentData() = default;
	explicit FETW_ReplicatedSquadAgentData(
		const FETW_MassSquadCommanderFragment& SquadCommanderFragment,
		const FETW_MassTeamFragment& TeamFragment,
		const FMassTargetLocationFragment& TargetLocationFragment,
		const FETW_MassSquadSharedFragment& SquadSharedFragment
		);

	void InitEntity(const UWorld& InWorld,
					const FMassEntityView& InEntityView,
					FETW_MassSquadCommanderFragment& OutSquadCommanderFragment,
					FETW_MassTeamFragment& OutTeamFragment,
					FMassTargetLocationFragment& OutTargetLocationFragment,
					FETW_MassSquadSharedFragment& OutSquadSharedFragment
					) const;

	void ApplyToEntity(const UWorld& InWorld, const FMassEntityView& InEntityView) const;

	UPROPERTY(Transient)
	FETW_MassFormation Formation;
	
	UPROPERTY(Transient)
	FVector_NetQuantize TargetLocation;

	UPROPERTY(Transient)
	TObjectPtr<UMassCommanderComponent> CommanderComp = nullptr;
	
	UPROPERTY(Transient)
	uint32 SquadIndex = 0;

	UPROPERTY(Transient)
	uint32 TargetSquadIndex = 0;
	
	UPROPERTY(Transient)
	int8 TeamIndex = 0;
	
};

//////////////////////////////////////////////////////////////////////////// FETW_MassClientBubbleSquadHandler ////////////////////////////////////////////////////////////////////////////
/**
 * To replicate squad unit make a member variable of this class in your TClientBubbleHandlerBase derived class. This class is a friend of TTETW_MassClientBubbleSquadHandler.
 * It is meant to have access to the protected data declared there.
 */
class FETW_MassClientBubbleSquadHandler
{
public:
	FETW_MassClientBubbleSquadHandler(FETW_MassSquadsClientBubbleHandler& InOwnerHandler)
		: OwnerHandler(InOwnerHandler)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE
	/** Sets the squad unit data in the client bubble on the server */
	void SetBubbleSquadData(const FMassReplicatedAgentHandle Handle,
		const FETW_MassSquadCommanderFragment& SquadCommanderFragment,
		const FETW_MassTeamFragment& TeamFragment,
		const FMassTargetLocationFragment& TargetLocationFragment,
		const FETW_MassSquadSharedFragment& SquadSharedFragment
		);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
	/**
	 * When entities are spawned in Mass by the replication system on the client, a spawn query is used to set the data on the spawned entities.
	 * The following functions are used to configure the query and then set that data for squad unit.
	 */
	static void AddRequirementsForSpawnQuery(FMassEntityQuery& InQuery);
	void CacheFragmentViewsForSpawnQuery(FMassExecutionContext& InExecContext);
	void ClearFragmentViewsForSpawnQuery();

	void SetSpawnedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadAgentData& ReplicatedUnitAgentData, const int32 EntityIdx) const;

	/** Call this when an Entity that has already been spawned is modified on the client */
	void SetModifiedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadAgentData& ReplicatedUnitAgentData) const;
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

protected:
	TArrayView<FETW_MassSquadCommanderFragment> CommanderFragmentList;
	TArrayView<FETW_MassTeamFragment> TeamFragmentList;
	TArrayView<FMassTargetLocationFragment> TargetFragmentList;
	TArrayView<FETW_MassSquadSharedFragment> SquadSharedFragmentList;

	FETW_MassSquadsClientBubbleHandler& OwnerHandler;
};

//////////////////////////////////////////////////////////////////////////// FMassReplicationProcessorSquadHandler ////////////////////////////////////////////////////////////////////////////
/**
 * Used to replicate squad unit by your UMassReplicationProcessorBase derived class. This class should only get used on the server.
 */
class ENTITYTOTALWAR_API FMassReplicationProcessorSquadHandler
{
public:
	/** Adds the requirements for the  squad unit  to the query. */
	static void AddRequirements(FMassEntityQuery& InQuery);

	/** Cache any component views you want to, this will get called before we iterate through entities. */
	void CacheFragmentViews(FMassExecutionContext& InExecContext);

	/**
	 * Set the replicated squad unit data when we are adding an entity to the client bubble.
	 * @param EntityIdx the index of the entity in fragment views that have been cached.
	 * @param InOutReplicatedSquadData the data to set.
	 */
	void AddEntity(const int32 EntityIdx, FETW_ReplicatedSquadAgentData& InOutReplicatedSquadData) const;

	/**
	 * Set the replicated squad unit data when we are modifying an entity that already exists in the client bubble.
	 * @param Handle to the agent in the TMassClientBubbleHandler (that TMassClientBubbleSquadHandler is a member variable of).
	 * @param EntityIdx the index of the entity in fragment views that have been cached.
	 * @param BubbleSquadHandler handler to actually set the data in the client bubble
	 * @param bLastClient means it safe to reset any dirtiness
	 */
	void ModifyEntity(const FMassReplicatedAgentHandle Handle, const int32 EntityIdx, FETW_MassClientBubbleSquadHandler& BubbleSquadHandler, bool bLastClient);

	TArrayView<FETW_MassSquadCommanderFragment> CommanderFragmentList;
	TArrayView<FETW_MassTeamFragment> TeamFragmentList;
	TArrayView<FMassTargetLocationFragment> TargetFragmentList;
	TArrayView<FETW_MassSquadSharedFragment> SquadSharedFragmentList;
};