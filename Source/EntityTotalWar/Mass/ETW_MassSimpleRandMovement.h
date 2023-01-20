// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "ETW_MassSimpleRandMovement.generated.h"


USTRUCT()
struct FMassSimpleRandMovementTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassMassSimpleRandMovementParams : public FMassSharedFragment
{
	GENERATED_BODY()
	
	/** Speed (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm/s"))
	float Speed = 200.f;

	/** AcceptanceRadius cm */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float AcceptanceRadius = 20.f;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float MoveDistMax = 400.f;
};


/**
 * 
 */
UCLASS(meta = (DisplayName = "ETW Simple Random Movement"))
class ENTITYTOTALWAR_API UMassSimpleRandMovementTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassMassSimpleRandMovementParams Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UMassSimpleRandMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassSimpleRandMovementProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
