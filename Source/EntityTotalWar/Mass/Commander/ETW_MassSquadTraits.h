// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ETW_MassSquadFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassReplicationTrait.h"

#include "ETW_MassSquadTraits.generated.h"

UCLASS(meta = (DisplayName = "ETW Squad Replication"))
class ENTITYTOTALWAR_API UETW_MassSquadReplicationTrait : public UMassReplicationTrait
{
	GENERATED_BODY()

public:
	UETW_MassSquadReplicationTrait();
};


UCLASS(meta = (DisplayName = "ETW Squad Units Replication"))
class ENTITYTOTALWAR_API UETW_MassSquadUnitsReplicationTrait : public UMassReplicationTrait
{
	GENERATED_BODY()

public:
	UETW_MassSquadUnitsReplicationTrait();
};

UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class ENTITYTOTALWAR_API UETW_MassSquadUnitTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	// check if contain UETW_MassSquadTrait
	virtual void ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	
	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	mutable FETW_MassSquadParams Params;

	// should contain UETW_MassSquadTrait
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UMassEntityConfigAsset> SquadEntityTemplate;
};