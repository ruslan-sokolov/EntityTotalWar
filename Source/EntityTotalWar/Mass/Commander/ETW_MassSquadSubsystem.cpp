// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSquadSubsystem.h"

#include "ETW_MassSubsystem.h"
#include "MassReplicationSubsystem.h"
#include "MassSimulationSubsystem.h"
#include "Replication/Squad/ETW_MassSquadBubble.h"
#include "Replication/SquadUnits/ETW_MassSquadUnitsBubble.h"

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
	SquadUnitsPostSpawnProcessor = NewObject<UMassSquadUnitsPostSpawnProcessor>(this, UMassSquadUnitsPostSpawnProcessor::StaticClass(), FName(GetName() + TEXT("_MassSquadUnitsPostSpawnProc")));
	SquadPostSpawnProcessor = NewObject<UMassSquadPostSpawnProcessor>(this, UMassSquadPostSpawnProcessor::StaticClass(), FName(GetName() + TEXT("_MassSquadPostSpawnProc")));
}

UMassProcessor* UETW_MassSquadSubsystem::GetSpawnDataInitializer(TSubclassOf<UMassProcessor> InitializerClass)
{
	if (!InitializerClass)
	{
		return nullptr;
	}

	TObjectPtr<UMassProcessor>* const Initializer = SpawnDataInitializers.FindByPredicate([InitializerClass](const UMassProcessor* Processor)
		{
			return Processor && Processor->GetClass() == InitializerClass;
		}
	);

	if (Initializer == nullptr)
	{
		UMassProcessor* NewInitializer = NewObject<UMassProcessor>(this, InitializerClass);
		NewInitializer->Initialize(*this);
		SpawnDataInitializers.Add(NewInitializer);
		return NewInitializer;
	}

	return *Initializer;
}

void UETW_MassSquadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassSimulationSubsystem>();
	Collection.InitializeDependency<UMassReplicationSubsystem>();
	Collection.InitializeDependency<UETW_MassSubsystem>();

	SquadManager->Initialize();

	ensure(SquadUnitsPostSpawnProcessor);
	SquadUnitsPostSpawnProcessor->Initialize(*this);

	ensure(SquadPostSpawnProcessor);
	SquadPostSpawnProcessor->Initialize(*this);
}

void UETW_MassSquadSubsystem::PostInitialize()
{
	UMassReplicationSubsystem* ReplicationSubsystem = UWorld::GetSubsystem<UMassReplicationSubsystem>(GetWorld());

	check(ReplicationSubsystem);
	ReplicationSubsystem->RegisterBubbleInfoClass(AETW_MassSquadUnitClientBubbleInfo::StaticClass());
	ReplicationSubsystem->RegisterBubbleInfoClass(AETW_MassSquadClientBubbleInfo::StaticClass());
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

