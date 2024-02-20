// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassMoveToCursor.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassObserverRegistry.h"
#include "MassSimulationLOD.h"
#include "../Common/Fragments/ETW_MassFragments.h"
#include "MassCommanderComponent.h"
#include "Kismet/GameplayStatics.h"
#include <Translators/MassCharacterMovementTranslators.h>
#include "GameFramework/CharacterMovementComponent.h"
#include "MassCommands.h"
#include "MassEntityView.h"
#include "DrawDebugHelpers.h"

namespace ETW
{
	bool bDebugMoveToCursor = false;
	FAutoConsoleVariableRef CVarbDebugReplication(TEXT("etw.debug.moveToCursor"), bDebugMoveToCursor, TEXT("Draw Move To Cursor"), ECVF_Cheat);

#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR

	void DebugMoveToCursorAgent(FMassEntityHandle Entity, const FMassEntityManager& EntityManager, float Radius = 100.f)
	{
		if (!bDebugMoveToCursor)
		{
			return;
		}

		static const FVector DebugCylinderHeight = FVector(0.f, 0.f, 200.f);

		const FMassEntityView EntityView(EntityManager, Entity);

		const FTransformFragment& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
		const FMassMoveToCursorCommanderFragment& CommanderFragment = EntityView.GetFragmentData<FMassMoveToCursorCommanderFragment>();
		const FMassMoveToCursorFragment& MoveToCursorFragment = EntityView.GetFragmentData<FMassMoveToCursorFragment>();
		const FMassVelocityFragment& VelocityFragment = EntityView.GetFragmentData<FMassVelocityFragment>();

		const FVector& Pos = TransformFragment.GetTransform().GetLocation();
		const FVector& Target = MoveToCursorFragment.Target;
		const FVector& Velocity = VelocityFragment.Value;



		FAgentRadiusFragment* RadiusFragment = EntityView.GetFragmentDataPtr<FAgentRadiusFragment>();
		if (RadiusFragment)
		{
			Radius = RadiusFragment->Radius;
		}

		const UWorld* World = EntityManager.GetWorld();
		float WorldDeltaSec = World->GetDeltaSeconds();
		
		DrawDebugSphere(World, Pos, Radius, 16, FColor::Green, false, WorldDeltaSec * 2);
		DrawDebugSphere(World, Target, Radius, 16, FColor::Turquoise, false, WorldDeltaSec * 2);
		DrawDebugDirectionalArrow(World, Pos, Pos + Velocity, 30.f, FColor::Turquoise, false, WorldDeltaSec * 2);
	}
#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
}

void UETW_MassMoveToCursorTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	BuildContext.RequireFragment<FMassVelocityFragment>();
	//BuildContext.RequireFragment<FMassForceFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	//BuildContext.RequireFragment<FMassMovementParameters>();
	
	BuildContext.AddFragment<FMassMoveToCursorFragment>();
	BuildContext.AddFragment<FMassMoveToCursorCommanderFragment>();
	BuildContext.AddFragment<FCharacterMovementComponentWrapperFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

UETW_MassMoveToCursorProcessor::UETW_MassMoveToCursorProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	//ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UETW_MassMoveToCursorProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	//EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveToCursorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveToCursorCommanderFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FCharacterMovementComponentWrapperFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	EntityQuery.AddConstSharedRequirement<FMassMoveToCursorParams>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::Optional);
}

