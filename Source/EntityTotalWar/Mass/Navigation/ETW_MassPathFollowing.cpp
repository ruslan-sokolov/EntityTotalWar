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
#include "ETW_MassNavigationSubsystem.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"

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

	// updates FMassMoveTargetFragment with new path point
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([World](FMassExecutionContext& Context){

		const FMassPathFollowParams& PathFollowParams = Context.GetConstSharedFragment<FMassPathFollowParams>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassPathFragment> PathFragList = Context.GetMutableFragmentView<FMassPathFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetFragList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassTargetLocationFragment> TargetLocationList = Context.GetMutableFragmentView<FMassTargetLocationFragment>();

		UETW_MassNavigationSubsystem* NavigationSubsystem = Context.GetMutableSubsystem<UETW_MassNavigationSubsystem>(World);

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FMassMoveTargetFragment& MoveTargetFrag = MoveTargetFragList[EntityIndex];
			FMassPathFragment& PathFrag = PathFragList[EntityIndex];
			FVector& TargetLocation = TargetLocationList[EntityIndex].Target;
			const FVector CurrentLocation = TransformList[EntityIndex].GetTransform().GetLocation();

			// update MoveTargetFragment
			FVector DirectionToTarget = TargetLocation - CurrentLocation;
			MoveTargetFrag.Center = CurrentLocation;
			// FVector DirectionToTarget = MoveTargetFrag.Center - CurrentLocation;
			MoveTargetFrag.Forward = DirectionToTarget.GetSafeNormal();
			MoveTargetFrag.DistanceToGoal = DirectionToTarget.Size();
			
			if (MoveTargetFrag.GetCurrentAction() == EMassMovementAction::Stand || MoveTargetFrag.DistanceToGoal <= PathFollowParams.SlackRadius)
			{
				FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);
				
				// deferred:
				Context.Defer().PushCommand<FMassDeferredChangeCompositionCommand>([NavigationSubsystem, EntityHandle, &PathFrag, &MoveTargetFrag, &TargetLocation, World, CurrentLocation, &PathFollowParams](FMassEntityManager& Manager){
					
					if (NavigationSubsystem->EntityExtractNextPathPoint(EntityHandle, PathFrag))
					{
						// set new target from path list
						MoveTargetFrag.CreateNewAction(EMassMovementAction::Move, *World);
						MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand;
						TargetLocation = PathFrag.GetPathPoint();
					}
					else
					{
						// add tag for able to process path with UETW_MassPathFollowProcessor
						//Manager.AddTagToEntity(EntityHandle, FMassPathFollowingRequestTag::StaticStruct());
						//Manager.RemoveTagFromEntity(EntityHandle, FMassPathFollowingProgressTag::StaticStruct());
						const float DistMax = PathFollowParams.TempTestRandomNavigationRadius;
						const FVector NewMoveToLocation = FVector(FMath::FRandRange(-DistMax, DistMax), FMath::FRandRange(-DistMax, DistMax), 0) + CurrentLocation;
						NavigationSubsystem->EntityRequestNewPath(EntityHandle, PathFollowParams, CurrentLocation, NewMoveToLocation, PathFrag);

						if (NavigationSubsystem->EntityExtractNextPathPoint(EntityHandle, PathFrag))
						{
							// set new target from path list
							MoveTargetFrag.CreateNewAction(EMassMovementAction::Move, *World);
							MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand;
							TargetLocation = PathFrag.GetPathPoint();
						}
					}
				});
			}
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
	EntityQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddConstSharedRequirement<FMassPathFollowParams>();
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>();
}

void UETW_MassPathFollowInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = EntityManager.GetWorld();
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassPathFragment> PathList = Context.GetMutableFragmentView<FMassPathFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassTargetLocationFragment> TargetLocationList = Context.GetMutableFragmentView<FMassTargetLocationFragment>();

		const FMassPathFollowParams& PathFollowParams = Context.GetConstSharedFragment<FMassPathFollowParams>();

		const float DistMax = PathFollowParams.TempTestRandomNavigationRadius;
		const float DesiredSpeed = Context.GetConstSharedFragment<FMassMovementParameters>().MaxSpeed;
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			const FVector& InitialLocation = TransformList[EntityIndex].GetTransform().GetLocation();
			
			// initialize desired speed:
			MoveTargetList[EntityIndex].DesiredSpeed = FMassInt16Real(DesiredSpeed);
			MoveTargetList[EntityIndex].DistanceToGoal = 0.f;
			TargetLocationList[EntityIndex].Target = InitialLocation;
			
			FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);

			// deferred:
			Context.Defer().PushCommand<FMassDeferredChangeCompositionCommand>([=](FMassEntityManager& Manager){
				//NavigationSubsystem->EntityRequestNewPath(EntityHandle, PathFollowParams, InitialLocation, NewMoveToLocation, PathFragment);

				// add tag for able to process path with UETW_MassPathFollowProcessor
				Manager.AddTagToEntity(EntityHandle, FMassPathFollowingProgressTag::StaticStruct());
				Manager.RemoveTagFromEntity(EntityHandle, FMassPathFollowingRequestTag::StaticStruct());
			});

			
		}
	});
}
