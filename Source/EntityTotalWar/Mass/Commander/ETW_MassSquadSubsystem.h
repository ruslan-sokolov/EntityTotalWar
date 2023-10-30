// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ETW_MassSquadProcessors.h"

#include "ETW_MassSquadSubsystem.generated.h"

struct ENTITYTOTALWAR_API FMassSquadManager : public TSharedFromThis<FMassSquadManager>, public FGCObject
{
	FMassSquadManager() = delete;
	explicit FMassSquadManager(UObject* InOwner);
	FMassSquadManager(const FMassEntityManager& Other) = delete;
	virtual ~FMassSquadManager();

	virtual void Initialize();
	virtual void Deinitialize();

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FMassSquadManager");
	}
	// End of FGCObject interface
	
private:
	TWeakObjectPtr<UObject> Owner;

	std::atomic<uint32> UnitIdGenerator = 0;
	std::atomic<uint32> SquadIdGenerator = 0;

	TMultiMap<uint32, uint32> SquadToEntityMap;
	TMap<uint32, uint32> EntityToSquadMap;

public:
	// Squad Manager functions
	uint32 GetSquadId() { return SquadIdGenerator.fetch_add(1); };
	uint32 GetUnitId() { return UnitIdGenerator.fetch_add(1); };
	
	// Map unit to squad

	//entity handle get squad entity (squad index, manager)
	//array<entityhandle> get units entity (squad index, manager)
	//get squad entity from unit id (unit index, manager)
	// get squads of team (team index, manager)

	// End Squad Manager functions
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/** Called once all UWorldSubsystems have been initialized */
	virtual void PostInitialize() override;
	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Creates all runtime data using main collection */
	void InitializeRuntime();

public:

	UETW_MassSquadSubsystem();
	FMassSquadManager& GetMutablSquadManager() const { check(SquadManager); return *SquadManager.Get(); }
	const FMassSquadManager& GetSquadManager() { check(SquadManager); return *SquadManager.Get(); }

	UMassSquadUnitsPostSpawnProcessor* GetSquadUnitsPostSpawnProcessor() const { return SquadUnitsPostSpawnProcessor; }
	UMassSquadPostSpawnProcessor* GetSquadPostSpawnProcessor() const { return SquadPostSpawnProcessor; }

	UMassProcessor* GetSpawnDataInitializer(TSubclassOf<UMassProcessor> InitializerClass);
	
protected:
    TSharedPtr<FMassSquadManager> SquadManager;

	UPROPERTY()
	TObjectPtr<UMassSquadUnitsPostSpawnProcessor> SquadUnitsPostSpawnProcessor;

	UPROPERTY()
	TObjectPtr<UMassSquadPostSpawnProcessor> SquadPostSpawnProcessor;
	
	UPROPERTY()
	TArray<TObjectPtr<UMassProcessor>> SpawnDataInitializers;
};

template<>
struct TMassExternalSubsystemTraits<UETW_MassSquadSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};