// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ETW_MassSquadSubsystem.generated.h"


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
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Creates all runtime data using main collection */
	void InitializeRuntime();
};

template<>
struct TMassExternalSubsystemTraits<UETW_MassSquadSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};