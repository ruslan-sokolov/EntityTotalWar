// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassRepresentationSubsystem.h"
#include "MassVisualizer.h"
#include "MassVisualizationComponent.h"

#include "ETW_RepresentationSubsystem.generated.h"


USTRUCT()
struct ENTITYTOTALWAR_API FMassInstancedStaticMeshInfo_PublicAccess : public FMassInstancedStaticMeshInfo
{
	GENERATED_BODY()

public:
	using FMassInstancedStaticMeshInfo::Desc;
	using FMassInstancedStaticMeshInfo::InstancedStaticMeshComponents;
	using FMassInstancedStaticMeshInfo::LODSignificanceRanges;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_MassVisualizationComponent : public UMassVisualizationComponent
{
	GENERATED_BODY()

protected:
	// hides super class method
	void ConstructStaticMeshComponents();

public:
	// hides super class method
	void BeginVisualChanges();
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API AETW_MassVisualizer : public AMassVisualizer
{
	GENERATED_BODY()

public:
	AETW_MassVisualizer();

	// casted visualization component
	UPROPERTY()
	TObjectPtr<UETW_MassVisualizationComponent> ETW_VisComponent;

	// return casted visualization component
	class UETW_MassVisualizationComponent* GetCustomVisualizationComponent() const { return ETW_VisComponent; }
};

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_RepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// hides super class method
	void OnProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase) const;

	// casted visualizer actor
	UPROPERTY(Transient)
	TObjectPtr<AETW_MassVisualizer> ETW_Visualizer;

	/** casted visualization component */
	UPROPERTY(Transient)
	TObjectPtr<UETW_MassVisualizationComponent> ETW_VisualizationComponent;
};
