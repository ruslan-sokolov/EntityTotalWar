// Fill out your copyright notice in the Description page of Project Settings.


#include "MassCommanderComponent.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"

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
			if (APlayerController* PC = OwnerPawn->GetController<APlayerController>())
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
		if (APlayerController* PC = OwnerPawn->GetController<APlayerController>())
		{
			bTraceFromCursor = PC->bShowMouseCursor;
			PC->DeprojectMousePositionToWorld(ClientCursorLocation, ClientCursorDirection);
		}
	}

	float HalfHeight = 500.f;
	float Radius = 100.f;
	//FVector Origin = ClientCursorLocation + ClientCursorDirection * (Radius + HalfHeight);
	FVector Origin = ClientCursorLocation;
	FQuat Rotation = ClientCursorDirection.ToOrientationQuat();

	//DrawDebugCapsule(GetWorld(), Origin, HalfHeight, Radius, Rotation, FColor::Orange, false, 10.f);
	DrawDebugLine(GetWorld(), Origin, Origin+ClientCursorDirection*1000.f, FColor::Orange, false, 10.f);

	K2_ReceiveCommandInputAction();

	ServerProcessInputAction(ClientCursorLocation, ClientCursorDirection, bTraceFromCursor);
}

void UMassCommanderComponent::ServerProcessInputAction_Implementation(FVector_NetQuantize ClientCursorLocation, FVector_NetQuantizeNormal ClientCursorDirection, bool bTraceFromCursor)
{
	RaycastCommandTarget(ClientCursorLocation, ClientCursorDirection, bTraceFromCursor);
	K2_ServerProcessInputAction();
	OnCommandProcessedDelegate.Broadcast(CommandTraceResult);
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
}

void UMassCommanderComponent::OnRep_CommandTraceResult()
{
	OnCommandProcessedDelegate.Broadcast(CommandTraceResult);
}
