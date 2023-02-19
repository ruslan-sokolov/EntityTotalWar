// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassObserverProcessor.h"
#include "ETW_MassCollisionTypes.h"

#include "ETW_MassCollisionProcessors.generated.h"


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassCollisionObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UETW_MassCollisionObserver();

protected:
	virtual void Register() override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	FMassEntityQuery CapsuleFragmentAddQuery;
	FMassEntityQuery CapsuleFragmentRemoveQuery;
};

/**
 * 
 */
UCLASS(meta = (DisplayName = "ETW Capsule Collision"))
class ENTITYTOTALWAR_API UETW_MassCapsuleCollisionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Collision")
	FETW_MassCapsuleCollisionParams Params;
};