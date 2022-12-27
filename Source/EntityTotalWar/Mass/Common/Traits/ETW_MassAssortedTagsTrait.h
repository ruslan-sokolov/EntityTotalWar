// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTraitBase.h"
#include "InstancedStruct.h"
#include "ETW_MassAssortedTagsTrait.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName="Assorted Tags"))
class ENTITYTOTALWAR_API UETW_MassAssortedTagsTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	
	UPROPERTY(Category="Traits", EditAnywhere, meta = (BaseStruct = "MassTag", ExcludeBaseStruct))
	TArray<FInstancedStruct> Tags;
};
