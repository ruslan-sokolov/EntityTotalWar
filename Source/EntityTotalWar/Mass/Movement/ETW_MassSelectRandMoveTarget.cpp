// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSelectRandMoveTarget.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassObserverRegistry.h"
#include "MassSimulationLOD.h"
#include "Mass/Common/Fragments/ETW_MassFragments.h"


void UETW_MassSelectRandMoveTargetTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	BuildContext.RequireFragment<FMassMoveTargetFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
	
	BuildContext.AddFragment<FMassTargetLocationFragment>();
	BuildContext.AddFragment<FMassInitialLocationFragment>();
	BuildContext.AddTag<FMassSelectRandMoveTargetTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

UETW_MassSelectRandMoveTargetProcessor::UETW_MassSelectRandMoveTargetProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UETW_MassSelectRandMoveTargetProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FMassTargetLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassInitialLocationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassSelectRandMoveTargetTag>(EMassFragmentPresence::All);

	EntityQuery.AddConstSharedRequirement<FMassSelectRandMoveTargetParams>(EMassFragmentPresence::All);
	
	// off lod optimization
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);

	// utilize variable tick rate if present
	EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
	//EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
}

void UETW_MassSelectRandMoveTargetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([&EntityManager](FMassExecutionContext& Context)
		{
			const UWorld* World = EntityManager.GetWorld();

			const float MoveDistMax = Context.GetConstSharedFragment<FMassSelectRandMoveTargetParams>().MoveDistMax;
			const float ArriveSlackRadius = Context.GetConstSharedFragment<FMassSelectRandMoveTargetParams>().TargetReachThreshold;
		
			const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
			const TArrayView<FMassTargetLocationFragment> TargetLocationList = Context.GetMutableFragmentView<FMassTargetLocationFragment>();
			const TConstArrayView<FMassInitialLocationFragment> InitialLocationList = Context.GetFragmentView<FMassInitialLocationFragment>();
			const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

			for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
			{
				FMassMoveTargetFragment& MoveTargetFrag = MoveTargetList[EntityIndex];
				FVector& TargetLocation = TargetLocationList[EntityIndex].Target;
				const FVector& InitialLocation = InitialLocationList[EntityIndex].Location;
				const FTransform& Transform = TransformList[EntityIndex].GetTransform();

				FVector CurrentLocation = Transform.GetLocation();

				if (MoveTargetFrag.GetCurrentAction() == EMassMovementAction::Stand || MoveTargetFrag.DistanceToGoal <= ArriveSlackRadius)
				{
					// set new target
					MoveTargetFrag.CreateNewAction(EMassMovementAction::Move, *World);
					MoveTargetFrag.IntentAtGoal = EMassMovementAction::Stand;
					const FVector NewMoveToLocation = FVector(FMath::FRandRange(-MoveDistMax, MoveDistMax), FMath::FRandRange(-MoveDistMax, MoveDistMax), 0) + InitialLocation;
					TargetLocation = NewMoveToLocation;
					// MoveTargetFrag.Center = NewMoveToLocation;
				}
				
				// update MoveTargetFragment
				FVector DirectionToTarget = TargetLocation - CurrentLocation;
				MoveTargetFrag.Center = CurrentLocation;
				// FVector DirectionToTarget = MoveTargetFrag.Center - CurrentLocation;
				MoveTargetFrag.Forward = DirectionToTarget.GetSafeNormal();
				MoveTargetFrag.DistanceToGoal = DirectionToTarget.Size();

				//GEngine->AddOnScreenDebugMessage(1252, 1.f, FColor::Red, " " + Transform.GetLocation().ToString());
			}
		}));
}

// Initializer (Observer)

UETW_MassSelectRandMoveTargetInitializer::UETW_MassSelectRandMoveTargetInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassInitialLocationFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UETW_MassSelectRandMoveTargetInitializer::Register()
{
	check(ObservedType);
	UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
}

void UETW_MassSelectRandMoveTargetInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassInitialLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassSelectRandMoveTargetTag>(EMassFragmentPresence::All);

	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	//EntityQuery.AddConstSharedRequirement<FMassSelectRandMoveTargetParams>(EMassFragmentPresence::All);
}

void UETW_MassSelectRandMoveTargetInitializer::Execute(FMassEntityManager& EntityManager,
                                                       FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext Context)
	{
		const float DesiredSpeed = Context.GetConstSharedFragment<FMassMovementParameters>().MaxSpeed;
		//const float TargetReachThreshold = Context.GetConstSharedFragment<FMassSelectRandMoveTargetParams>().TargetReachThreshold;

		const TArrayView<FMassInitialLocationFragment> InitialLocationList = Context.GetMutableFragmentView<FMassInitialLocationFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			// initialize initial location:
			InitialLocationList[EntityIndex].Location = TransformList[EntityIndex].GetTransform().GetLocation();

			// initialize desired speed:
			MoveTargetList[EntityIndex].DesiredSpeed = FMassInt16Real(DesiredSpeed);
			
			// initialize target reach threshold:
            // MoveTargetList[EntityIndex].SlackRadius = TargetReachThreshold;
		}
	});
}

