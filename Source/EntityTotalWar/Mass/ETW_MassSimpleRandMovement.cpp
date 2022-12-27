// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSimpleRandMovement.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationLOD.h"
#include "Kismet/KismetSystemLibrary.h"


void UMassSimpleRandMovementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FMassMovementTargetPosFragment>();
	BuildContext.AddTag<FMassSimpleRandMovementTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

UMassSimpleRandMovementProcessor::UMassSimpleRandMovementProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
}

void UMassSimpleRandMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMovementTargetPosFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassSimpleRandMovementTag>(EMassFragmentPresence::All);

	EntityQuery.AddConstSharedRequirement<FMassMassSimpleRandMovementParams>(EMassFragmentPresence::All);

	EntityQuery.AddRequirement<FMassSimulationVariableTickFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassSimpleRandMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([this](FMassExecutionContext& Context)
		{
			const float Speed = Context.GetConstSharedFragment<FMassMassSimpleRandMovementParams>().Speed;
			const float AcceptanceRadius = Context.GetConstSharedFragment<FMassMassSimpleRandMovementParams>().AcceptanceRadius;
			const float MoveDistMax = Context.GetConstSharedFragment<FMassMassSimpleRandMovementParams>().MoveDistMax;

			const TConstArrayView<FMassSimulationVariableTickFragment> SimVariableTickList = Context.GetFragmentView<FMassSimulationVariableTickFragment>();
			const bool bHasVariableTick = (SimVariableTickList.Num() > 0);
			const float WorldDeltaTime = Context.GetDeltaTimeSeconds();
		
			const TArrayView<FMassVelocityFragment> VelocitiesList = Context.GetMutableFragmentView<FMassVelocityFragment>();
			const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
			const TArrayView<FMassMovementTargetPosFragment> TargetList = Context.GetMutableFragmentView<FMassMovementTargetPosFragment>();
			

			for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
			{
				FTransform& Transform = TransformList[EntityIndex].GetMutableTransform();
				FVector& MoveTarget = TargetList[EntityIndex].Target;

				FVector CurrentLocation = Transform.GetLocation();
				FVector TargetVector = MoveTarget - CurrentLocation;

				FVector& Velocity = VelocitiesList[EntityIndex].Value;
				Velocity = TargetVector.GetSafeNormal() * Speed;

				const float DeltaTime = bHasVariableTick ? SimVariableTickList[EntityIndex].DeltaTime : WorldDeltaTime;
				
				if (FMath::Abs((CurrentLocation - MoveTarget).Size()) <= AcceptanceRadius || MoveTarget.IsZero())
				{
					// set new target
					MoveTarget = FVector(FMath::FRandRange(-MoveDistMax, MoveDistMax), FMath::FRandRange(-MoveDistMax, MoveDistMax), 200.f);
				}
				else
				{
					// move to target
					Transform.SetLocation(CurrentLocation + Velocity * DeltaTime);
				}

				// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, "dt: " + FString::SanitizeFloat(DeltaTime) + " " + Transform.GetLocation().ToString());
				
			}
		}));

}
