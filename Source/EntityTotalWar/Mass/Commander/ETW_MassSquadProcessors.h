// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassSquadFragments.h"
#include "ETW_MassSquadProcessors.generated.h"

UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FETW_MassSquadSharedFragment Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UETW_MassSquadProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};