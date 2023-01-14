// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "ETW_MassFragments.generated.h"

USTRUCT()
struct FMassTargetLocationFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Target;
};

USTRUCT()
struct FMassInitialLocationFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Location;
};
