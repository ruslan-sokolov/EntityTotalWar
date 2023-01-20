// Fill out your copyright notice in the Description page of Project Settings.


#include "MassCommanderComponent.h"
#include "EntityTotalWar/ETW_PlayerCameraPawn.h"

UMassCommanderComponent::UMassCommanderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMassCommanderComponent::BeginPlay()
{
	Super::BeginPlay();
	PlayerCameraPawn = Cast<AETW_PlayerCameraPawn>(GetOwner());
}

void UMassCommanderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMassCommanderComponent::TrySelectUnit()
{
	ensure(PlayerCameraPawn);
	
	const bool bSuccessHit = LineTraceFromCursor(LastSelectUnitResult);
	PlayerCameraPawn->OnTrySelectUnit(LastSelectUnitResult, bSuccessHit);
}

void UMassCommanderComponent::TryCommandUnit()
{	ensure(PlayerCameraPawn);
	
	const bool bSuccessHit = LineTraceFromCursor(LastCommandUnitResult);
	PlayerCameraPawn->OnTryCommandUnit(LastCommandUnitResult, bSuccessHit);

}

bool UMassCommanderComponent::LineTraceFromCursor(FHitResult& OutHit)
{
	ensure(PlayerCameraPawn);
	
	bool bSuccessHit = false;
	
	if (const APlayerController* PC = PlayerCameraPawn->GetController<APlayerController>())
	{
		FVector TraceStart;
		FVector TraceDirection;
		PC->DeprojectMousePositionToWorld(TraceStart, TraceDirection);
		const FVector TraceEnd = TraceStart + TraceDirection * 30000.f;
		
		bSuccessHit = GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility);
	}

	return bSuccessHit;
}

