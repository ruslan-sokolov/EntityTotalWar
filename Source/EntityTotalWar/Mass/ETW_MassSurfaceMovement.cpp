// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSurfaceMovement.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationLOD.h"
#include "MassCommandBuffer.h"
#include "MassObserverRegistry.h"
#include "Math/UnrealMathUtility.h"

FCollisionQueryParams FMassSurfaceMovementParams::DefaultCollisionQueryParams(SCENE_QUERY_STAT(STAT_SurfaceMovementTrace), false);

namespace MassSurfaceMovementConstants
{
	const float MAX_STEP_SIDE_Z = 0.08f;
	const float VERTICAL_SLOPE_NORMAL_Z = 0.001f;

	/** Minimum delta time considered when ticking. Delta times below this are not considered. This is a very small non-zero value to avoid potential divide-by-zero in simulation code. */
	const float MIN_TICK_TIME = 1e-6f;

	/** Minimum acceptable distance for Character capsule to float above floor when walking. */
	const float MIN_FLOOR_DIST = 1.9f;

	/** Maximum acceptable distance for Character capsule to float above floor when walking. */
	const float MAX_FLOOR_DIST = 2.4f;

	/** Reject sweep impacts that are this close to the edge of the vertical portion of the capsule when performing vertical sweeps, and try again with a smaller capsule. */
	const float SWEEP_EDGE_REJECT_DISTANCE = 0.15f;

	/** Stop completely when braking and velocity magnitude is lower than this. */
	const float BRAKE_TO_STOP_VELOCITY = 10.f;

	const float PENETRATION_PULLBACK_DISTANCE = 0.125f;

	const float PENETRATION_OVERLAP_INFLATION = 0.100f;
}

void UMassSurfaceMovementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassVelocityFragment>();
	//BuildContext.AddFragment<FMassForceFragment>();
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassSurfaceMovementFragment>();
	BuildContext.AddTag<FMassSurfaceMovementTag>();
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct MovementParamsFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParams);
	const FConstSharedStruct SurfaceMovementParamsFragment = EntityManager.GetOrCreateConstSharedFragment(SurfaceMovementParams);
	BuildContext.AddConstSharedFragment(MovementParamsFragment);
	BuildContext.AddConstSharedFragment(SurfaceMovementParamsFragment);
}

UMassApplySurfaceMovementProcessor::UMassApplySurfaceMovementProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UMassApplySurfaceMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	//EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FMassSurfaceMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassSurfaceMovementTag>(EMassFragmentPresence::All);

	EntityQuery.AddConstSharedRequirement<FMassSurfaceMovementParams>(EMassFragmentPresence::All);

	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
}

void UMassApplySurfaceMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	check(World);
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([World](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		
		const FMassSurfaceMovementParams& SurfaceMovementParams = Context.GetConstSharedFragment<FMassSurfaceMovementParams>();
	
		const TArrayView<FMassVelocityFragment> VelocitiesList = Context.GetMutableFragmentView<FMassVelocityFragment>();
		//const TArrayView<FMassForceFragment> ForcesList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();

		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
	
		const TArrayView<FMassSurfaceMovementFragment> SurfaceMovementList = Context.GetMutableFragmentView<FMassSurfaceMovementFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			SCOPE_CYCLE_COUNTER(STAT_SurfaceMovement);

			FVector& OutVelocity = VelocitiesList[EntityIndex].Value;
			FVector Velocity = OutVelocity;  // copy velocity vector
			//const FVector& Force = ForcesList[EntityIndex].Value;
			const float Radius = RadiusList[EntityIndex].Radius;
			FMassSurfaceMovementFragment& SurfaceMovementFrag = SurfaceMovementList[EntityIndex];
			FTransform& Transform = TransformList[EntityIndex].GetMutableTransform();
			
			FMassSurfaceMovementFloor NewFloor;
			bool bFloorUpdated = false;

			FVector CurrentLocation = Transform.GetLocation();

			DrawDebugSphere(World, CurrentLocation, Radius, 12, FColor::Red, false, 0);
			// impact velocity from forces
			//Velocity += Force * DeltaTime;
			// keep velocity horizontal
			Velocity.Z = 0.f;

			float LastMoveTimeSlice = DeltaTime;

			const FVector Delta = Velocity * DeltaTime;
			FVector RampDelta = SurfaceMovementFrag.ComputeGroundMovementDelta(Delta);

			//DrawDebugLine(World, SurfaceMovementFrag.Floor.TempDbgSurfaceLocation, SurfaceMovementFrag.Floor.TempDbgSurfaceLocation + SurfaceMovementFrag.Floor.SurfaceNormal * 200.f, FColor::Red, false, 0);
			
			// sweep
			FHitResult Sweep(1.f);
			SurfaceMovementParams.SweepAlongSurfaceHandlePenetration(World, Radius, CurrentLocation, RampDelta, Sweep);  // TRACE COUNT 1
			CurrentLocation = Sweep.bBlockingHit ? Sweep.Location : Sweep.TraceEnd;
			
			if (Sweep.bStartPenetrating)  // stuck in geometry DONE
			{
				SurfaceMovementParams.SlideAlongSurfaceResolvePenetration(World, 1.f, Radius, CurrentLocation, Delta, SurfaceMovementFrag, Sweep);  // TRACE COUNT 3
				CurrentLocation = Sweep.bBlockingHit ? Sweep.Location : Sweep.TraceEnd;
			}
			else if (Sweep.bBlockingHit)  // can be hit wall or stairs up or ramp
			{
				// We impacted something (most likely another ramp, but possibly a barrier).
				float PercentTimeApplied = Sweep.Time;
				if ((Sweep.Time > 0.f) && (Sweep.Normal.Z > UE_KINDA_SMALL_NUMBER) && SurfaceMovementParams.SurfaceIsWalkable(Sweep))  // if not started with blocked hit, if not wall or extreme angle ramp
				{
					// Another walkable ramp
					const float InitialPercentRemaining = 1.f - PercentTimeApplied;
					RampDelta = SurfaceMovementFrag.ComputeGroundMovementDelta(Delta * InitialPercentRemaining);
					LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
					SurfaceMovementParams.SweepAlongSurfaceHandlePenetration(World, Radius, CurrentLocation, RampDelta, Sweep);  // TRACE COUNT 2
					CurrentLocation = Sweep.bBlockingHit ? Sweep.Location : Sweep.TraceEnd;

					const float SecondHitPercent = Sweep.Time * InitialPercentRemaining;
					PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
				}

				if (Sweep.bBlockingHit)
				{
					// hit a barrier, try to step up
					const FVector PreStepUpLocation = CurrentLocation;

					const bool SteppedUp = SurfaceMovementParams.StepUp(World, Radius, CurrentLocation, Delta * (1.f - PercentTimeApplied), SurfaceMovementFrag, Sweep, NewFloor);
					if (!SteppedUp)
					{
						SurfaceMovementParams.SlideAlongSurface(World, 1.f - PercentTimeApplied, Radius, CurrentLocation, Delta, SurfaceMovementFrag, Sweep);
						CurrentLocation = Sweep.bBlockingHit ? Sweep.Location : Sweep.TraceEnd;
					}
					else
					{
						bFloorUpdated = true;
						CurrentLocation = Sweep.bBlockingHit ? Sweep.Location : Sweep.TraceEnd;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaTime;
						if (StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						{
							Velocity = (CurrentLocation - PreStepUpLocation) / StepUpTimeSlice;
							Velocity.Z = 0;
						}
					}
				}
			}

			if (!bFloorUpdated)  // update floor
			{
				SurfaceMovementParams.FindFloor(World, CurrentLocation, Radius, NewFloor);
			}

#if WITH_MASSGAMEPLAY_DEBUG
			if (UE::MassMovement::bFreezeMovement)
			{
				Velocity = FVector::ZeroVector;
			}
#endif // WITH_MASSGAMEPLAY_DEBUG

			// Write FMassVelocityFragment 
			OutVelocity = Velocity;			
			
			// Write FTransformFragment
			Transform.SetTranslation(CurrentLocation);

			// Write FMassSurfaceMovementFragment
			SurfaceMovementFrag.Floor = NewFloor;
		}
	}));
}

UMassApplySurfaceMovementInitializer::UMassApplySurfaceMovementInitializer()
	: EntityQuery(*this)
{
	ObservedType = FMassSurfaceMovementFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UMassApplySurfaceMovementInitializer::Register()
{
	check(ObservedType);
    UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
}

void UMassApplySurfaceMovementInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassSurfaceMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FMassSurfaceMovementParams>();
}

void UMassApplySurfaceMovementInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [World](FMassExecutionContext Context)
	{
		const TArrayView<FMassSurfaceMovementFragment> SurfaceMovementList = Context.GetMutableFragmentView<FMassSurfaceMovementFragment>();
		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const FMassSurfaceMovementParams& SurfaceMovementParams = Context.GetConstSharedFragment<FMassSurfaceMovementParams>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FTransform& Transform = TransformList[EntityIndex].GetMutableTransform();
			FVector AgentLocation = Transform.GetLocation();
			const float AgentRadius = RadiusList[EntityIndex].Radius;
			AgentLocation.Z += AgentRadius;
			Transform.SetTranslation(AgentLocation);
			FMassSurfaceMovementFragment& SurfaceMovementFragment = SurfaceMovementList[EntityIndex];
			SurfaceMovementFragment.MovementMode = ESurfaceMovementMode::Walking;

			DrawDebugSphere(World, AgentLocation, AgentRadius, 8, FColor::Yellow, false, 10.f);
			
			SurfaceMovementParams.FindFloor(World, AgentLocation, AgentRadius, SurfaceMovementFragment.Floor);
		}
	});
}

