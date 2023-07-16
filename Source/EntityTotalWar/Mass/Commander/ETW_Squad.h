// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTypes.h"
#include "ETW_Squad.generated.h"

class AETW_Squad;

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassSquadFragment : public FMassSharedFragment
{
	GENERATED_BODY()


};

UCLASS(Blueprintable)
class ENTITYTOTALWAR_API AETW_Squad : public APawn
{
	GENERATED_BODY()

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	AETW_Squad();
	
	FMassEntityHandle SquadEntity;

	UPROPERTY(BlueprintReadWrite, Replicated)
	int32 TeamIndex;

	UPROPERTY(BlueprintReadWrite, Replicated)
	TWeakObjectPtr<AETW_Squad> TargetSquad;

	UPROPERTY(BlueprintReadWrite, Replicated)
	FVector TargetLocation;


};