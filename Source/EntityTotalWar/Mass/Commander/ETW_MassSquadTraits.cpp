// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadTraits.h"
#include "ETW_MassSquadSubsystem.h"
#include "MassCommanderComponent.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "Replication/Squad/ETW_MassSquadBubble.h"
#include "Replication/Squad/ETW_MassSquadReplicator.h"
#include "Replication/SquadUnits/ETW_MassSquadUnitsBubble.h"
#include "Replication/SquadUnits/ETW_MassSquadUnitsReplicator.h"
#include "VisualLogger/VisualLogger.h"

UETW_MassSquadReplicationTrait::UETW_MassSquadReplicationTrait()
{
	Params.BubbleInfoClass = AETW_MassSquadClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UETW_MassSquadsReplicator::StaticClass();
}

UETW_MassSquadUnitsReplicationTrait::UETW_MassSquadUnitsReplicationTrait()
{
	Params.BubbleInfoClass = AETW_MassSquadUnitClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UETW_MassSquadUnitsReplicator::StaticClass();
}


void UETW_MassSquadTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddFragment<FETW_MassSquadCommanderFragment>();
	BuildContext.AddFragment<FMassTargetLocationFragment>();  
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FETW_MassTeamFragment>();
	//BuildContext.AddFragment<FAgentRadiusFragment>();  // actually required for replication
	
	FETW_MassSquadSharedFragment SquadSharedFragment;
	
	uint32 SquadSharedFragmentHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(SquadSharedFragment));
	FSharedStruct SquadSharedFragmentStruct = EntityManager.GetOrCreateSharedFragmentByHash<FETW_MassSquadSharedFragment>(SquadSharedFragmentHash, SquadSharedFragment);
	BuildContext.AddSharedFragment(SquadSharedFragmentStruct);  // add there for correct archeotype composition descriptor, Change actual shared fragment value in custom entities spawning function

	FETW_MassSquadParams Params;
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

void UETW_MassSquadTrait::ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	//todo duno check something
}

void UETW_MassSquadUnitTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassTargetLocationFragment>();
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FETW_MassUnitFragment>();  
	BuildContext.AddFragment<FETW_MassTeamFragment>(); 
	
	FETW_MassSquadSharedFragment SquadSharedFragment;
	SquadSharedFragment.Formation = Params.DefaultFormation;
	
	uint32 SquadSharedFragmentHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(SquadSharedFragment));
	FSharedStruct SquadSharedFragmentStruct = EntityManager.GetOrCreateSharedFragmentByHash<FETW_MassSquadSharedFragment>(SquadSharedFragmentHash, SquadSharedFragment);
	BuildContext.AddSharedFragment(SquadSharedFragmentStruct);  // add there for correct archeotype composition descriptor, Change actual shared fragment value in custom entities spawning function

	// Copy the asset reference ID
	Params.SquadEntityTemplate = SquadEntityTemplate;
	const FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);
}

void UETW_MassSquadUnitTrait::ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	UMassEntityConfigAsset* SquadConfig = SquadEntityTemplate.LoadSynchronous();
	if (!IsValid(SquadConfig))
	{
		UE_VLOG(this, ETW_Mass, Error, TEXT("SquadUnitTrait invalid SquadEntityTemplate"));
		return;
	}
	TConstArrayView<UMassEntityTraitBase*> SquadConfig_Traits = SquadConfig->GetConfig().GetTraits();

	bool bHasSquadTrait = SquadConfig_Traits.ContainsByPredicate([](const UMassEntityTraitBase* Trait)
	{
		return IsValid(Cast<UETW_MassSquadTrait>(Trait));
	});
	if (!bHasSquadTrait)
	{
		UE_VLOG(this, ETW_Mass, Error, TEXT("SquadEntityTemplate should contain UETW_MassSquadTrait"));
	}

	bool bHasSquadRepTrait = SquadConfig_Traits.ContainsByPredicate([](const UMassEntityTraitBase* Trait)
	{
	return IsValid(Cast<UETW_MassSquadReplicationTrait>(Trait));
	});
	if (!bHasSquadRepTrait)
	{
		UE_VLOG(this, ETW_Mass, Error, TEXT("SquadEntityTemplate should contain UETW_MassSquadReplicationTrait"));
	}
	
}