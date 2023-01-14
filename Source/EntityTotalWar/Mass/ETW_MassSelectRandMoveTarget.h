// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "ETW_MassSelectRandMoveTarget.generated.h"


// ---                              ||
// Set Rand FMassMoveTargetFragment ||
// ---                              \/


USTRUCT()
struct FMassSelectRandMoveTargetTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassSelectRandMoveTargetParams : public FMassSharedFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float MoveDistMax = 1024.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float TargetReachThreshold = 50.f;
};

/**
 * 
 */
UCLASS(meta = (DisplayName = "Select Rand Move Target XY"))
class ENTITYTOTALWAR_API UETW_MassSelectRandMoveTargetTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassSelectRandMoveTargetParams Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSelectRandMoveTargetProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UETW_MassSelectRandMoveTargetProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassSelectRandMoveTargetInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UETW_MassSelectRandMoveTargetInitializer();

protected:
	virtual void Register() override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};