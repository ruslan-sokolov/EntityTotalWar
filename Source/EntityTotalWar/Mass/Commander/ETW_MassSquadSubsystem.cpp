// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadSubsystem.h"
#include "MassSimulationSubsystem.h"

FMassSquadManager::FMassSquadManager(UObject* InOwner)
	: Owner(InOwner)
{
	
}

FMassSquadManager::~FMassSquadManager()
{
	
}

void FMassSquadManager::Initialize()
{
}

void FMassSquadManager::Deinitialize()
{
}

void FMassSquadManager::AddReferencedObjects(FReferenceCollector& Collector)
{
}


UETW_MassSquadSubsystem::UETW_MassSquadSubsystem()
	: SquadManager(MakeShareable(new FMassSquadManager(this)))
{
	const FString ObjectName = GetName() + TEXT("_MassSquadPostSpawnProc");
	SquadPostSpawnProcessor = NewObject<UMassSquadPostSpawnProcessor>(this, UMassSquadPostSpawnProcessor::StaticClass(), FName(ObjectName));
}

void UETW_MassSquadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassSimulationSubsystem>();
	SquadManager->Initialize();

	ensure(SquadPostSpawnProcessor);
	SquadPostSpawnProcessor->Initialize(*this);
}

void UETW_MassSquadSubsystem::PostInitialize()
{
}

void UETW_MassSquadSubsystem::Deinitialize()
{
	SquadManager->Deinitialize();
}

void UETW_MassSquadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	InitializeRuntime();
}

void UETW_MassSquadSubsystem::InitializeRuntime()
{
}

