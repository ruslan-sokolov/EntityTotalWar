// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassCollisionSubsystem.h"

#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassSimulationSubsystem.h"
#include "Components/CapsuleComponent.h"

AETW_MassCollider::AETW_MassCollider()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->SetMobility(EComponentMobility::Static);

	PrimaryActorTick.bCanEverTick = false;
}

void UETW_MassCollisionSubsystem::CreateCapsuleEntity(FETW_MassCopsuleFragment& OutCapsuleFragment,
	const FMassEntityHandle Entity, const FTransform& Transform, const FETW_MassCapsuleCollisionParams& Params)
{
	check(MassCollider);

	UCapsuleComponent* CapsuleComponent = NewObject<UCapsuleComponent>(MassCollider);
	CapsuleComponent->SetupAttachment(MassCollider->GetRootComponent());
	CapsuleComponent->SetRelativeLocation_Direct(Transform.GetLocation());
	CapsuleComponent->SetRelativeRotation_Direct(Transform.Rotator());
	CapsuleComponent->SetRelativeScale3D_Direct(Transform.GetScale3D());
	CapsuleComponent->InitCapsuleSize(Params.CapsuleRadius, Params.CapsuleHalfHeight);
	CapsuleComponent->SetCollisionProfileName(Params.CollisionProfleName.Name);
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;
	CapsuleComponent->PrimaryComponentTick.bCanEverTick = false;
	CapsuleComponent->RegisterComponent();

	MassCollider->CapsuleCollisions.Emplace(Entity, CapsuleComponent);
	OutCapsuleFragment.SetCapsuleComponent(CapsuleComponent);
}

void UETW_MassCollisionSubsystem::DestroyCapsuleEntity(const FMassEntityHandle Entity)
{
	check(MassCollider);

	if (TObjectPtr<UCapsuleComponent>* CapsuleComponentPtr = MassCollider->CapsuleCollisions.Find(Entity))
	{
		UCapsuleComponent* CapsuleComponent = CapsuleComponentPtr->Get();
		CapsuleComponent->DestroyComponent();

		MassCollider->CapsuleCollisions.Remove(Entity);
	}
}

void UETW_MassCollisionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMassSimulationSubsystem::StaticClass());
	Super::Initialize(Collection);
}

void UETW_MassCollisionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UETW_MassCollisionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	InitializeRuntime(&InWorld);
}

void UETW_MassCollisionSubsystem::InitializeRuntime(UWorld* World)
{
	ensure(World);

	if (MassCollider == nullptr)
	{
		EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World).AsShared();
		
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The helper actor is only once per world so we can allow it to spawn during construction script.
		SpawnInfo.bAllowDuringConstructionScript = true;
		
		MassCollider = World->SpawnActor<AETW_MassCollider>(SpawnInfo);
		check(MassCollider);

#if WITH_EDITOR
		MassCollider->SetActorLabel(FString::Printf(TEXT("%s_MassCollider"), *GetClass()->GetName()), /*bMarkDirty*/false);
#endif
	}
}
