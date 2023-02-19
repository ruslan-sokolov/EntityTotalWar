// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_RepresentationSubsystem.h"

#include "Engine/World.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassSimulationSubsystem.h"
#include "MassSimulationSettings.h"
#include "MassAgentSubsystem.h"
#include "MassEntityManager.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "MassEntityUtils.h"
#include "Components/InstancedStaticMeshComponent.h"

DEFINE_LOG_CATEGORY(LogMassRepresentation);

void UETW_MassVisualizationComponent::ConstructStaticMeshComponents()
{
	AActor* ActorOwner = GetOwner();
	check(ActorOwner);

	UE_MT_SCOPED_WRITE_ACCESS(InstancedStaticMeshInfosDetector);
	for (FMassInstancedStaticMeshInfo& Info_ : InstancedStaticMeshInfos)
	{
		FMassInstancedStaticMeshInfo_PublicAccess& Info = (FMassInstancedStaticMeshInfo_PublicAccess&)Info_;  // c style cast to get protected members access 
		// Check if it is already created
		if (!Info.InstancedStaticMeshComponents.IsEmpty())
		{
			continue;
		}

		// Check if there are any specified meshes for this visual type
		if(Info.Desc.Meshes.Num() == 0)
		{
			UE_LOG(LogMassRepresentation, Error, TEXT("No associated meshes for this intanced static mesh type"));
			continue;
		}
		for (const FStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
		{			
			FISMCSharedData* SharedData = ISMCSharedData.Find(MeshDesc);
			UInstancedStaticMeshComponent* ISMC = SharedData ? SharedData->ISMC : nullptr;
			if (SharedData)
			{
				SharedData->RefCount += 1;
			}
			else
			{
				ISMC = NewObject<UInstancedStaticMeshComponent>(ActorOwner);
				ISMC->SetupAttachment(ActorOwner->GetRootComponent());
				ISMC->bAlwaysCreatePhysicsState = true;
				ISMC->RegisterComponent();

				ISMC->SetStaticMesh(MeshDesc.Mesh);
				ISMC->CreatePhysicsState();
				
				for (int32 ElementIndex = 0; ElementIndex < MeshDesc.MaterialOverrides.Num(); ++ElementIndex)
				{
					if (UMaterialInterface* MaterialOverride = MeshDesc.MaterialOverrides[ElementIndex])
					{
						ISMC->SetMaterial(ElementIndex, MaterialOverride);
					}
				}
				ISMC->SetCullDistances(0, 1000000); // @todo: Need to figure out what to do here, either LOD or cull distances.
				ISMC->SetCastShadow(MeshDesc.bCastShadows);
				ISMC->Mobility = MeshDesc.Mobility;
				ISMC->SetReceivesDecals(false);

				ISMC->SetCanEverAffectNavigation(false);
				ISMC->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);  // @todo: collision profile receiving from shared fragment
				ISMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				ISMC->SetCollisionObjectType(ECC_WorldDynamic);
				ISMC->SetCollisionResponseToAllChannels(ECR_Block);
				//ISMC->bAlwaysCreatePhysicsState;
				//ISMC->CreatePhysicsState();

				if (ISMC->IsPhysicsStateCreated())
				{
					UE_LOG(LogMass, Warning, TEXT("Created"));
				}
				else
				{
					UE_LOG(LogMass, Warning, TEXT("Not Created"));
				}
				
				ISMCSharedData.Emplace(MeshDesc, FISMCSharedData(ISMC));
			}

			Info.InstancedStaticMeshComponents.Add(ISMC);
		}

		// Build the LOD significance ranges
		TArray<float> AllLODSignificances;
		auto UniqueInsertOrdered = [&AllLODSignificances](const float Significance)
		{
			int i = 0;
			for (; i < AllLODSignificances.Num(); ++i)
			{
				// I did not use epsilon check here on purpose, because it will make it hard later meshes inside.
				if (Significance == AllLODSignificances[i])
				{
					return;
				}
				if (AllLODSignificances[i] > Significance)
				{
					break;
				}
			}
			AllLODSignificances.Insert(Significance, i);
		};
		for (const FStaticMeshInstanceVisualizationMeshDesc& MeshDesc : Info.Desc.Meshes)
		{
			UniqueInsertOrdered(MeshDesc.MinLODSignificance);
			UniqueInsertOrdered(MeshDesc.MaxLODSignificance);
		}
		Info.LODSignificanceRanges.SetNum(AllLODSignificances.Num() - 1);
		for (int i = 0; i < Info.LODSignificanceRanges.Num(); ++i)
		{
			FMassLODSignificanceRange& Range = Info.LODSignificanceRanges[i];
			Range.MinSignificance = AllLODSignificances[i];
			Range.MaxSignificance = AllLODSignificances[i+1];
			Range.ISMCSharedDataPtr = &ISMCSharedData;

			for (int j = 0; j < Info.Desc.Meshes.Num(); ++j)
			{
				const FStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Info.Desc.Meshes[j];
				const bool bAddMeshInRange = (Range.MinSignificance >= MeshDesc.MinLODSignificance && Range.MinSignificance < MeshDesc.MaxLODSignificance);
				if (bAddMeshInRange)
				{
					Range.StaticMeshRefs.Add(MeshDesc);
				}
			}
		}

	}
}

