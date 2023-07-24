// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ETW_MassSquadSubsystem.generated.h"

struct ENTITYTOTALWAR_API FMassSquadManager : public TSharedFromThis<FMassSquadManager>, public FGCObject
{
public:
    
	explicit FMassSquadManager();
	FMassSquadManager(const FMassEntityManager& Other) = delete;
	virtual ~FMassSquadManager();
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/** Called once all UWorldSubsystems have been initialized */
	virtual void PostInitialize() override;
	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void BeginDestroy() override;

	/** Creates all runtime data using main collection */
	void InitializeRuntime();
	
    
    TSharedPtr<FMassSquadManager> SquadManager;
};

template<>
struct TMassExternalSubsystemTraits<UETW_MassSquadSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};