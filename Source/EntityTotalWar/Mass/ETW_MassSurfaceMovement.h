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

namespace MassSurfaceMovementConstants
{
	extern const float MAX_STEP_SIDE_Z;
	extern const float VERTICAL_SLOPE_NORMAL_Z;

	/** Minimum delta time considered when ticking. Delta times below this are not considered. This is a very small non-zero value to avoid potential divide-by-zero in simulation code. */
	extern const float MIN_TICK_TIME;

	/** Minimum acceptable distance for Character capsule to float above floor when walking. */
	extern const float MIN_FLOOR_DIST;

	/** Maximum acceptable distance for Character capsule to float above floor when walking. */
	extern const float MAX_FLOOR_DIST;

	/** Reject sweep impacts that are this close to the edge of the vertical portion of the capsule when performing vertical sweeps, and try again with a smaller capsule. */
	extern const float SWEEP_EDGE_REJECT_DISTANCE;

	/** Stop completely when braking and velocity magnitude is lower than this. */
	extern const float BRAKE_TO_STOP_VELOCITY;

	extern const float PENETRATION_PULLBACK_DISTANCE;
	extern const float PENETRATION_OVERLAP_INFLATION;
}

using namespace MassSurfaceMovementConstants;

USTRUCT()
struct FMassSurfaceMovementTag : public FMassTag
{
	GENERATED_BODY()
};

enum class ESurfaceMovementMode : uint8
{
	Walking,
	Falling
};

USTRUCT()
struct FMassSurfaceMovementFloor
{
	GENERATED_BODY()
	
	FVector_NetQuantizeNormal SurfaceNormal;
	float SurfaceDistance;
	
	bool bHasFloor;
	
	float GetSurfaceDistance() const { return SurfaceDistance; };
	bool bHasSurface() const { return bHasFloor; };
	void Clear() { SurfaceNormal = FVector::UpVector, SurfaceDistance = 0.f, bHasFloor = false; }
	void SetFromSweep(const FHitResult& InHit, const float InSweepFloorDist, const bool bIsWalkableFloor)
	{
		SurfaceDistance = InSweepFloorDist;
		SurfaceNormal = InHit.ImpactNormal;
		bHasFloor = bIsWalkableFloor;
	}
};

USTRUCT()
struct FMassSurfaceMovementFragment : public FMassFragment
{
	GENERATED_BODY()
	
	FMassSurfaceMovementFloor Floor;
	ESurfaceMovementMode MovementMode;

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
	
	void FindFloor(const UWorld* World, const FVector& AgentLocation, const float AgentRadius, FMassSurfaceMovementFloor& OutFloor, ESurfaceMovementMode MovementMode = ESurfaceMovementMode::Walking) const;

