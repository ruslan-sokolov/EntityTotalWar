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
	FETW_MassSquadParams Params;
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

	FMassEntityQuery EntityQuery_Squad;
	FMassEntityQuery EntityQuery_Unit;
};

UCLASS()
class ENTITYTOTALWAR_API UMassSquadPostSpawnProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassSquadPostSpawnProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Unit;
};


// add squad spawn post process initializer with payload?