void FMassSurfaceMovementParams::FindFloor(const UWorld* World, const FVector& AgentLocation, float AgentRadius,
                                           FMassSurfaceMovementFloor& OutFloor, ESurfaceMovementMode MovementMode) const
{
	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (MovementMode == ESurfaceMovementMode::Walking ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);
	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, StepHeight + HeightCheckAdjust);

	OutFloor.Clear();
	
	// Sweep floor
	if (FloorSweepTraceDist > 0.f)
	{

		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		float ShrinkHeight = (AgentRadius * 0.5f) * (1.f - ShrinkScale);
		float TraceDist = FloorSweepTraceDist + ShrinkHeight;

		FCollisionShape TraceSphere;
		TraceSphere.SetSphere(AgentRadius);
		
		FHitResult Hit(1.f);
		bool bBlockingHit = World->SweepSingleByChannel(Hit, AgentLocation, AgentLocation + FVector(0.f,0.f,-TraceDist), FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(AgentLocation, Hit.ImpactPoint, AgentRadius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				TraceSphere.Sphere.Radius = FMath::Max(0.f, AgentRadius - SWEEP_EDGE_REJECT_DISTANCE - UE_KINDA_SMALL_NUMBER);
				if (!TraceSphere.IsNearlyZero())
				{
					Hit.Reset(1.f, false);

					bBlockingHit = World->SweepSingleByChannel(Hit, AgentLocation, AgentLocation + FVector(0.f,0.f,-TraceDist), FQuat::Identity, CollisionChannel, TraceSphere, CollisionQueryParams);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, AgentRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloor.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && SurfaceIsWalkable(Hit))
			{		
				if (SweepResult <= FloorSweepTraceDist)
				{
					// Hit within test distance.
					OutFloor.bHasFloor = true;
				}
			}
		}
	}
}

float FMassSurfaceMovementParams::SlideAlongSurface(const UWorld* World, float Time, float AgentRadius,
                                                    const FVector& AgentLocation, const FVector& Delta, const FMassSurfaceMovementFragment& SurfaceMovementFragment,
                                                    FHitResult& OutHit) const
{
	if (!OutHit.bBlockingHit)
	{
		return 0.f;
	}

	const FMassSurfaceMovementFloor& CurrentFloor = SurfaceMovementFragment.Floor;
	
	FVector Normal(OutHit.ImpactNormal);
	if (SurfaceMovementFragment.MovementMode == ESurfaceMovementMode::Walking)
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!SurfaceIsWalkable(OutHit))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.GetSurfaceDistance() < MIN_FLOOR_DIST && CurrentFloor.bHasSurface())
			{
				const FVector FloorNormal = CurrentFloor.SurfaceNormal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
				
				Normal = Normal.GetSafeNormal2D();
			}
		}
	}
	
	const FVector OldHitNormal = Normal;
	FVector SlideDelta = ComputeSlideDelta(Delta, Time, Normal, CurrentFloor, AgentRadius);
	if ((SlideDelta | Delta) > 0.f)
	{
		SweepAlongSurfaceHandlePenetration(World, AgentRadius, AgentLocation, SlideDelta, OutHit);  // todo safe move resolve penetr
		
		const float FirstHitPercent = OutHit.Time;
		float PercentTimeApplied = FirstHitPercent;
		
		if (OutHit.IsValidBlockingHit())
		{
			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, OutHit, OldHitNormal, CurrentFloor, AgentRadius);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				SweepAlongSurfaceHandlePenetration(World, AgentRadius, OutHit.Location, SlideDelta, OutHit);  // todo save move resolve penetr
				const float SecondHitPercent = OutHit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}

