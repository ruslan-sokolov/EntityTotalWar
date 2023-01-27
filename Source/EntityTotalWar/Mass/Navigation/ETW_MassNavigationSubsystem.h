// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavigationSystem/Public/NavigationData.h"
#include "ETW_MassNavigationTypes.h"
#include "MassExecutionContext.h"
#include "ETW_MassNavigationSubsystem.generated.h"

class UMassSignalSubsystem;
class UNavigationSystemV1;

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassNavigationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EntityRequestNewPath(const FMassEntityHandle Entity, const FMassPathFollowParams& PathFollowParams, const FVector& MoveFrom, const FVector& MoveTo, FMassPathFragment& OutPathFragment);
	void EntityRequestNewPathDeferred(FMassExecutionContext& Context, const FMassEntityHandle Entity, const FMassPathFollowParams& PathFollowParams, const FVector& MoveFrom, const FVector& MoveTo, FMassPathFragment& OutPathFragment);
	bool EntityExtractNextPathPoint(const FMassEntityHandle Entity, FMassPathFragment& OutPathFragment);
	void EntityExtractNextPathPointDeferred(FMassExecutionContext& Context, const FMassEntityHandle Entity, FMassPathFragment& OutPathFragment);


protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Creates all runtime data using main collection */
	void InitializeRuntime();
	
	UPROPERTY()
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY()
	TObjectPtr<UNavigationSystemV1> NavigationSystem;

	TMap<FMassEntityHandle, FNavigationPath> EntityNavigationPathMap;
};

template<>
struct TMassExternalSubsystemTraits<UETW_MassNavigationSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};