// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassNavigationSubsystem.h"

#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "NavigationSystem.h"
#include "MassSignalSubsystem.h"
#include "MassSimulationSubsystem.h"

void UETW_MassNavigationSubsystem::EntityRequestNewPath(const FMassEntityHandle Entity, const FMassPathFollowParams& PathFollowParams, const FVector& MoveFrom, const FVector& MoveTo, FMassPathFragment& OutPathFragment)
{
	ensure(NavigationSystem);

	// create finding query
	const FNavAgentProperties& NavAgentProps = PathFollowParams.NavAgentProps;
	const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(NavAgentProps);
	FPathFindingQuery Query = FPathFindingQuery(this, *NavData, MoveFrom, MoveTo);
	Query.SetAllowPartialPaths(PathFollowParams.bAllowPartialPath);
				
	// request path from nav system:
	const FPathFindingResult PathResult = NavigationSystem->FindPathSync(NavAgentProps, Query, EPathFindingMode::Hierarchical);  // todo: async
	if (PathResult.Result != ENavigationQueryResult::Error)
	{
		EntityNavigationPathMap[Entity] = MoveTemp(*PathResult.Path.Get());
		OutPathFragment.NextPathVertIdx = 0;
	}
	else
	{
		// todo
	}
}

void UETW_MassNavigationSubsystem::EntityRequestNewPathDeferred(FMassExecutionContext& Context,
	const FMassEntityHandle Entity, const FMassPathFollowParams& PathFollowParams, const FVector& MoveFrom,
	const FVector& MoveTo, FMassPathFragment& OutPathFragment)
{
	Context.Defer().PushCommand<FMassDeferredSetCommand>([&](const FMassEntityManager& Manager){
		UETW_MassNavigationSubsystem* NavSubsystem = UWorld::GetSubsystem<UETW_MassNavigationSubsystem>(Manager.GetWorld());
		NavSubsystem->EntityRequestNewPath(Entity, PathFollowParams, MoveFrom, MoveTo, OutPathFragment);
	});
}

bool UETW_MassNavigationSubsystem::EntityExtractNextPathPoint(const FMassEntityHandle Entity, FMassPathFragment& OutPathFragment)
{
	bool bSuccess = false;

	if (const FNavigationPath* const NavPath = EntityNavigationPathMap.Find(Entity))
	{
		const TArray<FNavPathPoint>& PathPoints = NavPath->GetPathPoints();
		if (PathPoints.IsValidIndex(OutPathFragment.NextPathVertIdx))
		{
			OutPathFragment.PathPoint = PathPoints[OutPathFragment.NextPathVertIdx++].Location;
			bSuccess = true;
		}
	}

	if (!bSuccess)
	{
		// todo
	}

	return bSuccess;
}

void UETW_MassNavigationSubsystem::EntityExtractNextPathPointDeferred(FMassExecutionContext& Context,
	const FMassEntityHandle Entity, FMassPathFragment& OutPathFragment)
{
	Context.Defer().PushCommand<FMassDeferredSetCommand>([&](const FMassEntityManager& Manager){
		UETW_MassNavigationSubsystem* NavSubsystem = UWorld::GetSubsystem<UETW_MassNavigationSubsystem>(Manager.GetWorld());
		NavSubsystem->EntityExtractNextPathPoint(Entity, OutPathFragment);
	});
}

void UETW_MassNavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Collection.InitializeDependency<UMassSimulationSubsystem>(); // todo: fix unresolved external link

	SignalSubsystem = Collection.InitializeDependency<UMassSignalSubsystem>();
	checkfSlow(SignalSubsystem != nullptr, TEXT("MassSignalSubsystem is required"));

	NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); // todo check if not null
}

void UETW_MassNavigationSubsystem::Deinitialize()
{
	
	Super::Deinitialize(); 	// should called at the end
}
