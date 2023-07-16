// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Commander/ETW_Squad.h"
#include "Net/UnrealNetwork.h"

void AETW_Squad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TeamIndex);
	DOREPLIFETIME(ThisClass, TargetSquad);
	DOREPLIFETIME(ThisClass, TargetLocation);
}

AETW_Squad::AETW_Squad()
{
	SetReplicates(true);
}
