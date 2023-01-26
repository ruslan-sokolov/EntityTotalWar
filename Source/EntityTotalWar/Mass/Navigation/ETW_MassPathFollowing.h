// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "ETW_MassNavigationTypes.h"
#include "ETW_MassPathFollowing.generated.h"


/**
 * 
 */
UCLASS(meta = (DisplayName = "ETW Path Follow"))
class ENTITYTOTALWAR_API UETW_PathFollowTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassPathFollowParams Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassPathFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UETW_MassPathFollowProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassPathFollowInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UETW_MassPathFollowInitializer();

protected:
	virtual void Register() override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};