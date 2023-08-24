// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassCollisionProcessors.h"
#include "MassObserverRegistry.h"
#include "ETW_MassCollisionTypes.h"
#include "MassCommonFragments.h"
#include "ETW_MassCollisionSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

UETW_MassCollisionObserver::UETW_MassCollisionObserver()
	: CapsuleFragmentAddQuery(*this), CapsuleFragmentRemoveQuery(*this)
{
	ObservedType = FETW_MassCopsuleFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UETW_MassCollisionObserver::Register()
{
	check(ObservedType);
	UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
	UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, EMassObservedOperation::Remove, GetClass());
}

void UETW_MassCollisionObserver::ConfigureQueries()
{
	CapsuleFragmentAddQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	CapsuleFragmentAddQuery.AddRequirement<FETW_MassCopsuleFragment>(EMassFragmentAccess::ReadWrite);
	CapsuleFragmentAddQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	CapsuleFragmentAddQuery.AddConstSharedRequirement<FETW_MassCapsuleCollisionParams>();
	CapsuleFragmentAddQuery.AddSubsystemRequirement<UETW_MassCollisionSubsystem>(EMassFragmentAccess::ReadWrite);

	CapsuleFragmentRemoveQuery.AddRequirement<FETW_MassCopsuleFragment>(EMassFragmentAccess::None, EMassFragmentPresence::None);
	CapsuleFragmentRemoveQuery.AddSubsystemRequirement<UETW_MassCollisionSubsystem>(EMassFragmentAccess::ReadWrite);

}

void UETW_MassCollisionObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = EntityManager.GetWorld();

	CapsuleFragmentAddQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext Context)
	{
		UETW_MassCollisionSubsystem* CollisionSubsystem = Context.GetMutableSubsystem<UETW_MassCollisionSubsystem>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FETW_MassCopsuleFragment> CapsuleList = Context.GetMutableFragmentView<FETW_MassCopsuleFragment>();
		const TArrayView<FAgentRadiusFragment> AgentRadiusList = Context.GetMutableFragmentView<FAgentRadiusFragment>();
		const FETW_MassCapsuleCollisionParams& CollisionParams = Context.GetConstSharedFragment<FETW_MassCapsuleCollisionParams>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			if (AgentRadiusList.Num() > 0)
			{
				AgentRadiusList[EntityIndex].Radius = CollisionParams.CapsuleRadius;
			}
			
			FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);
			const FTransform& Transform = TransformList[EntityIndex].GetTransform();
			FETW_MassCopsuleFragment& CapsuleFragment = CapsuleList[EntityIndex];
			
			CollisionSubsystem->CreateCapsuleEntity(CapsuleFragment, EntityHandle, Transform, CollisionParams);
			
		}
	});

	CapsuleFragmentRemoveQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext Context)
	{
		UETW_MassCollisionSubsystem* CollisionSubsystem = Context.GetMutableSubsystem<UETW_MassCollisionSubsystem>();
		
		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FMassEntityHandle EntityHandle = Context.GetEntity(EntityIndex);
			CollisionSubsystem->DestroyCapsuleEntity(EntityHandle);
		}
	});
}

void UETW_MassCapsuleCollisionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FETW_MassCopsuleFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}
