// Fill out your copyright notice in the Description page of Project Settings.


#include "MassCommanderComponent.h"

#include "MassEntityConfigAsset.h"
#include "MassEntityEQSSpawnPointsGenerator.h"
#include "Math/RandomStream.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnerTypes.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"
#include "VisualLogger/VisualLogger.h"
#include "MassEntityConfigAsset.h"

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
	UClass* PointGeneratorClass = StaticCast<UClass*>(PointGeneratorPath.TryLoad());
	UMassEntityEQSSpawnPointsGenerator* SpawnPointGeneratorCDO = CastChecked<UMassEntityEQSSpawnPointsGenerator>(PointGeneratorClass->ClassDefaultObject);

	UMassEntityConfigAsset* EntityConfig = StaticCast<UMassEntityConfigAsset*>(EntityTemplatePath.TryLoad());
	EntityConfig->GetOrCreateEntityTemplate(*GetWorld());
	
	FMassSpawnedEntityType EntityType;
	EntityType.EntityConfig = EntityTemplatePath;
	EntityType.Proportion = 1.f;
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
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());

	if (SpawnerSystem == nullptr)
	{
		UE_VLOG_UELOG(this, ETW_Mass, Error, TEXT("UMassSpawnerSubsystem missing while trying to spawn entities"));
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	for (const FMassEntitySpawnDataGeneratorResult& Result : Results)
	{
		if (Result.NumEntities <= 0)
		{
			continue;
		}
		
		check(PendingSpawnEntityTypes.IsValidIndex(Result.EntityConfigIndex));
		check(Result.SpawnDataProcessor != nullptr);
		
		FMassSpawnedEntityType& EntityType = PendingSpawnEntityTypes[Result.EntityConfigIndex];

		if (UMassEntityConfigAsset* EntityConfig = EntityType.EntityConfig.LoadSynchronous())
		{
			const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
			if (EntityTemplate.IsValid())
			{
				TArray<FMassEntityHandle> OutEntities;
				SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, OutEntities);
			}
		}
	}
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
