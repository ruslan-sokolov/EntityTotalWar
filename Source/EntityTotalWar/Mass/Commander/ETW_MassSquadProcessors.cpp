// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadProcessors.h"

void UETW_MassSquadTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
}

UETW_MassSquadProcessor::UETW_MassSquadProcessor()
{
}

void UETW_MassSquadProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
}

void UETW_MassSquadProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Super::Execute(EntityManager, Context);
}
