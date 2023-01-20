// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "NavigationSystem/Public/NavigationData.h"
#include "ETW_MassPathFollowing.generated.h"


// ---                              ||
// Set Rand FMassMoveTargetFragment ||
// ---                              \/

USTRUCT()
struct FMassPathFollowingProgressTag : public FMassTag
{
	GENERATED_BODY();
};

USTRUCT()
struct FMassPathFollowingRequestTag : public FMassTag
{
	GENERATED_BODY();
};

// todo: for async
USTRUCT()
struct FMassPathFollowingRespondTag : public FMassTag
{
	GENERATED_BODY();
};

// todo: use global squad ai controller for avoiding massive code duplication and bugs
// todo: move from fragment to subsystem (heavy to store in entity) or make shared non const fragment
USTRUCT()
struct FMassPathFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FNavigationPath NavPath;
	
	uint32 NextPathVertIdx = 0;

	void SetNavPath(const FNavigationPath& InNavPath)
	{
		NavPath = InNavPath;
		NextPathVertIdx = 0;
	}
	
	bool GetNextPathPoint(FVector& OutPathPoint)
	{
		bool bSuccess = false;
	
		TArray<FNavPathPoint>& PathPoints = NavPath.GetPathPoints();
		if (PathPoints.IsValidIndex(NextPathVertIdx))
		{
			OutPathPoint = PathPoints[NextPathVertIdx++].Location;
			bSuccess = true;
		}

		return bSuccess;
	}
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassPathFollowParams : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FNavAgentProperties NavAgentProps = FNavAgentProperties::DefaultProperties;
	
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float SlackRadius = 30.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float TempTestRandomNavigationRadius = 3000.f;

	// todo config NavPoints step
};

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