// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntitySubsystem.h"
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
struct FMassMovementTargetPosFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Target;
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

USTRUCT()
struct FMassMoveToTargetTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassRandMoveToParams : public FMassSharedFragment
{
	GENERATED_BODY()

	/** AcceptanceRadius cm */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float AcceptanceRadius = 200.f;

	/** Force cm / s^2 */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm / s^2"))
	float ForceMagnitude = 200.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float MoveDistMax = 400.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float Z = 50.f;
};


/**
 * 
 */
UCLASS(meta = (DisplayName = "Simple Random Movement"))
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


/**
 * 
 */
UCLASS(meta = (DisplayName = "Select Random Move To"))
class ENTITYTOTALWAR_API UETW_MassSelectRandomMoveToTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassRandMoveToParams Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MoveToTargetProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UETW_MoveToTargetProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
