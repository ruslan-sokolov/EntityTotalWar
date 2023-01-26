// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "ETW_MassNavigationTypes.generated.h"

namespace UE::Mass::Signals
{
	const FName RequestNewPath = FName(TEXT("RequestNewPath"));
	const FName ReachedPathEnd = FName(TEXT("ReachedPathEnd"));
}

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
	friend class UETW_MassNavigationSubsystem;
	
	GENERATED_BODY()

	const FVector& GetPathPoint() const { return PathPoint; }

protected:
	FVector PathPoint;
	uint16 NextPathVertIdx = 0;
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassPathFollowParams : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FNavAgentProperties NavAgentProps = FNavAgentProperties::DefaultProperties;
	
	UPROPERTY(EditAnywhere, Category = "Navigation", meta = (ClampMin = "0", ForceUnits="cm"))
	float SlackRadius = 30.f;

	UPROPERTY(EditAnywhere, Category = "Navigation", meta = (ClampMin = "0", ForceUnits="cm"))
	float TempTestRandomNavigationRadius = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Navigation", meta = (ClampMin = "0", ForceUnits="cm"))
	bool bAllowPartialPath = true;

	// todo config NavPoints step
};