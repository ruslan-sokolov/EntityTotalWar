// Fill out your copyright notice in the Description page of Project Settings.


#include "MassCommanderComponent.h"

#include "ETW_MassSquadProcessors.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityEQSSpawnPointsGenerator.h"
#include "Math/RandomStream.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnerTypes.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"
#include "VisualLogger/VisualLogger.h"
#include "MassEntityConfigAsset.h"
#include "MassExecutor.h"
#include "ETW_MassSquadSubsystem.h"
#include "ETW_MassSquadTraits.h"
#include "Math/UnitConversion.h"

void UMassCommanderComponent::SetTraceFromComponent_Implementation(USceneComponent* InTraceFromComponent)
{
	if (IsValid(InTraceFromComponent) && TraceFromComponent == nullptr)
	{
		TraceFromComponent = InTraceFromComponent;
	}
}

bool UMassCommanderComponent::RaycastCommandTarget(const FVector& ClientCursorLocation, const FVector& ClientCursorDirection, bool bTraceFromCursor)
{
	bool bSuccessHit = false;

	if (bTraceFromCursor)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC_ = OwnerPawn->GetController<APlayerController>())
			{
				const FVector TraceEnd = ClientCursorLocation + ClientCursorDirection * 30000.f;

				bSuccessHit = GetWorld()->LineTraceSingleByChannel(LastCommandTraceResult, ClientCursorLocation, TraceEnd, ECC_Visibility);
				CommandTraceResult = { LastCommandTraceResult };
			}
		}
	}
	else if (TraceFromComponent.IsValid())
	{
		const FVector TraceStart = TraceFromComponent->GetComponentLocation();
		const FVector TraceDirection = TraceFromComponent->GetForwardVector();
		const FVector TraceEnd = TraceStart + TraceDirection * 30000.f;

		bSuccessHit = GetWorld()->LineTraceSingleByChannel(LastCommandTraceResult, TraceStart, TraceEnd, ECC_Visibility);

		//CommandTraceResult = { LastCommandTraceResult.Location, LastCommandTraceResult.Normal, LastCommandTraceResult.Component };
		CommandTraceResult = { LastCommandTraceResult };
	}
	else
	{
		UE_DEBUG_BREAK();
	}

	return bSuccessHit;
}

UMassCommanderComponent::UMassCommanderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void UMassCommanderComponent::ReceiveCommandInputAction()
{
	FVector_NetQuantize ClientCursorLocation, ClientCursorDirection = FVector::ZeroVector;
	bool bTraceFromCursor = false;
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC_ = OwnerPawn->GetController<APlayerController>())
		{
			bTraceFromCursor = PC_->bShowMouseCursor;
			PC_->DeprojectMousePositionToWorld(ClientCursorLocation, ClientCursorDirection);
		}
	}

	K2_ReceiveCommandInputAction();

	ServerProcessInputAction(ClientCursorLocation, ClientCursorDirection, bTraceFromCursor);
}

void UMassCommanderComponent::ServerProcessInputAction_Implementation(FVector_NetQuantize ClientCursorLocation, FVector_NetQuantizeNormal ClientCursorDirection, bool bTraceFromCursor)
{
	RaycastCommandTarget(ClientCursorLocation, ClientCursorDirection, bTraceFromCursor);
	K2_ServerProcessInputAction();
	OnCommandProcessedDelegate.Broadcast(CommandTraceResult);
}

void UMassCommanderComponent::BeginPlay()
{
	Super::BeginPlay();
	
	RegisterEntityTemplates();
}

void UMassCommanderComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	SetTraceFromComponent(GetOwner()->GetComponentByClass<UCameraComponent>());

	if (APlayerController* PC_ = Cast<APlayerController>(GetOwner()->GetOwner()))
	{
		PC = PC_;
	}
}

