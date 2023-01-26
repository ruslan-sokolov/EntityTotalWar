// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassCommonTypes.h"
#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassMovementFragments.h"
#include "ETW_MassSurfaceMovement.generated.h"

DECLARE_STATS_GROUP(TEXT("ETW Mass"), STATGROUP_ETWMass, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement"), STAT_SurfaceMovement, STATGROUP_ETWMass);
//DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Trace"), STAT_SurfaceMovementTrace, STATGROUP_ETWMass);

USTRUCT()
struct FMassSurfaceMovementTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassSurfaceMovementFloor
{
	GENERATED_BODY()
		
	FVector_NetQuantizeNormal SurfaceNormal;

	// distance from sphere center to projected on ground sphere center on Z axis
	// if zero - mean on ground, negative - fall or stairs down, positive - stairs up or stuck in wall 
	float SurfaceDistance;

	// distance from sphere center to projected on ground sphere center on Z axis
	// if zero - mean on ground, negative - fall or stairs down, positive - stairs up or stuck in wall 
	float GetRelativeDistance() const { return SurfaceDistance; }
	
	float GetAbsoluteDistance() const { return FMath::Abs(SurfaceDistance); };
	
	bool bHasNoSurface () const { return SurfaceDistance == FLT_MIN; };
	bool bInsideWall() const { return SurfaceDistance == FLT_MAX; };
	bool bHasSurface() const { return SurfaceDistance < FLT_MAX && SurfaceDistance > FLT_MIN; };
	bool bIsOnSurface() const { return SurfaceDistance == 0.f; }
	bool bShouldStepUp() const { return SurfaceDistance > 0.f; }
	bool bShouldStepDown() const { return SurfaceDistance < 0.f; }
};

USTRUCT()
struct FMassSurfaceMovementFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FMassSurfaceMovementFloor Floor;

	FVector ComputeGroundMovementDelta(const FVector& Delta) const
	{
		if (Floor.SurfaceNormal.Z < (1.f - UE_KINDA_SMALL_NUMBER) && Floor.SurfaceNormal.Z > UE_KINDA_SMALL_NUMBER)
		{
			// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
			const float FloorDotDelta = (Floor.SurfaceNormal | Delta);
			const FVector RampMovement(Delta.X, Delta.Y, -FloorDotDelta / Floor.SurfaceNormal.Z);
			return RampMovement.GetSafeNormal() * Delta.Size();
		}

		return Delta;
	}
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassSurfaceMovementParams : public FMassSharedFragment
{
	GENERATED_BODY()

	static FCollisionQueryParams DefaultCollisionQueryParams;

	FCollisionQueryParams CollisionQueryParams = DefaultCollisionQueryParams;
	
	/** StepHeight cm */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float StepHeight = 45;

	/** MaxFloorTraceDist  should be greater then Step Height!!!! cm */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cm"))
	float MaxFloorTraceDist = 200;

	/** Cos */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits="cosine"))
	float SurfaceNormalWalkableZ = 0.71;
	
	/** Gravity */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ForceUnits="cm / s^2"))
	float Gravity = -980.f;

	UPROPERTY(EditAnywhere, Category = "Trace")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldStatic;
	
	void FindFloor(const UWorld* World, const FVector& AgentLocation, float AgentRadius, FMassSurfaceMovementFloor& OutFloor) const
	{
		FVector TraceStart, TraceEnd = AgentLocation;
		TraceStart.Z += StepHeight;
		TraceEnd.Z -= StepHeight;

		FCollisionShape TraceSphere;
		TraceSphere.MakeSphere(AgentRadius);

		FHitResult Hit(1.f);
		bool bSuccessHit = World->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
		
		if (!bSuccessHit)
		{
			OutFloor.SurfaceDistance = FLT_MIN;  // falling
		}
		else if (Hit.bStartPenetrating) // stuck in wall
		{
			OutFloor.SurfaceDistance = FLT_MAX;
		}
		else if (Hit.Location.Z == AgentLocation.Z)  // stands on ground
		{
			OutFloor.SurfaceDistance = 0.f;
		}
		else  // stairs up or stairs down
		{
			float Sign = Hit.Location.Z > AgentLocation.Z ? 1.f : -1.f;
			OutFloor.SurfaceDistance = FMath::Abs(AgentLocation.Z - Hit.Location.Z) * Sign;
		}

		OutFloor.SurfaceNormal = Hit.ImpactNormal;
	}

	void SweepAlongSurface(const UWorld* World, float AgentRadius, const FVector& AgentLocation, const FVector& Delta, FHitResult& OutHit) const
	{
		FCollisionShape TraceSphere;
		TraceSphere.MakeSphere(AgentRadius);
		
		World->SweepSingleByChannel(OutHit, AgentLocation, AgentLocation + Delta, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
	}

	bool SurfaceIsWalkable(const FHitResult& Hit) const
	{
		return Hit.ImpactNormal.Z < SurfaceNormalWalkableZ;
	}

	bool SurfaceIsWalkable(const float SurfaceNormalZ) const
	{
		return SurfaceNormalZ < SurfaceNormalWalkableZ;
	}

	FVector ComputeSlideDelta(const FVector& Delta, float Time, const FVector_NetQuantizeNormal& InNormal, const FMassSurfaceMovementFloor& Floor, float AgentRadius) const
	{
		FVector_NetQuantizeNormal Normal(InNormal);
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!SurfaceIsWalkable(InNormal.Z))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (Floor.bHasSurface() && Floor.GetAbsoluteDistance() <  AgentRadius * 0.5)
			{
				const FVector FloorNormal = Floor.SurfaceNormal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
			
				Normal = Normal.GetSafeNormal2D();
			}
		}
		
		return FVector::VectorPlaneProject(Delta, Normal) * Time;
	}

	float SlideAlongSurface(const UWorld* World, float Time, float AgentRadius, const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFloor& Floor, FHitResult& OutHit) const;
	
	void TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal, const FMassSurfaceMovementFloor& Floor, float AgentRadius) const
	{
		FVector Delta = OutDelta;
		const FVector HitNormal = Hit.Normal;

		if ((OldHitNormal | HitNormal) <= 0.f) //90 or less corner, so use cross product for direction
		{
			const FVector DesiredDir = Delta;
			FVector NewDir = (HitNormal ^ OldHitNormal);
			NewDir = NewDir.GetSafeNormal();
			Delta = (Delta | NewDir) * (1.f - Hit.Time) * NewDir;
			if ((DesiredDir | Delta) < 0.f)
			{
				Delta = -1.f * Delta;
			}
		}
		else //adjust to new wall
		{
			const FVector DesiredDir = Delta;
			Delta = ComputeSlideDelta(Delta, 1.f - Hit.Time, HitNormal, Floor, AgentRadius);
			if ((Delta | DesiredDir) <= 0.f)
			{
				Delta = FVector::ZeroVector;
			}
			else if ( FMath::Abs((HitNormal | OldHitNormal) - 1.f) < UE_KINDA_SMALL_NUMBER )
			{
				// we hit the same wall again even after adjusting to move along it the first time
				// nudge away from it (this can happen due to precision issues)
				Delta += HitNormal * 0.01f;
			}
		}

		OutDelta = Delta;
	}

	bool IsWithinEdgeTolerance(const FVector& AgentLocation, const FVector& TestImpactPoint, const float AgentRadius) const
	{
		const float DistFromCenterSq = (TestImpactPoint - AgentLocation).SizeSquared2D();
		const float ReducedRadiusSq = FMath::Square(FMath::Max(0.15f + UE_KINDA_SMALL_NUMBER, AgentRadius - 0.15f));
		return DistFromCenterSq < ReducedRadiusSq;
	}
	
	bool StepUp(const UWorld* World, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFloor& Floor, FHitResult& OutHit, FMassSurfaceMovementFloor& OutFloor) const;
};


/**
 * 
 */
UCLASS(meta = (DisplayName = "ETW Surface Movement"))
class ENTITYTOTALWAR_API UMassSurfaceMovementTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassSurfaceMovementParams SurfaceMovementParams;

	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassMovementParameters MovementParams;
};


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UMassApplySurfaceMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassApplySurfaceMovementProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	FMassEntityQuery EntityQuery;
};

/**
 * Set Initial floor data for MassSurfaceMovementProcessor  WARNING: it supposes than entities spawned not stucked in collision!
 */
UCLASS()
class ENTITYTOTALWAR_API UMassApplySurfaceMovementInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UMassApplySurfaceMovementInitializer();

protected:
	virtual void Register() override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
