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

void UMassCommanderComponent::SpawnSquad_Implementation(UMassEntityConfigAsset* EntityTemplate, int32 NumToSpawn, UMassEntityEQSSpawnPointsGenerator* PointGenerator)
{
	if (!EntityTemplate)
	{
		UE_LOG(ETW_Mass, Error, TEXT("Entity template or environment query not set!"));
		return;
	}

	FMassSpawnedEntityType EntityType;
	EntityType.EntityConfig = EntityTemplate;
	EntityType.Proportion = 1.f;
	PendingSpawnEntityTypes.Add(EntityType);

	FFinishedGeneratingSpawnDataSignature Delegate = FFinishedGeneratingSpawnDataSignature::CreateUObject(this, &UMassCommanderComponent::OnSpawnQueryGeneratorFinished);
	PointGenerator->Generate(*GetOwner(), TArrayView<FMassSpawnedEntityType>(PendingSpawnEntityTypes), NumToSpawn, Delegate);
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

void UMassCommanderComponent::InitializeComponent()
{
	Super::InitializeComponent();
	SetTraceFromComponent(GetOwner()->GetComponentByClass<UCameraComponent>());

	if (APlayerController* PC_ = Cast<APlayerController>(GetOwner()->GetOwner()))
	{
		PC = PC_;
	}
}

void UMassCommanderComponent::OnRep_CommandTraceResult()
{
	OnCommandProcessedDelegate.Broadcast(CommandTraceResult);
}