	bool SweepAlongSurface(const UWorld* World, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, FHitResult& OutHit) const
	{
		FCollisionShape TraceSphere;
		TraceSphere.SetSphere(AgentRadius);
		
		return World->SweepSingleByChannel(OutHit, AgentLocation, AgentLocation + Delta, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
	}
	
	bool ResolvePenetration(const UWorld* World, const FHitResult& Hit, const float AgentRadius, FVector& OutAdjustedLocation) const
	{
		const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);
		const FVector Adjustment = Hit.Normal * (PenetrationDepth + PENETRATION_PULLBACK_DISTANCE);
		
		if (Adjustment.IsZero())
		{
			return false;
		}

		FCollisionShape TraceSphere;
		TraceSphere.SetSphere(AgentRadius + PENETRATION_OVERLAP_INFLATION);
		bool bEncroached = World->OverlapBlockingTestByChannel(Hit.TraceStart + Adjustment, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
		if (!bEncroached)
		{
			// Move without sweeping.
			OutAdjustedLocation = Hit.Location + Adjustment;
			return true;
		}
		else
		{
			FHitResult SweepOutHit(1.f);
			bool bMoved = SweepAlongSurface(World, AgentRadius, Hit.Location, Adjustment, SweepOutHit);

			FVector CurrentLocation = SweepOutHit.bBlockingHit ? SweepOutHit.Location : SweepOutHit.TraceEnd;
			
			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const float SecondPenetrationDepth = (SweepOutHit.PenetrationDepth > 0.f ? SweepOutHit.PenetrationDepth : 0.125f);
				const FVector SecondMTD = SweepOutHit.Normal * (SecondPenetrationDepth + PENETRATION_PULLBACK_DISTANCE);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = SweepAlongSurface(World, AgentRadius, CurrentLocation, CombinedMTD, SweepOutHit);
					CurrentLocation = SweepOutHit.bBlockingHit ? SweepOutHit.Location : SweepOutHit.TraceEnd;
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = Hit.TraceEnd - Hit.TraceStart;
				if (!MoveDelta.IsZero())
				{
					bMoved = SweepAlongSurface(World, AgentRadius, CurrentLocation, Adjustment + MoveDelta, SweepOutHit);
					CurrentLocation = SweepOutHit.bBlockingHit ? SweepOutHit.Location : SweepOutHit.TraceEnd;
					
					// Finally, try the original move without MTD adjustments, but allowing depenetration along the MTD normal.
					// This was blocked because MOVECOMP_NeverIgnoreBlockingOverlaps was true for the original move to try a better depenetration normal, but we might be running in to other geometry in the attempt.
					// This won't necessarily get us all the way out of penetration, but can in some cases and does make progress in exiting the penetration.
					if (!bMoved && FVector::DotProduct(MoveDelta, Adjustment) > 0.f)
					{
						bMoved = SweepAlongSurface(World, AgentRadius, CurrentLocation, MoveDelta, SweepOutHit);
						CurrentLocation = SweepOutHit.bBlockingHit ? SweepOutHit.Location : SweepOutHit.TraceEnd;
					}
				}
			}

			OutAdjustedLocation = CurrentLocation;

			return bMoved;
		}
	}
	
	void SweepAlongSurfaceHandlePenetration(const UWorld* World, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, FHitResult& OutHit) const
	{
		FCollisionShape TraceSphere;
		TraceSphere.MakeSphere(AgentRadius);
		
		World->SweepSingleByChannel(OutHit, AgentLocation, AgentLocation + Delta, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
		
		// handle initial penetrations
		if (OutHit.bStartPenetrating)
		{
			FVector AdjustedLocation;
			if (ResolvePenetration(World, OutHit, AgentRadius, AdjustedLocation))
			{
				World->SweepSingleByChannel(OutHit, AdjustedLocation, AdjustedLocation + Delta, FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
			}
		}
		
	}

	bool SurfaceIsWalkable(const FHitResult& Hit) const
	{
		return Hit.ImpactNormal.Z < SurfaceNormalWalkableZ;
	}

	bool SurfaceIsWalkable(const float SurfaceNormalZ) const
	{
		return SurfaceNormalZ < SurfaceNormalWalkableZ;
	}

	FVector ComputeSlideDelta(const FVector& Delta, const float Time, const FVector_NetQuantizeNormal& InNormal, const FMassSurfaceMovementFloor& Floor, const float AgentRadius) const
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
			if (Floor.bHasSurface() && Floor.GetSurfaceDistance() <  AgentRadius * 0.5)
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

	float SlideAlongSurface(const UWorld* World, const float Time, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFragment& SurfaceMovementFragment, FHitResult& OutHit) const;
	float SlideAlongSurfaceResolvePenetration(const UWorld* World, const float Time, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFragment& SurfaceMovementFragment, FHitResult& OutHit) const;
	
	void TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal, const FMassSurfaceMovementFloor& Floor, const float AgentRadius) const;

	bool IsWithinEdgeTolerance(const FVector& AgentLocation, const FVector& TestImpactPoint, const float AgentRadius) const
	{
		const float DistFromCenterSq = (TestImpactPoint - AgentLocation).SizeSquared2D();
		const float ReducedRadiusSq = FMath::Square(FMath::Max(MassSurfaceMovementConstants::SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, AgentRadius - MassSurfaceMovementConstants::SWEEP_EDGE_REJECT_DISTANCE));
		return DistFromCenterSq < ReducedRadiusSq;
	}
	
	bool StepUp(const UWorld* World, const float AgentRadius, const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFragment& SurfaceMovementFragment, FHitResult& OutHit, FMassSurfaceMovementFloor& OutFloor) const;
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
