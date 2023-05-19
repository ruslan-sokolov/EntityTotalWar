// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/ETW_MassSubsystem.h"
#include "MassSimulationSubsystem.h"
#include "MassReplicationSubsystem.h"
#include "Mass/Replication/ETW_MassClientBubbleInfo.h"

void UETW_MassSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassSimulationSubsystem>();
}

void UETW_MassSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UMassReplicationSubsystem* ReplicationSubsystem = UWorld::GetSubsystem<UMassReplicationSubsystem>(GetWorld());

	check(ReplicationSubsystem);
	ReplicationSubsystem->RegisterBubbleInfoClass(AETW_MassClientBubbleInfo::StaticClass());
}
