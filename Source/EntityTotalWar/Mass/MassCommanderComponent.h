// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MassCommanderComponent.generated.h"

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class ENTITYTOTALWAR_API UMassCommanderComponent : public UActorComponent
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FHitResult LastSelectUnitResult;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FHitResult LastCommandUnitResult;

protected:
	UPROPERTY()
	class AETW_PlayerCameraPawn* PlayerCameraPawn;
	
public:
	// Sets default values for this component's properties
	UMassCommanderComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void TrySelectUnit();
	void TryCommandUnit();

protected:


private:
	
	bool LineTraceFromCursor(FHitResult& OutHit);
	
};