void UMassCommanderComponent::RegisterEntityTemplates()
{
	UWorld* World = GetWorld();
	check(World);
	for (TSoftObjectPtr<UMassEntityConfigAsset>& ConfigAssetSoftRef : PreLoadTemplates)
	{
		if (const UMassEntityConfigAsset* EntityConfig = ConfigAssetSoftRef.LoadSynchronous())
		{
			EntityConfig->GetOrCreateEntityTemplate(*World);
		}
	}
}

void UMassCommanderComponent::Server_SpawnSquad_Implementation(FSoftObjectPath EntityTemplatePath, int32 NumToSpawn,  FSoftObjectPath PointGeneratorPath)
{
	if (bSquadEntitiesSpawnInProgress)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassCommanderComponent::Server_SpawnSquad_Implementation spawning squad allready in progress"));
	}
	
	bSquadEntitiesSpawnInProgress = true;
	
	UClass* PointGeneratorClass = StaticCast<UClass*>(PointGeneratorPath.TryLoad());
	UMassEntityEQSSpawnPointsGenerator* SpawnPointGeneratorCDO = CastChecked<UMassEntityEQSSpawnPointsGenerator>(PointGeneratorClass->ClassDefaultObject);

	UMassEntityConfigAsset* EntityConfig = StaticCast<UMassEntityConfigAsset*>(EntityTemplatePath.TryLoad());
	EntityConfig->GetOrCreateEntityTemplate(*GetWorld());

	bool bHasSquadTrait = EntityConfig->GetConfig().GetTraits().FindByPredicate([](const UMassEntityTraitBase* Trait){return Trait->IsA<UETW_MassSquadUnitTrait>(); }) != nullptr;
	ensureMsgf(bHasSquadTrait, TEXT("%s Mass Commander Component can only spawn from entity config that containts squad entity trait!"), *EntityConfig->GetName());
	
	FMassSpawnedEntityType EntityType;
	EntityType.EntityConfig = EntityTemplatePath;
	EntityType.Proportion = 1.f;
	PendingSpawnEntityTypes.Empty();
	PendingSpawnEntityTypes.Add(EntityType);

	
	
	FFinishedGeneratingSpawnDataSignature Delegate = FFinishedGeneratingSpawnDataSignature::CreateUObject(this, &UMassCommanderComponent::OnSpawnQueryGeneratorFinished);
	SpawnPointGeneratorCDO->Generate(*GetOwner(), TArrayView<FMassSpawnedEntityType>(PendingSpawnEntityTypes), NumToSpawn, Delegate);
}

void UMassCommanderComponent::SpawnSquad(TSoftObjectPtr<UMassEntityConfigAsset> EntityTemplate, int32 NumToSpawn,
	TSoftClassPtr<UMassEntityEQSSpawnPointsGenerator> PointGenerator)
{
	Server_SpawnSquad(EntityTemplate.ToSoftObjectPath(), NumToSpawn, PointGenerator.ToSoftObjectPath());
}

