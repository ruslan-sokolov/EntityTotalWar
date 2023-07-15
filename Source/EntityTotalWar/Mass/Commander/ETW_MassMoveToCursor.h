// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassCommonFragments.h"
#include "ETW_MassMoveToCursor.generated.h"


USTRUCT()
struct FMassMoveToCursorCommanderFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TObjectPtr<class UMassCommanderComponent> CommanderComp;

	FMassMoveToCursorCommanderFragment() = default;

	explicit FMassMoveToCursorCommanderFragment(UMassCommanderComponent* CommanderComponent)
	{
		CommanderComp = CommanderComponent;
	}
};

USTRUCT()
struct FMassMoveToCursorFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Target = FVector::ZeroVector;
	float Timer = 0.f;
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassMoveToCursorParams : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float TargetReachThreshold = 50.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SpeedFallbackValue = 450.f;
};

/**
 * 
 */
UCLASS(meta = (DisplayName = "ETW Move To Cursor"))
class ENTITYTOTALWAR_API UETW_MassMoveToCursorTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassMoveToCursorParams Params;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassMoveToCursorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UETW_MassMoveToCursorProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassMoveToCursorInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UETW_MassMoveToCursorInitializer();

protected:
	virtual void Register() override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};