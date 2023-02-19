// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ETW_MassCollisionTypes.h"

#include "ETW_MassCollisionSubsystem.generated.h"


UCLASS(NotPlaceable, Transient)
class ENTITYTOTALWAR_API AETW_MassCollider : public AActor
{
	GENERATED_BODY()

public:
	AETW_MassCollider();

protected:
	
	// casted visualization component
	UPROPERTY()
	TMap<FMassEntityHandle, TObjectPtr<UCapsuleComponent>> CapsuleCollisions;

	friend class UETW_MassCollisionSubsystem;
};

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassCollisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void CreateCapsuleEntity(FETW_MassCopsuleFragment& OutCapsuleFragment, const FMassEntityHandle Entity, const FTransform& Transform, const FETW_MassCapsuleCollisionParams& Params);
	void DestroyCapsuleEntity(const FMassEntityHandle Entity);

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	// USubsystem END

	/** Creates all runtime data using main collection */
	void InitializeRuntime(UWorld* World);

	UPROPERTY(Transient)
	TObjectPtr<AETW_MassCollider> MassCollider;

	TSharedPtr<FMassEntityManager> EntityManager;
};

template<>
struct TMassExternalSubsystemTraits<UETW_MassCollisionSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};