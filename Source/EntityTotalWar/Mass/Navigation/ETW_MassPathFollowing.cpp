// Fill out your copyright notice in the Description page of Project Settings.

#include "ETW_MassPathFollowing.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassObserverRegistry.h"
#include "MassSimulationLOD.h"
#include "../Common/Fragments/ETW_MassFragments.h"
#include "NavigationSystem.h"
#include "ETW_MassNavigationSubsystem.h"

void UETW_PathFollowTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassPathFragment>();
	BuildContext.AddFragment<FMassTargetLocationFragment>();
	BuildContext.RequireFragment<FMassMoveTargetFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

UETW_MassPathFollowProcessor::UETW_MassPathFollowProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
}

void UETW_MassPathFollowProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassPathFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddSubsystemRequirement<UETW_MassNavigationSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddConstSharedRequirement<FMassPathFollowParams>(EMassFragmentPresence::All);

	EntityQuery.AddTagRequirement<FMassPathFollowingProgressTag>(EMassFragmentPresence::All);
}

void UETW_MassPathFollowProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = EntityManager.GetWorld();
	UETW_MassNavigationSubsystem& NavigationSubsystem = Context.GetMutableSubsystemChecked<UETW_MassNavigationSubsystem>(World);

	// updates FMassMoveTargetFragment with new path point
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&NavigationSubsystem, &World](FMassExecutionContext& Context){
		const FMassPathFollowParams& PathFollowParams = Context.GetConstSharedFragment<FMassPathFollowParams>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassPathFragment> PathFragList = Context.GetMutableFragmentView<FMassPathFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetFragList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassTargetLocationFragment> TargetLocationList = Context.GetMutableFragmentView<FMassTargetLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FMassMoveTargetFragment& MoveTargetFrag = MoveTargetFragList[EntityIndex];
			FMassPathFragment& PathFrag = PathFragList[EntityIndex];
			FVector& TargetLocation = TargetLocationList[EntityIndex].Target;
			const FVector CurrentLocation = TransformList[EntityIndex].GetTransform().GetLocation();
			
			if (MoveTargetFrag.GetCurrentAction() == EMassMovementAction::Stand || MoveTargetFrag.DistanceToGoal <= PathFollowParams.SlackRadius)
			{
				FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);
				
				// deferred:
				Context.Defer().PushCommand<FMassDeferredSetCommand>([&](const FMassEntityManager& Manager){
					
					if (NavigationSubsystem.EntityExtractNextPathPoint(EntityHandle, PathFrag))
					{
						// set new target from path list
						MoveTargetFrag.CreateNewAction(EMassMovementAction::Move, *World);
						MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand;
						TargetLocation = PathFrag.GetPathPoint();
					}
					else
					{
						// add tag for able to process path with UETW_MassPathFollowProcessor
						Context.Defer().AddTag<FMassPathFollowingProgressTag>(EntityHandle);
						Context.Defer().RemoveTag<FMassPathFollowingRequestTag>(EntityHandle);
					}
				});
			}
			
			// update MoveTargetFragment
			FVector DirectionToTarget = TargetLocation - CurrentLocation;
			MoveTargetFrag.Center = CurrentLocation;
			// FVector DirectionToTarget = MoveTargetFrag.Center - CurrentLocation;
			MoveTargetFrag.Forward = DirectionToTarget.GetSafeNormal();
			MoveTargetFrag.DistanceToGoal = DirectionToTarget.Size();
		}
	}));
}

UETW_MassPathFollowInitializer::UETW_MassPathFollowInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassPathFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UETW_MassPathFollowInitializer::Register()
{
	check(ObservedType);
	UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
	UMassObserverRegistry::GetMutable().RegisterObserver(*FMassPathFollowingRequestTag::StaticStruct(), EMassObservedOperation::Add, GetClass());
}

void UETW_MassPathFollowInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UETW_MassNavigationSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddConstSharedRequirement<FMassPathFollowParams>();
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>();
}

void UETW_MassPathFollowInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UETW_MassNavigationSubsystem& NavigationSubsystem = Context.GetMutableSubsystemChecked<UETW_MassNavigationSubsystem>(World);
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&NavigationSubsystem, &World, this](FMassExecutionContext Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassPathFragment> PathList = Context.GetMutableFragmentView<FMassPathFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();

		const FMassPathFollowParams& PathFollowParams = Context.GetConstSharedFragment<FMassPathFollowParams>();

		const float DistMax = PathFollowParams.TempTestRandomNavigationRadius;
		const float DesiredSpeed = Context.GetConstSharedFragment<FMassMovementParameters>().MaxSpeed;
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			const FVector& InitialLocation = TransformList[EntityIndex].GetTransform().GetLocation();
			FMassPathFragment& PathFragment = PathList[EntityIndex];
			const FVector NewMoveToLocation = FVector(FMath::FRandRange(-DistMax, DistMax), FMath::FRandRange(-DistMax, DistMax), 0) + InitialLocation;
			
			// initialize desired speed:
			MoveTargetList[EntityIndex].DesiredSpeed = FMassInt16Real(DesiredSpeed);
			
			FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);

			// deferred:
			Context.Defer().PushCommand<FMassDeferredSetCommand>([&](const FMassEntityManager& Manager){
				NavigationSubsystem.EntityRequestNewPath(EntityHandle, PathFollowParams, InitialLocation, NewMoveToLocation, PathFragment);
			});
			// add tag for able to process path with UETW_MassPathFollowProcessor
			Context.Defer().AddTag<FMassPathFollowingProgressTag>(EntityHandle);
			Context.Defer().RemoveTag<FMassPathFollowingRequestTag>(EntityHandle);
			
		}
	});
}