void UETW_MassMoveToCursorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([](FMassExecutionContext& Context)
		{
			const FMassMoveToCursorParams MoveToCursorParams = Context.GetConstSharedFragment<FMassMoveToCursorParams>();
			const float ArriveSlackRadius = MoveToCursorParams.TargetReachThreshold;
			const FMassMovementParameters* MovementParams = Context.GetConstSharedFragmentPtr<FMassMovementParameters>();
			
			const TConstArrayView<FCharacterMovementComponentWrapperFragment> CharacterMovementList = Context.GetFragmentView<FCharacterMovementComponentWrapperFragment>();
			bool bHasMovementComponent = CharacterMovementList.Num() > 0;

			const float Speed = MovementParams ? MovementParams->MaxSpeed : MoveToCursorParams.SpeedFallbackValue;
		
			const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
			//const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
			const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FMassMoveToCursorFragment> MoveToCursorList = Context.GetMutableFragmentView<FMassMoveToCursorFragment>();
			const TArrayView<FMassMoveToCursorCommanderFragment> MoveToCursorCommanderList = Context.GetMutableFragmentView<FMassMoveToCursorCommanderFragment>();

			FMassEntityManager& EntityManager = Context.GetEntityManagerChecked();

			for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
			{
				FMassMoveToCursorCommanderFragment& CommanderFragment = MoveToCursorCommanderList[EntityIndex];
				//if (!CommanderFragment.CommanderComp)
				{
					bool FoundCommanderComp = false;
					if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(EntityManager.GetWorld(), 0))
					{
						if (UMassCommanderComponent* CommanderComp = PlayerPawn->GetComponentByClass<UMassCommanderComponent>())
						{
							FoundCommanderComp = true;
							CommanderFragment = FMassMoveToCursorCommanderFragment(CommanderComp);
						};
					}

					if (!FoundCommanderComp)
					{
						continue;
					}
				}

				float EntitySpeed = Speed;
				if (bHasMovementComponent && CharacterMovementList[EntityIndex].Component.IsValid())
				{
					EntitySpeed = CharacterMovementList[EntityIndex].Component->GetMaxSpeed();
				}
				
				FMassMoveToCursorFragment& MoveToCursor = MoveToCursorList[EntityIndex];

				//FVector& Force = ForceList[EntityIndex].Value;
				FVector& Velocity = VelocityList[EntityIndex].Value;
				const FTransform& Transform = TransformList[EntityIndex].GetTransform();
				const FVector& CurrentLocation = Transform.GetLocation();
				FVector& TargetLocation = MoveToCursor.Target;

				MoveToCursor.Timer += Context.GetDeltaTimeSeconds();
				
				if (MoveToCursor.Timer >= 1.f || FVector::Dist(TargetLocation, CurrentLocation) <= ArriveSlackRadius || TargetLocation.IsZero())
				{
					Velocity = FVector::Zero();
					MoveToCursor.Timer = 0.f;

					MoveToCursor.Target = CommanderFragment.CommanderComp->GetCommandLocation();
					//// deferred:
					//Context.Defer().PushCommand<FMassDeferredChangeCompositionCommand>([&MoveToCursor, CommanderComp](FMassEntityManager& Manager){
					//	//NavigationSubsystem->EntityRequestNewPath(EntityHandle, PathFollowParams, InitialLocation, NewMoveToLocation, PathFragment);
					//	MoveToCursor.Target = CommanderComp->GetCommandLocation();
					//});

				}
				else
				{
					// update MoveTargetFragment
					FVector Direction = TargetLocation - CurrentLocation;
					Direction.Z = 0.f;
					Direction.Normalize();
					Velocity = Direction * Speed;
					//Transform.SetRotation(Direction.ToOrientationQuat());
				}

				#if WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
				ETW::DebugMoveToCursorAgent(Context.GetEntity(EntityIndex), EntityManager);
				#endif // WITH_MASSGAMEPLAY_DEBUG && WITH_EDITOR
				
			}
		}));
}

// Initializer (Observer)

UETW_MassMoveToCursorInitializer::UETW_MassMoveToCursorInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassMoveToCursorFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UETW_MassMoveToCursorInitializer::Register()
{
	check(ObservedType);
	// DEPRECATED
	//UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
}

void UETW_MassMoveToCursorInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassMoveToCursorFragment>(EMassFragmentAccess::ReadWrite);
}

void UETW_MassMoveToCursorInitializer::Execute(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext Context)
	{
		const TArrayView<FMassMoveToCursorFragment> MoveToCursorList = Context.GetMutableFragmentView<FMassMoveToCursorFragment>();

		TConstArrayView<FMassEntityHandle> Entities = Context.GetEntities();

		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (UMassCommanderComponent* CommanderComp = Cast<UMassCommanderComponent>(PlayerPawn->GetComponentByClass(UMassCommanderComponent::StaticClass())))
			{
				FMassMoveToCursorCommanderFragment CommanderFrag;
				CommanderFrag.CommanderComp = CommanderComp;

				for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
				{
					// this is actually not very good to create commander fragment copy for each entity
					Context.Defer().PushCommand<FMassCommandAddFragmentInstances>(Context.GetEntity(EntityIndex), CommanderFrag);

					FMassMoveToCursorFragment MoveToCursorFrag = MoveToCursorList[EntityIndex];
					MoveToCursorFrag.Timer = 0.f;
				}
			};
		}
	});

}