void UMassCommanderComponent::OnSpawnQueryGeneratorFinished(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results)
{
	if (Results.Num() > 1)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassCommanderComponent::SpawnSquad supports only one archeotype to spawn"));
		check(false);
		//return;
	}
	
	UWorld* World = GetWorld();
	check(World);

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	if (SpawnerSystem == nullptr)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassSpawnerSubsystem missing while trying to spawn entities"));
		return;
	}
		
	UETW_MassSquadSubsystem* SquadSubsystem = World->GetSubsystem<UETW_MassSquadSubsystem>();
	if (SpawnerSystem == nullptr)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UETW_MassSquadSubsystem missing while trying to spawn squads!"));
		return;
	}
	FMassSquadManager& SquadManager = SquadSubsystem->GetMutablSquadManager();
	
	TArray<FMassEntityHandle> SpawnedEntities;
	FMassArchetypeHandle UnitsArchetypeHandle;
	FMassEntityHandle SpawnedSquadEntity;
	FMassArchetypeHandle SquadArchetypeHandle;

	for (const FMassEntitySpawnDataGeneratorResult& Result : Results)
	{
		if (Result.NumEntities <= 0)
		{
			continue;
		}
		
		check(PendingSpawnEntityTypes.IsValidIndex(Result.EntityConfigIndex));
		check(Result.SpawnDataProcessor != nullptr);
		
		FMassSpawnedEntityType& EntityType = PendingSpawnEntityTypes[Result.EntityConfigIndex];
		if (UMassEntityConfigAsset* UnitEntityConfig = EntityType.EntityConfig.LoadSynchronous())
		{
			const FMassEntityTemplate& UnitEntityTemplate = UnitEntityConfig->GetOrCreateEntityTemplate(*World);
			if (UnitEntityTemplate.IsValid())
			{
				TArray<FMassEntityHandle> SpawnedUnitsEntities;
				FSharedStruct SquadSharedFragmentStruct;

				UnitsArchetypeHandle = UnitEntityTemplate.GetArchetype();
				
				FMassArchetypeSharedFragmentValues UnitMutableTemplateSharedFragments = UnitEntityTemplate.GetSharedFragmentValues();  // shared fragments values struct copy
				for (FSharedStruct& SharedStruct : UnitMutableTemplateSharedFragments.GetMutableSharedFragments())
				{
					if (FETW_MassSquadSharedFragment* SquadSharedFragment = SharedStruct.GetMutablePtr<FETW_MassSquadSharedFragment>())
					{
						FETW_MassSquadSharedFragment SquadSharedFragment_UniquePerSquad;
						SquadSharedFragment_UniquePerSquad.Formation = SquadSharedFragment->Formation;
						SquadSharedFragment_UniquePerSquad.SquadIndex = SquadManager.GetSquadId();
	
						uint32 SquadSharedFragmentHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(SquadSharedFragment_UniquePerSquad), SquadSharedFragment_UniquePerSquad.SquadIndex);  // add crc to make hash different from default object 
						SquadSharedFragmentStruct = EntityManager.GetOrCreateSharedFragmentByHash<FETW_MassSquadSharedFragment>(SquadSharedFragmentHash, SquadSharedFragment_UniquePerSquad);
						SharedStruct = SquadSharedFragmentStruct;  // make new shared fragment unique per squad and not archetype
						break;
					}
				}
			
				for (const FConstSharedStruct& ConstSharedStruct : UnitMutableTemplateSharedFragments.GetConstSharedFragments())
				{
					if (const FETW_MassSquadParams* SquadParams = ConstSharedStruct.GetPtr<FETW_MassSquadParams>())
					{
						if (UMassEntityConfigAsset* SquadEntityConfig = SquadParams->SquadEntityTemplate.LoadSynchronous())
						{
							const FMassEntityTemplate& SquadEntityTemplate = SquadEntityConfig->GetOrCreateEntityTemplate(*World);
							FMassArchetypeSharedFragmentValues SquadMutableTemplateSharedFragments = SquadEntityTemplate.GetSharedFragmentValues();

							for (FSharedStruct& SharedStruct : SquadMutableTemplateSharedFragments.GetMutableSharedFragments())
							{
								if (SharedStruct.GetMutablePtr<FETW_MassSquadSharedFragment>())
								{
									SharedStruct = SquadSharedFragmentStruct;  // copy new shared fragment unique per squad and not archetype
									break;
								}
							}

							for (const FConstSharedStruct& ConstSharedStruct_SquadTemplate : SquadMutableTemplateSharedFragments.GetConstSharedFragments())
							{
								if (ConstSharedStruct_SquadTemplate.GetPtr<FETW_MassSquadParams>())
								{
									FConstSharedStruct& MutableConstSharedStruct = const_cast<FConstSharedStruct&>(ConstSharedStruct_SquadTemplate);
									MutableConstSharedStruct = EntityManager.GetOrCreateConstSharedFragment(*SquadParams);  // copy new shared fragment unique per squad and not archetype
									break;
								}
							}
							
							SquadArchetypeHandle = SquadEntityTemplate.GetArchetype();
							SpawnedSquadEntity = EntityManager.CreateEntity(SquadArchetypeHandle, SquadMutableTemplateSharedFragments);
							TConstArrayView<FInstancedStruct> FragmentInstances = SquadEntityTemplate.GetInitialFragmentValues();
							EntityManager.SetEntityFragmentsValues(SpawnedSquadEntity, FragmentInstances);
						}
						break;
					}
				}
				
				TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.BatchCreateEntities(UnitsArchetypeHandle, UnitMutableTemplateSharedFragments, Result.NumEntities, SpawnedUnitsEntities);
				
				TConstArrayView<FInstancedStruct> FragmentInstances = UnitEntityTemplate.GetInitialFragmentValues();
				EntityManager.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), FragmentInstances);

				UMassProcessor* SpawnDataInitializer = Result.SpawnData.IsValid() ? SquadSubsystem->GetSpawnDataInitializer(Result.SpawnDataProcessor) : nullptr;
				if (SpawnDataInitializer)
				{
					FMassProcessingContext ProcessingContext(EntityManager, /*TimeDelta=*/0.0f);
					ProcessingContext.AuxData = Result.SpawnData;
					UE::Mass::Executor::RunProcessorsView(MakeArrayView(&SpawnDataInitializer, 1), ProcessingContext, &CreationContext->GetEntityCollection());
				}
				
				SpawnedEntities.Append(SpawnedUnitsEntities);
			}
		}
		
		break;
	}

	/* todo: Use Single Run Processor view in this function */
	UMassSquadUnitsPostSpawnProcessor* SquadUnitsPostSpawnProc = SquadSubsystem->GetSquadUnitsPostSpawnProcessor();
	if (SquadUnitsPostSpawnProc == nullptr)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassSquadUnitsPostSpawnProcessor missing while trying to spawn squads!"));
		return;
	}

	UMassSquadPostSpawnProcessor* SquadPostSpawnProc = SquadSubsystem->GetSquadPostSpawnProcessor();
	if (SquadPostSpawnProc == nullptr)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassSquadPostSpawnProcessor missing while trying to spawn squads!"));
		return;
	}


	FMassProcessingContext UnitsProcessingContext(EntityManager, /*TimeDelta=*/0.0f);

	// pass aux data to processor
	UnitsProcessingContext.AuxData.InitializeAs<FMassSquadUnitsSpawnAuxData>();
	FMassSquadUnitsSpawnAuxData& SpawnData = UnitsProcessingContext.AuxData.GetMutable<FMassSquadUnitsSpawnAuxData>();
	SpawnData.TeamIndex = TeamIndex;
	SpawnData.CommanderComp = this;
	SpawnData.SquadInitialTransform = GetOwner()->GetTransform();

	FMassArchetypeEntityCollection UnitsEntityCollection(UnitsArchetypeHandle, SpawnedEntities, FMassArchetypeEntityCollection::NoDuplicates);
	TArray<UMassProcessor*> SquadUnitsProcesorView = { SquadUnitsPostSpawnProc };
	UE::Mass::Executor::RunProcessorsView(SquadUnitsProcesorView, UnitsProcessingContext, &UnitsEntityCollection);

	
	FMassProcessingContext SquadEntityProcessingContext = UnitsProcessingContext;
	FMassArchetypeEntityCollection SquadEntityCollection(SquadArchetypeHandle, {SpawnedSquadEntity}, FMassArchetypeEntityCollection::NoDuplicates);
	TArray<UMassProcessor*> SquadProcessorView = { SquadPostSpawnProc };
	UE::Mass::Executor::RunProcessorsView(SquadProcessorView, SquadEntityProcessingContext, &SquadEntityCollection);
	
	bSquadEntitiesSpawnInProgress = false;
	OnSquadSpawningFinishedEvent.Broadcast();
}

void UMassCommanderComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CommandTraceResult);
}

void UMassCommanderComponent::OnRep_CommandTraceResult()
{
	OnCommandProcessedDelegate.Broadcast(CommandTraceResult);
}
