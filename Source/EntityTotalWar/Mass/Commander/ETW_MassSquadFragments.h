// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassTypes.h"
#include "ETW_MassSquadFragments.generated.h"

/*
squad
	squad shared (initialized)
	team frag
	transform frag (target location)
	


	
unit
	unit fragment (initialized)
	squad shared (initialized)
	
	
squad shared
	int32 squad index (initialized)
	int32 squad target index
	
	formation
	
	
unit fragment
	int32 unit index (initialized)
	formation offset
	
team fragment
	int8 team index
	
	
squad subsystem
	squad manager
	
	
squad manager
	entity handle get squad entity (squad index)
	array<entityhandle> get units entity (squad index)
	
	get squad entity from unit id (unit index)
	
	

struct formation
	FVector2D Box
	
	
-------------------------processors:

*/

USTRUCT()
struct FMassSquadSpawnAuxData
{
	GENERATED_BODY()

	TWeakObjectPtr<class UMassCommanderComponent> CommanderComp;
	int8 TeamIndex;
	FTransform SquadInitialTransform;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassUnitFragment : public FMassFragment
{
	GENERATED_BODY()

	uint32 UnitIndex;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassTeamFragment : public FMassFragment
{
	GENERATED_BODY()

	int8 TeamIndex;
};


UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EETW_FormationMovementMode : uint8
{
	NONE	   = 0 UMETA(Hidden),
	March = 1 << 0,
	Advance = 1 << 1,
	Sneak = 1 << 2
};
ENUM_CLASS_FLAGS(EETW_FormationMovementMode);

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EETW_FormationType : uint8
{
	NONE	   = 0 UMETA(Hidden),
	Rectangle = 1 << 0,
	Box = 1 << 1,
	Circle = 1 << 2,
	Ring = 1 << 3,
	Wedge = 1 << 4,

	Custom = 1 << 7 
};
ENUM_CLASS_FLAGS(EETW_FormationType);

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EETW_FormationDensity : uint8
{
	NONE	   = 0 UMETA(Hidden),
	Normal = 1 << 0,
	Loose = 1 << 1,
	Tight = 1 << 2
};
ENUM_CLASS_FLAGS(EETW_FormationDensity);


USTRUCT(BlueprintType)
struct FETW_MassFormation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int16 Length = 16;

	UPROPERTY(EditAnywhere)
	FMassInt16Real DencityInterval = FMassInt16Real(100.f);
	
	UPROPERTY(EditAnywhere)
	FMassInt16Real Speed = FMassInt16Real(160.f);

	UPROPERTY(EditAnywhere)
	EETW_FormationType FormationType = EETW_FormationType::Rectangle;

	UPROPERTY(EditAnywhere)
	EETW_FormationDensity FormationDensity = EETW_FormationDensity::Normal;
	
	UPROPERTY(EditAnywhere)
	EETW_FormationMovementMode FormationMovementMode = EETW_FormationMovementMode::March;
	
};

USTRUCT()
struct FETW_MassSquadCommanderFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()
	TObjectPtr<class UMassCommanderComponent> CommanderComp;

	FETW_MassSquadCommanderFragment() = default;

	explicit FETW_MassSquadCommanderFragment(UMassCommanderComponent* CommanderComponent)
	{
		CommanderComp = CommanderComponent;
	}
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	uint32 SquadIndex = 0;
	uint32 TargetSquadIndex = 0;
	

	FETW_MassFormation Formation;
};


USTRUCT(BlueprintType)
struct ENTITYTOTALWAR_API FETW_MassSquadParams : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FETW_MassFormation DefaultFormation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = EETW_FormationType))	
	int32 AvaliableFormations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = EETW_FormationDensity))
	int32 AvaliableDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = EETW_FormationMovementMode))
	int32 AvaliableMoveMode;
	
	UPROPERTY(EditAnywhere, meta=(Units="Centimeters"))
	float LooseDensity = 300.f;

	UPROPERTY(EditAnywhere, meta=(Units="Centimeters"))
	float NormalDensity = 180.f;

	UPROPERTY(EditAnywhere, meta=(Units="Centimeters"))
	float TightDensity = 110.f;

	UPROPERTY(EditAnywhere, meta=(Units="CentimetersPerSecond"))
	float MarchSpeed = 160.f;

	UPROPERTY(EditAnywhere, meta=(Units="CentimetersPerSecond"))
	float AdvanceSpeed = 340.f;

	UPROPERTY(EditAnywhere, meta=(Units="CentimetersPerSecond"))
	float SneakSpeed = 200.f;

	UPROPERTY(EditAnywhere)
	float CatchupSpeedFactor = 1.1f;
};