void UETW_MassVisualizationComponent::BeginVisualChanges()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MassVisualizationComponent BeginVisualChanges")

	// Conditionally construct static mesh components
	if (bNeedStaticMeshComponentConstruction)
	{
		ConstructStaticMeshComponents();
		bNeedStaticMeshComponentConstruction = false;
	}

	// Reset instance transform scratch buffers
	for (auto It = ISMCSharedData.CreateIterator(); It; ++It)
	{
		FISMCSharedData& SharedData = It.Value();
		SharedData.UpdateInstanceIds.Reset();
		SharedData.StaticMeshInstanceCustomFloats.Reset();
		SharedData.StaticMeshInstanceTransforms.Reset();
		SharedData.StaticMeshInstancePrevTransforms.Reset();
		SharedData.WriteIterator = 0;
	}
}

AETW_MassVisualizer::AETW_MassVisualizer()
{
	ETW_VisComponent = CreateDefaultSubobject<UETW_MassVisualizationComponent>(TEXT("ETW_VisualizerComponent"));
	VisComponent = ETW_VisComponent;
}

void UETW_RepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMassSimulationSubsystem::StaticClass());
	Collection.InitializeDependency(UMassActorSpawnerSubsystem::StaticClass());
	Collection.InitializeDependency(UMassAgentSubsystem::StaticClass());

	if (UWorld* World = GetWorld())
	{
		EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World).AsShared();

		ActorSpawnerSubsystem = World->GetSubsystem<UMassActorSpawnerSubsystem>();
		WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>();

		if (Visualizer == nullptr)
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// The helper actor is only once per world so we can allow it to spawn during construction script.
			SpawnInfo.bAllowDuringConstructionScript = true;

			// override actor class from super class:
			AETW_MassVisualizer* SpawnedVisualizer = World->SpawnActor<AETW_MassVisualizer>(SpawnInfo);
			Visualizer = SpawnedVisualizer;
			ETW_Visualizer = SpawnedVisualizer;
			check(Visualizer);
			UETW_MassVisualizationComponent* CustomVisualizationComponent = ETW_Visualizer->GetCustomVisualizationComponent();
			VisualizationComponent = CustomVisualizationComponent;
			ETW_VisualizationComponent = CustomVisualizationComponent;

#if WITH_EDITOR
			Visualizer->SetActorLabel(FString::Printf(TEXT("%sVisualizer"), *GetClass()->GetName()), /*bMarkDirty*/false);
#endif
		}

		UMassSimulationSubsystem* SimSystem = World->GetSubsystem<UMassSimulationSubsystem>();
		check(SimSystem);
		// override callback method from super class:
		SimSystem->GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).AddUObject(this, &UETW_RepresentationSubsystem::OnProcessingPhaseStarted, EMassProcessingPhase::PrePhysics);
		SimSystem->GetOnProcessingPhaseFinished(EMassProcessingPhase::PostPhysics).AddUObject(this, &UETW_RepresentationSubsystem::OnProcessingPhaseStarted, EMassProcessingPhase::PostPhysics);

		UMassAgentSubsystem* AgentSystem = World->GetSubsystem<UMassAgentSubsystem>();
		check(AgentSystem);
		AgentSystem->GetOnMassAgentComponentEntityAssociated().AddUObject(this, &UETW_RepresentationSubsystem::OnMassAgentComponentEntityAssociated);
		AgentSystem->GetOnMassAgentComponentEntityDetaching().AddUObject(this, &UETW_RepresentationSubsystem::OnMassAgentComponentEntityDetaching);
	}

	RetryMovedDistanceSq = FMath::Square(GET_MASSSIMULATION_CONFIG_VALUE(DesiredActorFailedSpawningRetryMoveDistance));
	RetryTimeInterval = GET_MASSSIMULATION_CONFIG_VALUE(DesiredActorFailedSpawningRetryTimeInterval);
}

void UETW_RepresentationSubsystem::OnProcessingPhaseStarted(const float DeltaSeconds,
                                                            const EMassProcessingPhase Phase) const
{
	check(ETW_VisualizationComponent);
	switch (Phase)
	{
	case EMassProcessingPhase::PrePhysics:
		ETW_VisualizationComponent->BeginVisualChanges();
		break;
	case EMassProcessingPhase::PostPhysics:/* Currently this is the end of phases signal */
		ETW_VisualizationComponent->EndVisualChanges();
		break;
	default:
		check(false); // Need to handle this case
		break;
	}
}