float FMassSurfaceMovementParams::SlideAlongSurfaceResolvePenetration(const UWorld* World, const float Time,
	const float AgentRadius, const FVector& AgentLocation, const FVector& Delta,
	const FMassSurfaceMovementFragment& SurfaceMovementFragment, FHitResult& OutHit) const
{
		if (!OutHit.bBlockingHit)
	{
		return 0.f;
	}

	const FMassSurfaceMovementFloor& CurrentFloor = SurfaceMovementFragment.Floor;
	
	FVector Normal(OutHit.ImpactNormal);
	if (SurfaceMovementFragment.MovementMode == ESurfaceMovementMode::Walking)
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!SurfaceIsWalkable(OutHit))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.GetSurfaceDistance() < MIN_FLOOR_DIST && CurrentFloor.bHasSurface())
			{
				const FVector FloorNormal = CurrentFloor.SurfaceNormal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
				
				Normal = Normal.GetSafeNormal2D();
			}
		}
	}
	
	const FVector OldHitNormal = Normal;
	FVector SlideDelta = ComputeSlideDelta(Delta, Time, Normal, CurrentFloor, AgentRadius);
	if ((SlideDelta | Delta) > 0.f)
	{
		SweepAlongSurfaceHandlePenetration(World, AgentRadius, AgentLocation, SlideDelta, OutHit);  // todo safe move resolve penetr
		
		const float FirstHitPercent = OutHit.Time;
		float PercentTimeApplied = FirstHitPercent;
		
		if (OutHit.IsValidBlockingHit())
		{
			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, OutHit, OldHitNormal, CurrentFloor, AgentRadius);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				SweepAlongSurfaceHandlePenetration(World, AgentRadius, OutHit.Location, SlideDelta, OutHit);  // todo save move resolve penetr
				const float SecondHitPercent = OutHit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}

void FMassSurfaceMovementParams::TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal,
                                               const FMassSurfaceMovementFloor& Floor, float AgentRadius) const
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

bool FMassSurfaceMovementParams::StepUp(const UWorld* World, const float AgentRadius, const FVector& AgentLocation,
                                        const FVector& Delta, const FMassSurfaceMovementFragment& SurfaceMovementFragment, FHitResult& OutHit,
                                        FMassSurfaceMovementFloor& OutFloor) const
{
	if (StepHeight <= 0.f)
	{
		return false;
	}

	const FVector OldLocation = AgentLocation;
	const float HalfRadius = AgentRadius * 0.5f;
	
	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = OutHit.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + AgentRadius * 0.5)
	{
		return false;
	}

	const FVector GravDir (0.f, 0.f, -1.f);

	float StepTravelUpHeight = StepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(OutHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - HalfRadius;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	if (SurfaceMovementFragment.Floor.bHasSurface())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, SurfaceMovementFragment.Floor.GetSurfaceDistance());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (StepHeight + MaxFloorTraceDist * 2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(OutHit.Location, OutHit.ImpactPoint, AgentRadius);
		if (!bHitVerticalFace)
		{
			PawnFloorPointZ = SurfaceMovementFragment.Floor.SurfaceNormal.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= SurfaceMovementFragment.Floor.GetSurfaceDistance();
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	SweepAlongSurfaceHandlePenetration(World, AgentRadius, AgentLocation, -GravDir * StepTravelUpHeight, SweepUpHit);
	FVector NewLocation = SweepUpHit.bBlockingHit ? SweepUpHit.Location : SweepUpHit.TraceEnd;

	if (SweepUpHit.bStartPenetrating)
	{
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	SweepAlongSurfaceHandlePenetration(World, AgentRadius, NewLocation, -GravDir * StepTravelUpHeight, Hit);
	NewLocation = Hit.bBlockingHit ? Hit.Location : Hit.TraceEnd;

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			return false;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(World, 1.f - Hit.Time, AgentRadius, NewLocation, Delta, SurfaceMovementFragment, Hit);
		NewLocation = Hit.bBlockingHit ? Hit.Location : Hit.TraceEnd;
		
		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			return false;
		}
	}
	
	// Step down
	SweepAlongSurfaceHandlePenetration(World, AgentRadius, NewLocation, GravDir * StepTravelDownHeight, Hit);
	NewLocation = Hit.bBlockingHit ? Hit.Location : Hit.TraceEnd;
	
	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		return false;
	}

	FMassSurfaceMovementFloor TestFloor;
	bool bFloorFound = false;
	
	if (Hit.IsValidBlockingHit())
	{	
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > StepHeight)
		{
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!SurfaceIsWalkable(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (Hit.Location.Z > OldLocation.Z)
			{
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, AgentRadius))
		{
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f)
		{
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		FindFloor(World, NewLocation, AgentRadius, TestFloor);

		// Reject unwalkable normals if we end up higher than our initial height.
		// It's fine to walk down onto an unwalkable surface, don't reject those moves.
		if (Hit.Location.Z > OldLocation.Z)
		{
			// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
			// In those cases we should instead abort the step up and try to slide along the stair.
			if (!OutFloor.bHasSurface() && StepSideZ < MAX_STEP_SIDE_Z)
			{
				return false;
			}
		}

		bFloorFound = true;
	}

	if (bFloorFound) OutFloor = TestFloor;
	OutHit = Hit;
	
	return true;
}
