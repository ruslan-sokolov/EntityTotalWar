// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadSubsystem.h"
#include "MassSimulationSubsystem.h"

FMassSquadManager::FMassSquadManager()
{
	
}

FMassSquadManager::~FMassSquadManager()
{
	
}


;
void UETW_MassSquadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>(); // todo: fix unresolved external link
}

void UETW_MassSquadSubsystem::PostInitialize()
{
	
	Super::PostInitialize();
}

void UETW_MassSquadSubsystem::Deinitialize()
{
	
	Super::Deinitialize(); 	// should called at the end
}

void UETW_MassSquadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	InitializeRuntime();
}

void UETW_MassSquadSubsystem::BeginDestroy()
{
	
	Super::BeginDestroy();
}

void UETW_MassSquadSubsystem::InitializeRuntime()
{
}
