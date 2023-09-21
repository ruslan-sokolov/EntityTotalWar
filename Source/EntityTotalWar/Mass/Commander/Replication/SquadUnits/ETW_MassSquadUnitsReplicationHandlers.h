// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassReplicationTypes.h"
#include "MassEntityView.h"
#include "MassClientBubbleHandler.h"
#include "MassReplicationTransformHandlers.h"
#include "Commander/ETW_MassSquadFragments.h"

#include "ETW_MassSquadUnitsReplicationHandlers.generated.h"


struct FMassEntityQuery;
class FETW_MassSquadUnitsClientBubbleHandler;

//////////////////////////////////////////////////////////////////////////// FETW_ReplicatedSquadUnitAgentData ////////////////////////////////////////////////////////////////////////////
/**
 * To replicate squad unit make a member variable of this class in your FReplicatedAgentBase derived class. In the FReplicatedAgentBase derived class you must also provide an accessor function
 * FETW_ReplicatedSquadUnitAgentData& GetReplicatedSquadUnitDataMutable().
 */
USTRUCT()
struct ENTITYTOTALWAR_API FETW_ReplicatedSquadUnitAgentData
{
	GENERATED_BODY()

	FETW_ReplicatedSquadUnitAgentData() = default;
	explicit FETW_ReplicatedSquadUnitAgentData(
		const FETW_MassUnitFragment& UnitFragment,
		const FETW_MassTeamFragment& TeamFragment,
		const FMassTargetLocationFragment& TargetLocationFragment
		);

	void InitEntity(const UWorld& InWorld,
					const FMassEntityView& InEntityView,
					FETW_MassUnitFragment& OutUnitFragment,
					FETW_MassTeamFragment& OutTeamFragment,
					FMassTargetLocationFragment& OutTargetLocationFragment) const;

	void ApplyToEntity(const UWorld& InWorld, const FMassEntityView& InEntityView) const;

	UPROPERTY(Transient)
	FVector_NetQuantize TargetLocation;
	
	UPROPERTY(Transient)
	uint32 UnitIndex = 0;
	
	UPROPERTY(Transient)
	int8 TeamIndex = 0;
};

//////////////////////////////////////////////////////////////////////////// FETW_MassClientBubbleSquadUnitHandler ////////////////////////////////////////////////////////////////////////////
/**
 * To replicate squad unit make a member variable of this class in your TClientBubbleHandlerBase derived class. This class is a friend of TTETW_MassClientBubbleSquadUnitHandler.
 * It is meant to have access to the protected data declared there.
 */
class FETW_MassClientBubbleSquadUnitHandler
{
public:
	FETW_MassClientBubbleSquadUnitHandler(FETW_MassSquadUnitsClientBubbleHandler& InOwnerHandler)
		: OwnerHandler(InOwnerHandler)
	{}

#if UE_REPLICATION_COMPILE_SERVER_CODE
	/** Sets the squad unit data in the client bubble on the server */
	void SetBubbleSquadUnitData(const FMassReplicatedAgentHandle Handle,
		const FETW_MassUnitFragment& UnitFragment,
		const FETW_MassTeamFragment& TeamFragment,
		const FMassTargetLocationFragment& TargetLocationFragment);
#endif // UE_REPLICATION_COMPILE_SERVER_CODE

#if UE_REPLICATION_COMPILE_CLIENT_CODE
	/**
	 * When entities are spawned in Mass by the replication system on the client, a spawn query is used to set the data on the spawned entities.
	 * The following functions are used to configure the query and then set that data for squad unit.
	 */
	static void AddRequirementsForSpawnQuery(FMassEntityQuery& InQuery);
	void CacheFragmentViewsForSpawnQuery(FMassExecutionContext& InExecContext);
	void ClearFragmentViewsForSpawnQuery();

	void SetSpawnedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadUnitAgentData& ReplicatedUnitAgentData, const int32 EntityIdx) const;

	/** Call this when an Entity that has already been spawned is modified on the client */
	void SetModifiedEntityData(const FMassEntityView& EntityView, const FETW_ReplicatedSquadUnitAgentData& ReplicatedUnitAgentData) const;
#endif // UE_REPLICATION_COMPILE_CLIENT_CODE

protected:
	TArrayView<FETW_MassUnitFragment> UnitFragmentList;
	TArrayView<FETW_MassTeamFragment> TeamFragmentList;
	TArrayView<FMassTargetLocationFragment> TargetFragmentList;

	FETW_MassSquadUnitsClientBubbleHandler& OwnerHandler;
};

//////////////////////////////////////////////////////////////////////////// FMassReplicationProcessorSquadUnitHandler ////////////////////////////////////////////////////////////////////////////
/**
 * Used to replicate squad unit by your UMassReplicationProcessorBase derived class. This class should only get used on the server.
 */
class ENTITYTOTALWAR_API FMassReplicationProcessorSquadUnitHandler
{
public:
	/** Adds the requirements for the  squad unit  to the query. */
	static void AddRequirements(FMassEntityQuery& InQuery);

	/** Cache any component views you want to, this will get called before we iterate through entities. */
	void CacheFragmentViews(FMassExecutionContext& ExecContext);

	/**
	 * Set the replicated squad unit data when we are adding an entity to the client bubble.
	 * @param EntityIdx the index of the entity in fragment views that have been cached.
	 * @param InOutReplicatedSquadUnitData the data to set.
	 */
	void AddEntity(const int32 EntityIdx, FETW_ReplicatedSquadUnitAgentData& InOutReplicatedSquadUnitData) const;

	/**
	 * Set the replicated squad unit data when we are modifying an entity that already exists in the client bubble.
	 * @param Handle to the agent in the TMassClientBubbleHandler (that TMassClientBubbleSquadUnitHandler is a member variable of).
	 * @param EntityIdx the index of the entity in fragment views that have been cached.
	 * @param BubbleSquadUnitHandler handler to actually set the data in the client bubble
	 * @param bLastClient means it safe to reset any dirtiness
	 */
	//template<typename AgentArrayItem>
	//void ModifyEntity(const FMassReplicatedAgentHandle Handle, const int32 EntityIdx, FETW_MassClientBubbleSquadUnitHandler<AgentArrayItem>& BubbleSquadUnitHandler, bool bLastClient);
	void ModifyEntity(const FMassReplicatedAgentHandle Handle, const int32 EntityIdx, FETW_MassClientBubbleSquadUnitHandler& BubbleSquadUnitHandler, bool bLastClient);

	TArrayView<FETW_MassUnitFragment> UnitFragmentList;
	TArrayView<FETW_MassTeamFragment> TeamFragmentList;
	TArrayView<FMassTargetLocationFragment> TargetFragmentList;
};
