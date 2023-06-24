// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassSurfaceMovement.h"

#include "CharacterMovementComponentAsync.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationLOD.h"
#include "MassCommandBuffer.h"
#include "MassObserverRegistry.h"
#include "Mass/Collision/ETW_MassCollisionTypes.h"
#include "Engine/ScopedMovementUpdate.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/UnitConversion.h"
#include "Math/UnrealMathUtility.h"
#include "BodyInstanceCore.h"


void UMassSurfaceMovementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassVelocityFragment>();
	//BuildContext.AddFragment<FMassForceFragment>();
	BuildContext.RequireFragment<FETW_MassCopsuleFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassSurfaceMovementFragment>();
	BuildContext.AddTag<FMassSurfaceMovementTag>();
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct MovementParamsFragment = EntityManager.GetOrCreateConstSharedFragment(MovementParams);
	const FConstSharedStruct SurfaceMovementParamsFragment = EntityManager.GetOrCreateConstSharedFragment(SurfaceMovementParams);
	BuildContext.AddConstSharedFragment(MovementParamsFragment);
	BuildContext.AddConstSharedFragment(SurfaceMovementParamsFragment);
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
			SurfaceMovementFragment.MovementMode = EMassSurfaceMovementMode::Walking;

			// DrawDebugSphere(World, AgentLocation, AgentRadius, 8, FColor::Yellow, false, 10.f);
			
			// SurfaceMovementParams.FindFloor(World, AgentLocation, AgentRadius, SurfaceMovementFragment.Floor);
		}
	});
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
	EntityQuery.AddTagRequirement<FMassSurfaceMovementTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);

	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FETW_MassCopsuleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSurfaceMovementFragment>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddConstSharedRequirement<FMassSurfaceMovementParams>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FETW_MassCapsuleCollisionParams>(EMassFragmentPresence::All);
}

void UMassApplySurfaceMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	check(World);
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, ([World](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		
		const FMassSurfaceMovementParams& SurfaceMovementParams = Context.GetConstSharedFragment<FMassSurfaceMovementParams>();
		const FETW_MassCapsuleCollisionParams& CapsuleCollisionParams = Context.GetConstSharedFragment<FETW_MassCapsuleCollisionParams>();
	
		const TArrayView<FMassVelocityFragment> VelocitiesList = Context.GetMutableFragmentView<FMassVelocityFragment>();
		//const TArrayView<FMassForceFragment> ForcesList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();

		const TConstArrayView<FETW_MassCopsuleFragment> CapsuleList = Context.GetFragmentView<FETW_MassCopsuleFragment>();
	
		const TArrayView<FMassSurfaceMovementFragment> SurfaceMovementList = Context.GetMutableFragmentView<FMassSurfaceMovementFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovement);

			FVector& OutVelocity = VelocitiesList[EntityIndex].Value;
			FVector Velocity = OutVelocity;  // copy velocity vector
			//const FVector& Force = ForcesList[EntityIndex].Value;

			const float Radius = CapsuleCollisionParams.CapsuleRadius;
			FMassSurfaceMovementFragment& SurfaceMovementFrag = SurfaceMovementList[EntityIndex];
			FTransform& Transform = TransformList[EntityIndex].GetMutableTransform();

			FVector CurrentLocation = Transform.GetLocation();

			// impact velocity from forces
			//Velocity += Force * DeltaTime;
			// keep velocity horizontal
			Velocity.Z = 0.f;

			// todo:

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
		}
	}));
}

bool UMassApplySurfaceMovementProcessor::SafeMoveUpdatedComponent(FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FVector& Delta, const FQuat& NewRotation, bool bSweep,
	FHitResult& OutHit, ETeleportType Teleport) const
{
	EMoveComponentFlags& MoveComponentFlags = MoveFrag.MoveComponentFlags;
		
	bool bMoveResult = false;

	// Scope for move flags
	{
		// Conditionally ignore blocking overlaps (based on CVar)
		const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
		TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MOVE_IGNORE_FIRST_BLOCKING_OVERLAP ? MoveComponentFlags : (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents));
		bMoveResult = MoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, NewRotation, bSweep, &OutHit, Teleport);
	}

	// Handle initial penetrations
	if (OutHit.bStartPenetrating)
	{
		const FVector RequestedAdjustment = GetPenetrationAdjustment(OutHit);
		if (ResolvePenetration(CapsuleFrag, MoveFrag, RequestedAdjustment, OutHit, NewRotation))
		{
			// Retry original move
			bMoveResult = MoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, NewRotation, bSweep, &OutHit, Teleport);
		}
	}

	return bMoveResult;
}

bool UMassApplySurfaceMovementProcessor::ResolvePenetration(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotationQuat) const
{
	// SceneComponent can't be in penetration, so this function really only applies to PrimitiveComponent.
	if (!Adjustment.IsZero())
	{
		SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovementResolvePenetration);

		UPrimitiveComponent* UpdatedPrimitive = CapsuleFrag.GetMutableCapsuleComponent();
		
		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.
		constexpr float OverlapInflation = PENETRATION_OVERLAP_INFLATION;
		bool bEncroached = OverlapTest(CapsuleFrag, Hit.TraceStart + Adjustment, NewRotationQuat, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(OverlapInflation));
		if (!bEncroached)
		{
			// Move without sweeping.
			MoveUpdatedComponent(CapsuleFrag, MoveFrag, Adjustment, NewRotationQuat, false, nullptr, ETeleportType::TeleportPhysics);
			return true;
		}
		else
		{
			EMoveComponentFlags& MoveComponentFlags = MoveFrag.MoveComponentFlags;
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);
			bool bMoved = MoveUpdatedComponent(CapsuleFrag, MoveFrag, Adjustment, NewRotationQuat, true, &SweepOutHit, ETeleportType::TeleportPhysics);
			
			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = MoveUpdatedComponent(CapsuleFrag, MoveFrag, CombinedMTD, NewRotationQuat, true, nullptr, ETeleportType::TeleportPhysics);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = Hit.TraceEnd - Hit.TraceStart;
				if (!MoveDelta.IsZero())
				{
					bMoved = MoveUpdatedComponent(CapsuleFrag, MoveFrag, Adjustment + MoveDelta, NewRotationQuat, true, nullptr, ETeleportType::TeleportPhysics);

					// Finally, try the original move without MTD adjustments, but allowing depenetration along the MTD normal.
					// This was blocked because MOVECOMP_NeverIgnoreBlockingOverlaps was true for the original move to try a better depenetration normal, but we might be running in to other geometry in the attempt.
					// This won't necessarily get us all the way out of penetration, but can in some cases and does make progress in exiting the penetration.
					if (!bMoved && FVector::DotProduct(MoveDelta, Adjustment) > 0.f)
					{
						bMoved = MoveUpdatedComponent(CapsuleFrag, MoveFrag, MoveDelta, NewRotationQuat, true, nullptr, ETeleportType::TeleportPhysics);
					}
				}
			}

			return bMoved;
		}
	}

	return false;
}

bool UMassApplySurfaceMovementProcessor::ComputePerchResult(const FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const float TestRadius,
	const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	float PawnRadius, PawnHalfHeight;
	CapsuleFrag.GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
	const FVector CapsuleLocation = (MoveParams.bUseFlatBaseForFloorChecks ? InHit.TraceStart : InHit.Location);

	const float InHitAboveBase = FMath::Max<float>(0.f, InHit.ImpactPoint.Z - (CapsuleLocation.Z - PawnHalfHeight));
	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(CapsuleFrag, MoveFrag, MoveParams, CapsuleLocation, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return true;
}

bool UMassApplySurfaceMovementProcessor::FloorSweepTest(const FMassSurfaceMovementParams& MoveParams,
                                                        FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel,
                                                        const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params,
                                                        const FCollisionResponseParams& ResponseParam) const
{
	bool bBlockingHit = false;

	if (!MoveParams.bUseFlatBaseForFloorChecks)
	{
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, CollisionShape, Params, ResponseParam);
	}
	else
	{
		// Test with a box that is enclosed by the capsule.
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		// First test with the box rotated so the corners are along the major axes (ie rotated 45 degrees).
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(FVector(0.f, 0.f, -1.f), UE_PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParam);

		if (!bBlockingHit)
		{
			// Test again with the same box, not rotated.
			OutHit.Reset(1.f, false);
			bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, BoxShape, Params, ResponseParam);
		}
	}

	return bBlockingHit;
}

void UMassApplySurfaceMovementProcessor::ComputeFloorDist(const FETW_MassCopsuleFragment& CapsuleFrag,
                                                          FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams,
                                                          const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult,
                                                          float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	OutFloorResult.Clear();
	
	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetCapsuleComponent();
	
	float PawnRadius, PawnHalfHeight;
	UpdatedComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{
		// Only if the supplied sweep was vertical and downward.
		if ((DownwardSweepResult->TraceStart.Z > DownwardSweepResult->TraceEnd.Z) &&
			(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).SizeSquared2D() <= UE_KINDA_SMALL_NUMBER)
		{
			// Reject hits that are barely on the cusp of the radius of the capsule
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(MoveParams, *DownwardSweepResult);
				const float FloorDist = (CapsuleLocation.Z - DownwardSweepResult->Location.Z);
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);
				
				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
	}

	// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	bool bBlockingHit = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(STAT_SurfaceMovementFloorDist), false);
	FCollisionResponseParams ResponseParam;
	UpdatedComponent->InitSweepCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// Sweep test
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		bBlockingHit = FloorSweepTest(MoveParams, Hit, CapsuleLocation, CapsuleLocation + FVector(0.f,0.f,-TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - UE_KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(MoveParams, Hit, CapsuleLocation, CapsuleLocation + FVector(0.f,0.f,-TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(MoveParams, Hit))
			{		
				if (SweepResult <= SweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}

	// Line trace
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;	
		const float TraceDist = LineDistance + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(MoveParams, Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}
	
	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

bool UMassApplySurfaceMovementProcessor::StepUp(FETW_MassCopsuleFragment& CapsuleFrag,
                                                FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& GravDir,
                                                const FVector& Delta, const FHitResult& InHit, FStepDownResult* OutStepDownResult) const
{
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovementStepUp);

	const float MaxStepHeight = MoveParams.MaxStepHeight;
	
	if (!CanStepUp(MoveFrag, InHit) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	float PawnRadius, PawnHalfHeight;
	UpdatedComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = InHit.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		return false;
	}

	if (GravDir.IsZero())
	{
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	if (IsMovingOnGround(MoveFrag) && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + MAX_FLOOR_DIST*2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(CapsuleFrag, MoveFrag, -GravDir * StepTravelUpHeight, PawnRotation, true, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, PawnRotation, true, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(CapsuleFrag, MoveFrag, SweepUpHit);
		}

		// pawn ran into a wall
		HandleImpact(CapsuleFrag, MoveFrag, SweepUpHit);
		if (IsFalling(MoveFrag))
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(CapsuleFrag, MoveFrag, MoveParams, Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		
		if (IsFalling(MoveFrag))
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}
	
	// Step down
	MoveUpdatedComponent(CapsuleFrag, MoveFrag, GravDir * StepTravelDownHeight, UpdatedComponent->GetComponentQuat(), true, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{	
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkable(MoveParams, Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (Hit.Location.Z > OldLocation.Z)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUp(MoveFrag, Hit))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(CapsuleFrag, MoveFrag, MoveParams,UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (Hit.Location.Z > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < MAX_STEP_SIDE_Z)
				{
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bComputedFloor = true;
		}
	}
	
	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	MoveFrag.bJustTeleported |= true;

	return true;
}

void UMassApplySurfaceMovementProcessor::TwoWallAdjust(FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = Delta;
	// Super::TwoWallAdjust(Delta, Hit, OldHitNormal);
	{
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
			Delta = ComputeSlideVector(Delta, 1.f - Hit.Time, HitNormal, Hit);
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
	}

	if (IsMovingOnGround(MoveFrag))
	{
		// Allow slides up walkable surfaces, but not unwalkable ones (treat those as vertical barriers).
		if (Delta.Z > 0.f)
		{
			if ((Hit.Normal.Z >= MoveParams.WalkableFloorZ || IsWalkable(MoveParams, Hit)) && Hit.Normal.Z > UE_KINDA_SMALL_NUMBER)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = Delta.GetSafeNormal() * InDelta.Size();
				Delta = FVector(InDelta.X, InDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;

				// Should never exceed MaxStepHeight in vertical component, so rescale if necessary.
				// This should be rare (Hit.Normal.Z above would have been very small) but we'd rather lose horizontal velocity than go too high.
				if (Delta.Z > MoveParams.MaxStepHeight)
				{
					const float Rescale = MoveParams.MaxStepHeight / Delta.Z;
					Delta *= Rescale;
				}
			}
			else
			{
				Delta.Z = 0.f;
			}
		}
		else if (Delta.Z < 0.f)
		{
			// Don't push down into the floor.
			if (MoveFrag.Floor.FloorDist < MIN_FLOOR_DIST && MoveFrag.Floor.bBlockingHit)
			{
				Delta.Z = 0.f;
			}
		}
	}
}

void UMassApplySurfaceMovementProcessor::FindFloor(FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& CapsuleLocation,
	FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_CharFindFloor);

	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	
	// No collision, no floor...
	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (IsMovingOnGround(MoveFrag) ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, MoveParams.MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;
	
	// Sweep floor
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		if ( MoveParams.bAlwaysCheckFloor || !bCanUseCachedLocation || MoveFrag.bForceNextFloorCheck || MoveFrag.bJustTeleported )
		{
			MoveFrag.bForceNextFloorCheck = false;
			ComputeFloorDist(CapsuleFrag, MoveFrag, MoveParams, CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, UpdatedComponent->GetScaledCapsuleRadius(), DownwardSweepResult);
		}
		else
		{
			// Force floor check if base has collision disabled or if it does not block us.
			UPrimitiveComponent* MovementBase = MoveFrag.BasedMovement.MovementBase;
			const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : NULL;
			const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

			if (MovementBase != NULL)
			{
				MoveFrag.bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
				|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block
				|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}
			MoveFrag.bForceNextFloorCheck = true;

			const bool IsActorBasePendingKill = BaseActor && !IsValid(BaseActor);
			
			//if (!MoveFrag.bForceNextFloorCheck)
			if ( !MoveFrag.bForceNextFloorCheck && !IsActorBasePendingKill && MovementBase )
			{
				OutFloorResult = MoveFrag.Floor;
				bNeedToValidateFloor = false;
			}
			else
			{
				MoveFrag.bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleFrag, MoveFrag, MoveParams, CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, UpdatedComponent->GetScaledCapsuleRadius(), DownwardSweepResult);
			}
		}
	}

	// OutFloorResult.HitResult is now the result of the vertical floor check.
	// See if we should try to "perch" at this location.
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		const bool bCheckRadius = true;
		if (ShouldComputePerchResult(CapsuleFrag, MoveParams, OutFloorResult.HitResult, bCheckRadius))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, MoveParams.MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround(MoveFrag))
			{
				MaxPerchFloorDist += FMath::Max(0.f, MoveParams.PerchAdditionalHeight);
			}

			FFindFloorResult PerchFloorResult;
			if (ComputePerchResult(CapsuleFrag, MoveFrag, MoveParams, GetValidPerchRadius(CapsuleFrag, MoveParams), OutFloorResult.HitResult, MaxPerchFloorDist, PerchFloorResult))
			{
				// Don't allow the floor distance adjustment to push us up too high, or we will move beyond the perch distance and fall next time.
				const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				// If the regular capsule is on an unwalkable surface but the perched one would allow us to stand, override the normal to be one that is walkable.
				if (!OutFloorResult.bWalkableFloor)
				{
					// Floor distances are used as the distance of the regular capsule to the point of collision, to make sure AdjustFloorHeight() behaves correctly.
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist, FMath::Max(OutFloorResult.FloorDist, MIN_FLOOR_DIST), true);
				}
			}
			else
			{
				// We had no floor (or an invalid one because it was unwalkable), and couldn't perch here, so invalidate floor (which will cause us to start falling).
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}
}

float UMassApplySurfaceMovementProcessor::SlideAlongSurface(FETW_MassCopsuleFragment& CapsuleFrag,
                                                            FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit,
                                                            bool bHandleImpact) const
{
	// movement component
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	if (IsMovingOnGround(MoveFrag))
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!IsWalkable(MoveParams, Hit))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			const FFindFloorResult& CurrentFloor = MoveFrag.Floor;
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
				
				Normal = Normal.GetSafeNormal2D();
			}
		}
	}

	// character movement component
	float PercentTimeApplied = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		const FQuat Rotation = CapsuleFrag.GetCapsuleComponent()->GetComponentQuat();
		SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, SlideDelta, Rotation, true, Hit);

		const float FirstHitPercent = Hit.Time;
		PercentTimeApplied = FirstHitPercent;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (bHandleImpact)
			{
				HandleImpact(CapsuleFrag, MoveFrag, Hit, FirstHitPercent * Time, SlideDelta);
			}

			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(MoveFrag, MoveParams, SlideDelta, Hit, OldHitNormal);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, SlideDelta, Rotation, true, Hit);
				const float SecondHitPercent = Hit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;

				// Notify second impact
				if (bHandleImpact && Hit.bBlockingHit)
				{
					HandleImpact(CapsuleFrag, MoveFrag,Hit, SecondHitPercent * Time, SlideDelta);
				}
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}

void UMassApplySurfaceMovementProcessor::MoveAlongFloor(FMassVelocityFragment& VelocityFrag, FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult) const
{
	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();

	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector Delta = FVector(InVelocity.X, InVelocity.Y, 0.f) * DeltaSeconds;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(MoveParams, Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;
	
	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(CapsuleFrag, MoveFrag, Hit);
		SlideAlongSurface(CapsuleFrag, MoveFrag, MoveParams, Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(MoveFrag, &Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (Hit.Normal.Z > UE_KINDA_SMALL_NUMBER) && IsWalkable(MoveParams, Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(MoveParams, Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			//if (CanStepUp(MoveFrag, Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			if (CanStepUp(MoveFrag, Hit))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				const FVector GravDir(0.f, 0.f, -1.f);
				if (!StepUp(CapsuleFrag, MoveFrag, MoveParams, GravDir, Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					HandleImpact(CapsuleFrag, MoveFrag, Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(CapsuleFrag, MoveFrag, MoveParams, Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					//if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						MoveFrag.bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						if (StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						{
							VelocityFrag.Value = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
							VelocityFrag.Value.Z = 0;
						}
					}
				}
			}
			//else if ( Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner) )
			//{
			//	HandleImpact(Hit, LastMoveTimeSlice, RampVector);
			//	SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			//}
		}
	}
}

void UMassApplySurfaceMovementProcessor::PhysWalking(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime) const
{
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovementWalking)

	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	//{
	//	Acceleration = FVector::ZeroVector;
	//	Velocity = FVector::ZeroVector;
	//	return;
	//}

	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	
	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, EMassSurfaceMovementMode::Walking);
		return;
	}
	
	MoveFrag.bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float RemainingTime = DeltaTime;

	// Perform the move
	//while ( (RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)) )
	if (RemainingTime >= MIN_TICK_TIME)
	{
		//MoveFrag.bJustTeleported = false;
		const float TimeTick = GetSimulationTimeStep(RemainingTime);
		RemainingTime -= TimeTick;

		// Save current values
		UPrimitiveComponent * const OldBase = MoveFrag.BasedMovement.MovementBase;
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		//RestorePreAdditiveRootMotionVelocity();

		FVector& Velocity = VelocityFrag.Value;
		FVector& Acceleration = ForceFrag.Value;
		
		// Ensure velocity is horizontal.
		//MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;
		Acceleration.Z = 0.f;

		// Apply acceleration
		//if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
		//{
		//	CalcVelocity(TimeTick, GroundFriction, false, GetMaxBrakingDeceleration());
		//	devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		//}

		CalcVelocity(VelocityFrag, ForceFrag, MoveFrag, SpeedParams, MoveParams, TimeTick, MoveParams.GroundFriction, false, GetMaxBrakingDeceleration(MoveFrag, MoveParams));
		
		//ApplyRootMotionToVelocity(TimeTick);
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if(IsFalling(MoveFrag))
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, RemainingTime+TimeTick);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = TimeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if ( bZeroDelta )
		{
			RemainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, MoveVelocity, TimeTick, &StepDownResult);

			if ( IsFalling(MoveFrag) )
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > UE_KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					RemainingTime += TimeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));
				}
				StartNewPhysics(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, RemainingTime);
				return;
			}
			//else if ( IsSwimming() ) //just entered water
			//{
			//	StartSwimming(OldLocation, OldVelocity, TimeTick, RemainingTime, Iterations);
			//	return;
			//}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(CapsuleFrag, MoveFrag, MoveParams, UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges(MoveParams);
		if ( bCheckLedges && !CurrentFloor.IsWalkableFloor() )
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f,0.f,-1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(CapsuleFrag, MoveParams, OldLocation, Delta, GravDir);
			if ( !NewDelta.IsZero() )
			{
				// first revert this move
				RevertMove(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, MoveParams, OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta/TimeTick;
				RemainingTime += TimeTick;
				//continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ( (bMustJump || !bCheckedFall) && CheckFall(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, OldFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, TimeTick, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, MoveParams, OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				RemainingTime = 0.f;
				//break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				//if (ShouldCatchAir(OldFloor, CurrentFloor))
				//{
				//	//HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, TimeTick);
				//	if (IsMovingOnGround(MoveFrag))
				//	{
				//		// If still walking, then fall. If not, assume the user set a different mode they want to keep.
				//		StartFalling(VelocityFrag, CapsuleFrag, MoveFrag, RemainingTime, TimeTick, Delta, OldLocation);
				//	}
				//	return;
				//}

				AdjustFloorHeight(CapsuleFrag, MoveFrag, MoveParams);
				SetBase(CapsuleFrag, MoveFrag, MoveParams, CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && RemainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(CapsuleFrag, MoveFrag, RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				MoveFrag.bForceNextFloorCheck = true;
			}

			//// check if just entered water
			//if ( IsSwimming() )
			//{
			//	StartSwimming(OldLocation, Velocity, TimeTick, RemainingTime, Iterations);
			//	return;
			//}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = MoveFrag.bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(VelocityFrag, ForceFrag,CapsuleFrag, MoveFrag, SpeedParams, MoveParams, OldFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, TimeTick, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;
			}
		}


		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround(MoveFrag))
		{
			// Make velocity reflect actual move
			//if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && TimeTick >= MIN_TICK_TIME)
			if( !MoveFrag.bJustTeleported && TimeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / TimeTick;
				//MaintainHorizontalGroundVelocity();
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			RemainingTime = 0.f;
		}	
	}

	//if (IsMovingOnGround(MoveFrag))
	//{
	//	MaintainHorizontalGroundVelocity();
	//}
}

void UMassApplySurfaceMovementProcessor::OnMovementModeChanged(FMassVelocityFragment& VelocityFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, EMassSurfaceMovementMode PreviousMovementMode) const
{
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	EMassSurfaceMovementMode& MovementMode = MoveFrag.MovementMode;

	FVector& Velocity = VelocityFrag.Value;
	
	//// Update collision settings if needed
	//if (MovementMode == MOVE_NavWalking)
	//{
	//	// Reset cached nav location used by NavWalking
	//	CachedNavLocation = FNavLocation();
    //
	//	GroundMovementMode = MovementMode;
	//	// Walking uses only XY velocity
	//	Velocity.Z = 0.f;
	//	SetNavWalkingPhysics(true);
	//}
	//else if (PreviousMovementMode == MOVE_NavWalking)
	//{
	//	if (MovementMode == DefaultLandMovementMode || IsWalking())
	//	{
	//		const bool bSucceeded = TryToLeaveNavWalking();
	//		if (!bSucceeded)
	//		{
	//			return;
	//		}
	//	}
	//	else
	//	{
	//		SetNavWalkingPhysics(false);
	//	}
	//}

	// React to changes in the movement mode.
	if (MoveFrag.MovementMode == EMassSurfaceMovementMode::Walking)
	{
		// Walking uses only XY velocity, and must be on a walkable floor, with a Base.
		Velocity.Z = 0.f;
		//bCrouchMaintainsBaseLocation = true;
		//GroundMovementMode = MovementMode;

		// make sure we update our new floor/base on initial entry of the walking physics
		FindFloor(CapsuleFrag, MoveFrag, MoveParams, UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		AdjustFloorHeight(CapsuleFrag, MoveFrag, MoveParams);
		SetBaseFromFloor(CapsuleFrag, MoveFrag, MoveParams, CurrentFloor);
	}
	else
	{
		CurrentFloor.Clear();
		//bCrouchMaintainsBaseLocation = false;

		if (MovementMode == EMassSurfaceMovementMode::Falling)
		{
			//DecayingFormerBaseVelocity = GetImpartedMovementBaseVelocity();
			const FVector DecayingFormerBaseVelocity = GetImpartedMovementBaseVelocity(CapsuleFrag, MoveFrag, MoveParams);
			Velocity += DecayingFormerBaseVelocity;
			//if (bMovementInProgress && CurrentRootMotion.HasAdditiveVelocity())
			//{
			//	// If we leave a base during movement and we have additive root motion, we need to add the imparted velocity so that it retains it next tick
			//	CurrentRootMotion.LastPreAdditiveVelocity += DecayingFormerBaseVelocity;
			//}
			//if (!CharacterMovementCVars::bAddFormerBaseVelocityToRootMotionOverrideWhenFalling || FormerBaseVelocityDecayHalfLife == 0.f)
			//{
			//	DecayingFormerBaseVelocity = FVector::ZeroVector;
			//}
			//CharacterOwner->Falling();
		}

		SetBase(CapsuleFrag, MoveFrag, MoveParams,NULL);

		//if (MovementMode == EMassSurfaceMovementMode::None)
		//{
		//	// Kill velocity and clear queued up events
		//	StopMovementKeepPathing();
		//	CharacterOwner->ResetJumpState();
		//	ClearAccumulatedForces();
		//}
	}

	//if (MovementMode == EMassSurfaceMovementMode::Falling && PreviousMovementMode != EMassSurfaceMovementMode::Falling)
	//{
	//	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	//	if (PFAgent)
	//	{
	//		PFAgent->OnStartedFalling();
	//	}
	//}

	//CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	// ensureMsgf(GroundMovementMode == MOVE_Walking || GroundMovementMode == MOVE_NavWalking, TEXT("Invalid GroundMovementMode %d. MovementMode: %d, PreviousMovementMode: %d"), GroundMovementMode.GetValue(), MovementMode.GetValue(), PreviousMovementMode);

}

void UMassApplySurfaceMovementProcessor::CalcVelocity(FMassVelocityFragment& VelocityFrag,
	FMassForceFragment& ForceFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams,
	const FMassSurfaceMovementParams& MoveParams, const float DeltaTime, float Friction, const bool bFluid, float BrakingDeceleration) const
{
		// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	//if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector& Velocity = VelocityFrag.Value;
	FVector& Acceleration = ForceFrag.Value;

	Friction = FMath::Max(0.f, Friction);
	// const float MaxAccel = GetMaxAcceleration(SpeedParams);
	float MaxSpeed = GetMaxSpeed(SpeedParams);
	
	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;

	//Use velocity requested by path following to compute a requested acceleration and speed.
	//if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	//{
	//	bZeroRequestedAcceleration = false;
	//}

	//if (bForceMaxAccel)
	//{
	//	// Force acceleration at full speed.
	//	// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
	//	if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
	//	{
	//		Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
	//	}
	//	else 
	//	{
	//		Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
	//	}
	//
	//	AnalogInputModifier = 1.f;
	//}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	//const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	//MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(Velocity, MaxSpeed);
	
	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		//const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		const float ActualBrakingFriction = MoveParams.GroundFriction;
		ApplyVelocityBraking(VelocityFrag, MoveParams, DeltaTime, ActualBrakingFriction, BrakingDeceleration);
	
		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(Velocity, MaxSpeed) ? Velocity.Size() : MaxSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(Velocity, RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	//if (bUseRVOAvoidance)
	//{
	//	CalcAvoidanceVelocity(DeltaTime);
	//}
}

void UMassApplySurfaceMovementProcessor::ApplyVelocityBraking(FMassVelocityFragment& VelocityFrag,
	const FMassSurfaceMovementParams& MoveParams, float DeltaTime, float Friction, float BrakingDeceleration) const
{
	FVector& Velocity = VelocityFrag.Value;
	
	if (Velocity.IsZero() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.f, MoveParams.BrakingFrictionFactor);
	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(MoveParams.BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while( RemainingTime >= MIN_TICK_TIME )
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt ; 
		
		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= UE_KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}

FVector UMassApplySurfaceMovementProcessor::GetLedgeMove(FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const
{
	if (Delta.IsZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDir(Delta.Y, -1.f * Delta.X, 0.f);
		
	// try left
	if ( CheckLedgeDirection(CapsuleFrag, MoveParams, OldLocation, SideDir, GravDir) )
	{
		return SideDir;
	}

	// try right
	SideDir *= -1.f;
	if ( CheckLedgeDirection(CapsuleFrag, MoveParams, OldLocation, SideDir, GravDir) )
	{
		return SideDir;
	}
	
	return FVector::ZeroVector;
}

bool UMassApplySurfaceMovementProcessor::CheckLedgeDirection(FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const
{
	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	
	const FVector SideDest = OldLocation + SideStep;
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(STAT_SurfaceMovementCheckLedgeDirection), false);
	FCollisionResponseParams ResponseParam;
	UpdatedComponent->InitSweepCollisionParams(CapsuleParams, ResponseParam);
	const FCollisionShape CapsuleShape = GetPawnCapsuleCollisionShape(UpdatedComponent, SHRINK_None);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	FHitResult Result(1.f);
	GetWorld()->SweepSingleByChannel(Result, OldLocation, SideDest, FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);

	if ( !Result.bBlockingHit || IsWalkable(MoveParams, Result) )
	{
		if ( !Result.bBlockingHit )
		{
			GetWorld()->SweepSingleByChannel(Result, SideDest, SideDest + GravDir * (MoveParams.MaxStepHeight + MoveParams.LedgeCheckThreshold), FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);
		}
		if ( (Result.Time < 1.f) && IsWalkable(MoveParams, Result) )
		{
			return true;
		}
	}
	return false;
}

void UMassApplySurfaceMovementProcessor::RevertMove(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams,
	const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& PreviousBaseLocation, const FFindFloorResult& OldFloor, bool bFailMove) const
{
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	//UE_LOG(LogCharacterMovement, Log, TEXT("RevertMove from %f %f %f to %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z, OldLocation.X, OldLocation.Y, OldLocation.Z);
	//UpdatedComponent->SetWorldLocation(OldLocation, false, nullptr, GetTeleportType());
	UpdatedComponent->SetWorldLocation(OldLocation, false, nullptr, ETeleportType::None);
	
	//UE_LOG(LogCharacterMovement, Log, TEXT("Now at %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z);
	MoveFrag.bJustTeleported = false;
	// if our previous base couldn't have moved or changed in any physics-affecting way, restore it
	if (IsValid(OldBase) && 
		(!MovementBaseUtility::IsDynamicBase(OldBase) ||
		 (OldBase->Mobility == EComponentMobility::Static) ||
		 (OldBase->GetComponentLocation() == PreviousBaseLocation)
		)
	   )
	{
		MoveFrag.Floor = OldFloor;
		SetBase(CapsuleFrag, MoveFrag, MoveParams,OldBase, OldFloor.HitResult.BoneName);
	}
	else
	{
		SetBase(CapsuleFrag, MoveFrag, MoveParams, NULL);
	}

	if ( bFailMove )
	{
		// end movement now
		VelocityFrag.Value = FVector::ZeroVector;
		ForceFrag.Value = FVector::ZeroVector;
		//UE_LOG(LogCharacterMovement, Log, TEXT("%s FAILMOVE RevertMove"), *CharacterOwner->GetName());
	}
}

void UMassApplySurfaceMovementProcessor::AdjustFloorHeight(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const
{
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovementCharAdjustFloorHeight)

	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	
	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const float InitialZ = UpdatedComponent->GetComponentLocation().Z;
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;
		SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, FVector(0.f,0.f,MoveDist), UpdatedComponent->GetComponentQuat(), true, AdjustHit );
		//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
			if (IsWalkable(MoveParams, AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		//MoveFrag.bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);
		MoveFrag.bJustTeleported |= true;
		
		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		//if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
		//{
		//	bForceNextFloorCheck = true;
		//}
	}
}

void UMassApplySurfaceMovementProcessor::SaveBaseLocation(FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const
{
	FBasedMovementInfo& BasedMovement = MoveFrag.BasedMovement;
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	const UPrimitiveComponent* MovementBase = BasedMovement.MovementBase;
	if (MovementBase)
	{
		// Read transforms into OldBaseLocation, OldBaseQuat. Do this regardless of whether the object is movable, since mobility can change.
		MovementBaseUtility::GetMovementBaseTransform(MovementBase, BasedMovement.BoneName, MoveFrag.OldBaseLocation, MoveFrag.OldBaseQuat);

		if (MovementBaseUtility::UseRelativeLocation(MovementBase))
		{
			// Relative Location
			FVector RelativeLocation;

			MovementBaseUtility::TransformLocationToLocal(MovementBase, BasedMovement.BoneName, UpdatedComponent->GetComponentLocation(), RelativeLocation);

			// Rotation
			if (MoveParams.bIgnoreBaseRotation)
			{
				// Absolute rotation
				SaveRelativeBasedMovement(MoveFrag, RelativeLocation, UpdatedComponent->GetComponentRotation(), false);
			}
			else
			{
				// Relative rotation
				const FRotator RelativeRotation = (FQuatRotationMatrix(UpdatedComponent->GetComponentQuat()) * FQuatRotationMatrix(MoveFrag.OldBaseQuat).GetTransposed()).Rotator();
				SaveRelativeBasedMovement(MoveFrag, RelativeLocation, RelativeRotation, true);
			}
		}
	}
}

void UMassApplySurfaceMovementProcessor::SetBase(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams,
                                                 UPrimitiveComponent* NewBaseComponent, const FName InBoneName) const
{
	FFindFloorResult& CurrentFloor = MoveFrag.Floor;
	
	// If NewBaseComponent is nullptr, ignore bone name.
	const FName BoneName = (NewBaseComponent ? InBoneName : NAME_None);

	FBasedMovementInfo& BasedMovement = MoveFrag.BasedMovement;
		
	// See what changed.
	const bool bBaseChanged = (NewBaseComponent != BasedMovement.MovementBase);
	const bool bBoneChanged = (BoneName != BasedMovement.BoneName);

	if (bBaseChanged || bBoneChanged)
	{
		// Verify no recursion.
		APawn* Loop = (NewBaseComponent ? Cast<APawn>(NewBaseComponent->GetOwner()) : nullptr);
		while (Loop)
		{
			//if (Loop == this)
			//{
			//	//UE_LOG(LogCharacter, Warning, TEXT(" SetBase failed! Recursion detected. Pawn %s already based on %s."), *GetName(), *NewBaseComponent->GetName()); //-V595
			//	return;
			//}
			if (UPrimitiveComponent* LoopBase =	Loop->GetMovementBase())
			{
				Loop = Cast<APawn>(LoopBase->GetOwner());
			}
			else
			{
				break;
			}
		}

		// Set base.
		//UPrimitiveComponent* OldBase = BasedMovement.MovementBase;
		BasedMovement.MovementBase = NewBaseComponent;
		BasedMovement.BoneName = BoneName;

		//const bool bBaseIsSimulating = MovementBaseUtility::IsSimulatedBase(NewBaseComponent);
		//if (bBaseChanged)
		//{
		//	//MovementBaseUtility::RemoveTickDependency(CharacterMovement->PrimaryComponentTick, OldBase);
		//	//// We use a special post physics function if simulating, otherwise add normal tick prereqs.
		//	//if (!bBaseIsSimulating)
		//	//{
		//	//	MovementBaseUtility::AddTickDependency(CharacterMovement->PrimaryComponentTick, NewBaseComponent);
		//	//}
		//}

		if (NewBaseComponent)
		{
			// Update OldBaseLocation/Rotation as those were referring to a different base
			// ... but not when handling replication for proxies (since they are going to copy this data from the replicated values anyway)
			//if (!bInBaseReplication)
			//{
			//	// Force base location and relative position to be computed since we have a new base or bone so the old relative offset is meaningless.
			//	CharacterMovement->SaveBaseLocation();
			//}
			//CharacterMovement->SaveBaseLocation();
			SaveBaseLocation(CapsuleFrag, MoveFrag, MoveParams);
			
			// Enable PostPhysics tick if we are standing on a physics object, as we need to to use post-physics transforms
			//CharacterMovement->PostPhysicsTickFunction.SetTickFunctionEnable(bBaseIsSimulating);
		}
		else
		{
			BasedMovement.BoneName = NAME_None; // None, regardless of whether user tried to set a bone name, since we have no base component.
			BasedMovement.bRelativeRotation = false;
			//CharacterMovement->CurrentFloor.Clear();
			CurrentFloor.Clear();
			//CharacterMovement->PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}

		//const ENetRole LocalRole = GetLocalRole();
		//if (LocalRole == ROLE_Authority || LocalRole == ROLE_AutonomousProxy)
		//{
		//	BasedMovement.bServerHasBaseComponent = (BasedMovement.MovementBase != nullptr); // Also set on proxies for nicer debugging.
		//	//UE_LOG(LogCharacter, Verbose, TEXT("Setting base on %s for '%s' to '%s'"), LocalRole == ROLE_Authority ? TEXT("Server") : TEXT("AutoProxy"), *GetName(), *GetFullNameSafe(NewBaseComponent));
		//}
		//else
		//{
		//	//UE_LOG(LogCharacter, Verbose, TEXT("Setting base on Client for '%s' to '%s'"), *GetName(), *GetFullNameSafe(NewBaseComponent));
		//}
		BasedMovement.bServerHasBaseComponent = (BasedMovement.MovementBase != nullptr);
	}
}

bool UMassApplySurfaceMovementProcessor::IsValidLandingSpot(FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	// Skip some checks if penetrating. Penetration will be handled by the FindFloor call (using a smaller capsule)
	if (!Hit.bStartPenetrating)
	{
		// Reject unwalkable floor normals.
		if (!IsWalkable(MoveParams, Hit))
		{
			return false;
		}

		float PawnRadius, PawnHalfHeight;
		CapsuleFrag.GetMutableCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

		// Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface).
		const float LowerHemisphereZ = Hit.Location.Z - PawnHalfHeight + PawnRadius;
		if (Hit.ImpactPoint.Z >= LowerHemisphereZ)
		{
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
	}
	else
	{
		// Penetrating
		if (Hit.Normal.Z < UE_KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
	}

	FFindFloorResult FloorResult;
	FindFloor(CapsuleFrag, MoveFrag, MoveParams, CapsuleLocation, FloorResult, false, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

void UMassApplySurfaceMovementProcessor::ApplyImpactPhysicsForces(const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const FHitResult& Impact,
	const FVector& ImpactAcceleration, const FVector& ImpactVelocity) const
{
	 if (MoveParams.bEnablePhysicsInteraction && Impact.bBlockingHit )
	 {
	 	if (UPrimitiveComponent* ImpactComponent = Impact.GetComponent())
	 	{
	 		FVector ForcePoint = Impact.ImpactPoint;
	 		float BodyMass = 1.0f; // set to 1 as this is used as a multiplier
	
	 		bool bCanBePushed = false;
	 		FBodyInstance* BI = ImpactComponent->GetBodyInstance(Impact.BoneName);
	 		if(BI != nullptr && BI->IsInstanceSimulatingPhysics())
	 		{
	 			BodyMass = FMath::Max(BI->GetBodyMass(), 1.0f);
	
	 			if(MoveParams.bPushForceUsingZOffset)
	 			{
	 				FBox Bounds = BI->GetBodyBounds();
	
	 				FVector Center, Extents;
	 				Bounds.GetCenterAndExtents(Center, Extents);
	
	 				if (!Extents.IsNearlyZero())
	 				{
	 					ForcePoint.Z = Center.Z + Extents.Z * MoveParams.PushForcePointZOffsetFactor;
	 				}
	 			}
	
	 			bCanBePushed = true;
	 		}
	 		//else if (CharacterMovementCVars::bGeometryCollectionImpulseWorkAround)
	 		else
	 		{
	 			const FName ClassName = ImpactComponent->GetClass()->GetFName();
	 			const FName GeometryCollectionClassName("GeometryCollectionComponent");
	 			if (ClassName == GeometryCollectionClassName && ImpactComponent->BodyInstance.bSimulatePhysics)
	 			{
	 				// in some case GetBodyInstance can return null while the BodyInstance still exists ( geometry collection component for example )
	 				// but we cannot check for its component directly here because of modules cyclic dependencies
	 				// todo(chaos): change this logic to be more driven at the primitive component level to avoid the high level classes to have to be aware of the different component
	
	 				// because of the above limititation we have to ignore bPushForceUsingZOffset
	
	 				bCanBePushed = true;
	 			}
	 		}
	
	 		if (bCanBePushed)
	 		{
	 			FVector Force = Impact.ImpactNormal * -1.0f;
	
	 			float PushForceModificator = 1.0f;
	
	 			const FVector ComponentVelocity = ImpactComponent->GetPhysicsLinearVelocity();
	 			const FVector VirtualVelocity = ImpactAcceleration.IsZero() ? ImpactVelocity : ImpactAcceleration.GetSafeNormal() * GetMaxSpeed(SpeedParams);
	
	 			float Dot = 0.0f;
	
	 			if (MoveParams.bScalePushForceToVelocity && !ComponentVelocity.IsNearlyZero())
	 			{
	 				Dot = ComponentVelocity | VirtualVelocity;
	
	 				if (Dot > 0.0f && Dot < 1.0f)
	 				{
	 					PushForceModificator *= Dot;
	 				}
	 			}
	
	 			if (MoveParams.bPushForceScaledToMass)
	 			{
	 				PushForceModificator *= BodyMass;
	 			}
	
	 			Force *= PushForceModificator;
	 			const float ZeroVelocityTolerance = 1.0f;
	 			if (ComponentVelocity.IsNearlyZero(ZeroVelocityTolerance))
	 			{
	 				Force *= MoveParams.InitialPushForceFactor;
	 				ImpactComponent->AddImpulseAtLocation(Force, ForcePoint, Impact.BoneName);
	 			}
	 			else
	 			{
	 				Force *= MoveParams.PushForceFactor;
	 				ImpactComponent->AddForceAtLocation(Force, ForcePoint, Impact.BoneName);
	 			}
	 		}
	 	}
	 }
}

void UMassApplySurfaceMovementProcessor::SimulateMovement(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime) const
{
	/*
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_SurfaceMovementSimulateMovement);

	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	
	FVector OldVelocity;
	FVector OldLocation;
	
	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, MoveParams.bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		bool bHandledNetUpdate = false;
		//if (bIsSimulatedProxy)
		{
			// Handle network changes
			//if (bNetworkUpdateReceived)
			//{
			//	bNetworkUpdateReceived = false;
			//	bHandledNetUpdate = true;
			//	UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s received net update"), *CharacterOwner->GetName());
			//	if (bNetworkMovementModeChanged)
			//	{
			//		ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
			//		bNetworkMovementModeChanged = false;
			//	}
			//	else if (bJustTeleported || bForceNextFloorCheck)
			//	{
			//		// Make sure floor is current. We will continue using the replicated base, if there was one.
			//		bJustTeleported = false;
			//		UpdateFloorFromAdjustment();
			//	}
			//}
			if (MoveFrag.bForceNextFloorCheck)
			{
				UpdateFloorFromAdjustment(CapsuleFrag, MoveFrag, MoveParams);
			}
		}

		//UpdateCharacterStateBeforeMovement(DeltaSeconds);

		if (MoveFrag.MovementMode != EMassSurfaceMovementMode::None)
		{
			//TODO: Also ApplyAccumulatedForces()?
			//HandlePendingLaunch();
		}
		//ClearAccumulatedForces();

		if (MoveFrag.MovementMode == EMassSurfaceMovementMode::None)
		{
			return;
		}

		//const bool bSimGravityDisabled = (bIsSimulatedProxy && CharacterOwner->bSimGravityDisabled);
		//const bool bZeroReplicatedGroundVelocity = (bIsSimulatedProxy && IsMovingOnGround() && ConstRepMovement.LinearVelocity.IsZero());
		//
		//// bSimGravityDisabled means velocity was zero when replicated and we were stuck in something. Avoid external changes in velocity as well.
		//// Being in ground movement with zero velocity, we cannot simulate proxy velocities safely because we might not get any further updates from the server.
		//if (bSimGravityDisabled || bZeroReplicatedGroundVelocity)
		//{
		//	Velocity = FVector::ZeroVector;
		//}

		MaybeUpdateBasedMovement(DeltaTime);
		
		FVector& Velocity = VelocityFrag.Value;
		
		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		UpdateProxyAcceleration();

		// May only need to simulate forward on frames where we haven't just received a new position update.
		if (!bHandledNetUpdate || !bNetworkSkipProxyPredictionOnNetUpdate || !CharacterMovementCVars::NetEnableSkipProxyPredictionOnNetUpdate)
		{
			UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
			FStepDownResult StepDownResult;
			MoveSmooth(Velocity, DeltaSeconds, &StepDownResult);

			// find floor and check if falling
			if (IsMovingOnGround() || MovementMode == MOVE_Falling)
			{
				if (StepDownResult.bComputedFloor)
				{
					CurrentFloor = StepDownResult.FloorResult;
				}
				else if (Velocity.Z <= 0.f)
				{
					FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
				}
				else
				{
					CurrentFloor.Clear();
				}

				if (!CurrentFloor.IsWalkableFloor())
				{
					if (!bSimGravityDisabled)
					{
						// No floor, must fall.
						if (Velocity.Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner->IsJumpProvidingForce())
						{
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
						}
					}
					SetMovementMode(MOVE_Falling);
				}
				else
				{
					// Walkable floor
					if (IsMovingOnGround())
					{
						AdjustFloorHeight();
						SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
					}
					else if (MovementMode == MOVE_Falling)
					{
						if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST || (bSimGravityDisabled && CurrentFloor.FloorDist <= MAX_FLOOR_DIST))
						{
							// Landed
							SetPostLandedPhysics(CurrentFloor.HitResult);
						}
						else
						{
							if (!bSimGravityDisabled)
							{
								// Continue falling.
								Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
							}
							CurrentFloor.Clear();
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s SKIPPING simulate movement"), *GetNameSafe(CharacterOwner));
		}

		UpdateCharacterStateAfterMovement(DeltaSeconds);

		// consume path following requested velocity
		LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	if (CharacterMovementCVars::BasedMovementMode == 0)
	{
		SaveBaseLocation(); // behaviour before implementing this fix
	}
	else
	{
		MaybeSaveBaseLocation();
	}
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
	*/
}

void UMassApplySurfaceMovementProcessor::UpdateFloorFromAdjustment(FETW_MassCopsuleFragment& CapsuleFrag,
	FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const
{
	//if (!HasValidData())
	//{
	//	return;
	//}

	// If walking, try to update the cached floor so it is current. This is necessary for UpdateBasedMovement() and MoveAlongFloor() to work properly.
	// If base is now NULL, presumably we are no longer walking. If we had a valid floor but don't find one now, we'll likely start falling.
	if (MoveFrag.BasedMovement.MovementBase)
	{
		const FVector& ComponentLocation = CapsuleFrag.GetCapsuleComponent()->GetComponentLocation();
		FindFloor(CapsuleFrag, MoveFrag, MoveParams, ComponentLocation, MoveFrag.Floor, false);
	}
	else
	{
		MoveFrag.Floor.Clear();
	}
}

void UMassApplySurfaceMovementProcessor::UpdateBasedMovement(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaSeconds) const
{
	/*
	//if (!HasValidData())
	//{
	//	return;
	//}

	const UPrimitiveComponent* MovementBase = MoveFrag.BasedMovement.MovementBase;
	if (!MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		return;
	}

	if (!IsValid(MovementBase) || !IsValid(MovementBase->GetOwner()))
	{
		SetBase(CapsuleFrag, MoveFrag, MoveParams, NULL);
		return;
	}

	// Ignore collision with bases during these movements.
	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveFrag.MoveComponentFlags, MoveFrag.MoveComponentFlags | MOVECOMP_IgnoreBases);

	FQuat DeltaQuat = FQuat::Identity;
	FVector DeltaPosition = FVector::ZeroVector;

	FQuat NewBaseQuat;
	FVector NewBaseLocation;
	if (!MovementBaseUtility::GetMovementBaseTransform(MovementBase, MoveFrag.BasedMovement.BoneName, NewBaseLocation, NewBaseQuat))
	{
		return;
	}

	FVector& OldBaseLocation = MoveFrag.OldBaseLocation;
	FQuat& OldBaseQuat = MoveFrag.OldBaseQuat;
	UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	
	// Find change in rotation
	const bool bRotationChanged = !OldBaseQuat.Equals(NewBaseQuat, 1e-8f);
	if (bRotationChanged)
	{
		DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
	}

	// only if base moved
	if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
	{
		// Calculate new transform matrix of base actor (ignoring scale).
		const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
		const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

		FQuat FinalQuat = UpdatedComponent->GetComponentQuat();
			
		if (bRotationChanged && !MoveParams.bIgnoreBaseRotation)
		{
			// Apply change in rotation and pipe through FaceRotation to maintain axis restrictions
			const FQuat PawnOldQuat = UpdatedComponent->GetComponentQuat();
			const FQuat TargetQuat = DeltaQuat * FinalQuat;
			FRotator TargetRotator(TargetQuat);
			//CharacterOwner->FaceRotation(TargetRotator, 0.f);
			FinalQuat = UpdatedComponent->GetComponentQuat();

			if (PawnOldQuat.Equals(FinalQuat, 1e-6f))
			{
				// Nothing changed. This means we probably are using another rotation mechanism (bOrientToMovement etc). We should still follow the base object.
				// @todo: This assumes only Yaw is used, currently a valid assumption. This is the only reason FaceRotation() is used above really, aside from being a virtual hook.
				if (MoveParams.bOrientRotationToMovement)
				{
					TargetRotator.Pitch = 0.f;
					TargetRotator.Roll = 0.f;
					MoveUpdatedComponent(CapsuleFrag, MoveFrag, FVector::ZeroVector, TargetRotator, false);
					FinalQuat = UpdatedComponent->GetComponentQuat();
				}
			}

			// Pipe through ControlRotation, to affect camera.
			//if (CharacterOwner->Controller) 
			//{
			//	const FQuat PawnDeltaRotation = FinalQuat * PawnOldQuat.Inverse();
			//	FRotator FinalRotation = FinalQuat.Rotator();
			//	UpdateBasedRotation(FinalRotation, PawnDeltaRotation.Rotator());
			//	FinalQuat = UpdatedComponent->GetComponentQuat();
			//}
		}

		// We need to offset the base of the character here, not its origin, so offset by half height
		float HalfHeight, Radius;
		UpdatedComponent->GetScaledCapsuleSize(Radius, HalfHeight);

		FVector const BaseOffset(0.0f, 0.0f, HalfHeight);
		FVector const LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponent->GetComponentLocation() - BaseOffset);
		FVector const NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos) + BaseOffset);
		DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponent->GetComponentLocation());

		// move attached actor
		if (bFastAttachedMove)
		{
			// we're trusting no other obstacle can prevent the move here
			UpdatedComponent->SetWorldLocationAndRotation(NewWorldPos, FinalQuat, false);
		}
		else
		{
			// hack - transforms between local and world space introducing slight error FIXMESTEVE - discuss with engine team: just skip the transforms if no rotation?
			FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;
			if (!bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f))
			{
				DeltaPosition.X = 0.f;
				DeltaPosition.Y = 0.f;
			}

			FHitResult MoveOnBaseHit(1.f);
			const FVector OldLocation = UpdatedComponent->GetComponentLocation();
			MoveUpdatedComponent(DeltaPosition, FinalQuat, true, &MoveOnBaseHit);
			if ((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero() == false)
			{
				OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
			}
		}

		if (MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh())
		{
			CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
		}
	}
	*/
}

void UMassApplySurfaceMovementProcessor::MaybeUpdateBasedMovement(const float DeltaSeconds) const
{
}

void UMassApplySurfaceMovementProcessor::PhysFalling(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime) const
{
	SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(STAT_CharPhysFalling);

	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector FallAcceleration = GetFallingLateralAcceleration(VelocityFrag, ForceFrag, SpeedParams, MoveParams, DeltaTime);
	FallAcceleration.Z = 0.f;
	const bool bHasLimitedAirControl = FallAcceleration.SizeSquared2D() > 0.f;

	FVector& Velocity = VelocityFrag.Value;
	FVector& Acceleration = ForceFrag.Value;
	UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
	float& JumpForceTimeRemaining = MoveFrag.JumpForceTimeRemaining;
	bool& bJustTeleported = MoveFrag.bJustTeleported;
	const bool& bApplyGravityWhileJumping = MoveParams.bApplyGravityWhileJumping;
	
	float RemainingTime = DeltaTime;
	//while( (RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) )
	//Iterations++;
	float TimeTick = GetSimulationTimeStep(RemainingTime);
	RemainingTime -= TimeTick;
	
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	bJustTeleported = false;

	const FVector OldVelocityWithRootMotion = Velocity;

	//RestorePreAdditiveRootMotionVelocity();

	const FVector OldVelocity = Velocity;

	// Apply input
	const float MaxDecel = GetMaxBrakingDeceleration(MoveFrag, MoveParams);
	//if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		// Compute Velocity
		{
			// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
			Velocity.Z = 0.f;
			CalcVelocity(VelocityFrag, ForceFrag, MoveFrag, SpeedParams, MoveParams, TimeTick, MoveParams.FallingLateralFriction, false, MaxDecel);
			Velocity.Z = OldVelocity.Z;
		}
	}

	// Compute current gravity
	const FVector Gravity(0.f, 0.f, MoveParams.GravityZ);
	float GravityTime = TimeTick;

	// If jump is providing force, gravity may be affected.
	bool bEndingJumpForce = false;
	if (JumpForceTimeRemaining > 0.0f)
	{
		// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
		const float JumpForceTime = FMath::Min(JumpForceTimeRemaining, TimeTick);
		GravityTime = bApplyGravityWhileJumping ? TimeTick : FMath::Max(0.0f, TimeTick - JumpForceTime);
		
		// Update Character state
		JumpForceTimeRemaining -= JumpForceTime;
		if (JumpForceTimeRemaining <= 0.0f)
		{
			//CharacterOwner->ResetJumpState();  // todo: jump
			bEndingJumpForce = true;
		}
	}

	// Apply gravity
	Velocity = NewFallVelocity(Velocity, Gravity, GravityTime);

	//UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) OldVelocityWithRootMotion=(%s) NewVelocity=(%s)"), TimeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *OldVelocityWithRootMotion.ToString(), *Velocity.ToString());
	//ApplyRootMotionToVelocity(TimeTick);
	//DecayFormerBaseVelocity(TimeTick);

	// See if we need to sub-step to exactly reach the apex. This is important for avoiding "cutting off the top" of the trajectory as framerate varies.
	//if (CharacterMovementCVars::ForceJumpPeakSubstep && OldVelocityWithRootMotion.Z > 0.f && Velocity.Z <= 0.f && NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
	if (FORCE_JUMP_PEAK_SUBSTEP && OldVelocityWithRootMotion.Z > 0.f && Velocity.Z <= 0.f)
	{
		const FVector DerivedAccel = (Velocity - OldVelocityWithRootMotion) / TimeTick;
		if (!FMath::IsNearlyZero(DerivedAccel.Z))
		{
			const float TimeToApex = -OldVelocityWithRootMotion.Z / DerivedAccel.Z;
			
			// The time-to-apex calculation should be precise, and we want to avoid adding a substep when we are basically already at the apex from the previous iteration's work.
			const float ApexTimeMinimum = 0.0001f;
			if (TimeToApex >= ApexTimeMinimum && TimeToApex < TimeTick)
			{
				const FVector ApexVelocity = OldVelocityWithRootMotion + (DerivedAccel * TimeToApex);
				Velocity = ApexVelocity;
				Velocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.
	
				// We only want to move the amount of time it takes to reach the apex, and refund the unused time for next iteration.
				const float TimeToRefund = (TimeTick - TimeToApex);
	
				RemainingTime += TimeToRefund;
				TimeTick = TimeToApex;
				//NumJumpApexAttempts++;
	
				// Refund time to any active Root Motion Sources as well
				//for (TSharedPtr<FRootMotionSource> RootMotionSource : CurrentRootMotion.RootMotionSources)
				//{
				//	const float RewoundRMSTime = FMath::Max(0.0f, RootMotionSource->GetTime() - TimeToRefund);
				//	RootMotionSource->SetTime(RewoundRMSTime);
				//}
			}
		}
	}

	//if (bNotifyApex && (Velocity.Z < 0.f))
	//{
	//	// Just passed jump apex since now going down
	//	bNotifyApex = false;
	//	NotifyJumpApex();
	//}

	// Compute change in position (using midpoint integration method).
	FVector Adjusted = 0.5f * (OldVelocityWithRootMotion + Velocity) * TimeTick;
	
	// Special handling if ending the jump force where we didn't apply gravity during the jump.
	if (bEndingJumpForce && !bApplyGravityWhileJumping)
	{
		// We had a portion of the time at constant speed then a portion with acceleration due to gravity.
		// Account for that here with a more correct change in position.
		const float NonGravityTime = FMath::Max(0.f, TimeTick - GravityTime);
		Adjusted = (OldVelocityWithRootMotion * NonGravityTime) + (0.5f*(OldVelocityWithRootMotion + Velocity) * GravityTime);
	}

	// Move
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, Adjusted, PawnRotation, true, Hit);
	
	//if (!HasValidData())
	//{
	//	return;
	//}
	
	float LastMoveTimeSlice = TimeTick;
	float SubTimeTickRemaining = TimeTick * (1.f - Hit.Time);
	
	//if ( IsSwimming() ) //just entered water
	//{
	//	RemainingTime += subTimeTickRemaining;
	//	StartSwimming(OldLocation, OldVelocity, TimeTick, RemainingTime, Iterations);
	//	return;
	//}
	//else if ( Hit.bBlockingHit )
	if ( Hit.bBlockingHit )
	{
		if (IsValidLandingSpot(CapsuleFrag, MoveFrag, MoveParams, UpdatedComponent->GetComponentLocation(), Hit))
		{
			RemainingTime += SubTimeTickRemaining;
			ProcessLanded(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, Hit, RemainingTime);
			return;
		}
		else
		{
			// Compute impact deflection based on final velocity, not integration step.
			// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
			Adjusted = Velocity * TimeTick;

			// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
			if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(CapsuleFrag, TimeTick, Adjusted, Hit))
			{
				const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
				FFindFloorResult FloorResult;
				FindFloor(CapsuleFrag, MoveFrag, MoveParams, PawnLocation, FloorResult, false);
				if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(CapsuleFrag, MoveFrag, MoveParams, PawnLocation, FloorResult.HitResult))
				{
					RemainingTime += SubTimeTickRemaining;
					ProcessLanded(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, FloorResult.HitResult, RemainingTime);
					return;
				}
			}

			HandleImpact(CapsuleFrag, MoveFrag, Hit, LastMoveTimeSlice, Adjusted);
			
			// If we've changed physics mode, abort.
			//if (!HasValidData() || !IsFalling(MoveFrag))
			if (!IsFalling(MoveFrag))
			{
				return;
			}

			// Limit air control based on what we hit.
			// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
			FVector VelocityNoAirControl = OldVelocity;
			FVector AirControlAccel = Acceleration;
			if (bHasLimitedAirControl)
			{
				// Compute VelocityNoAirControl
				{
					// Find velocity *without* acceleration.
					TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
					TGuardValue<FVector> RestoreVelocity(Velocity, OldVelocity);
					Velocity.Z = 0.f;
					CalcVelocity(VelocityFrag, ForceFrag, MoveFrag, SpeedParams, MoveParams, TimeTick, MoveParams.FallingLateralFriction, false, MaxDecel);
					VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
					VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, GravityTime);
				}

				const bool bCheckLandingSpot = false; // we already checked above.
				AirControlAccel = (Velocity - VelocityNoAirControl) / TimeTick;
				const FVector AirControlDeltaV = LimitAirControl(CapsuleFrag, MoveFrag, MoveParams, LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
				Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
			}

			const FVector OldHitNormal = Hit.Normal;
			const FVector OldHitImpactNormal = Hit.ImpactNormal;				
			FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

			// Compute velocity after deflection (only gravity component for RootMotion)
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();
			//if (CharacterMovementCVars::UseTargetVelocityOnImpact && !Velocity.IsNearlyZero() && MovementBaseUtility::IsSimulatedBase(HitComponent))
			if (!Velocity.IsNearlyZero() && MovementBaseUtility::IsSimulatedBase(HitComponent))
			{
				const FVector ContactVelocity = MovementBaseUtility::GetMovementBaseVelocity(HitComponent, NAME_None) + MovementBaseUtility::GetMovementBaseTangentialVelocity(HitComponent, NAME_None, Hit.ImpactPoint);
				const FVector NewVelocity = Velocity - Hit.ImpactNormal * FVector::DotProduct(Velocity - ContactVelocity, Hit.ImpactNormal);
				//Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
				Velocity = NewVelocity;
			}
			else if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
			{
				const FVector NewVelocity = (Delta / SubTimeTickRemaining);
				//Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
				Velocity = NewVelocity;
			}

			if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
			{
				// Move in deflected direction.
				SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, PawnRotation, true, Hit);
				
				if (Hit.bBlockingHit)
				{
					// hit second wall
					LastMoveTimeSlice = SubTimeTickRemaining;
					SubTimeTickRemaining = SubTimeTickRemaining * (1.f - Hit.Time);

					if (IsValidLandingSpot(CapsuleFrag, MoveFrag, MoveParams, UpdatedComponent->GetComponentLocation(), Hit))
					{
						RemainingTime += SubTimeTickRemaining;
						ProcessLanded(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, Hit, RemainingTime);
						return;
					}

					HandleImpact(CapsuleFrag, MoveFrag, Hit, LastMoveTimeSlice, Delta);

					// If we've changed physics mode, abort.
					//if (!HasValidData() || !IsFalling())
					if (!IsFalling(MoveFrag))
					{
						return;
					}

					// Act as if there was no air control on the last move when computing new deflection.
					if (bHasLimitedAirControl && Hit.Normal.Z > VERTICAL_SLOPE_NORMAL_Z)
					{
						const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
						Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
					}

					FVector PreTwoWallDelta = Delta;
					TwoWallAdjust(MoveFrag, MoveParams,Delta, Hit, OldHitNormal);

					// Limit air control, but allow a slide along the second wall.
					if (bHasLimitedAirControl)
					{
						const bool bCheckLandingSpot = false; // we already checked above.
						const FVector AirControlDeltaV = LimitAirControl(CapsuleFrag, MoveFrag, MoveParams, SubTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot) * SubTimeTickRemaining;

						// Only allow if not back in to first wall
						if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
						{
							Delta += (AirControlDeltaV * SubTimeTickRemaining);
						}
					}

					// Compute velocity after deflection (only gravity component for RootMotion)
					if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
					{
						const FVector NewVelocity = (Delta / SubTimeTickRemaining);
						//Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
						Velocity = NewVelocity;
					}

					// bDitch=true means that pawn is straddling two slopes, neither of which it can stand on
					bool bDitch = ( (OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= UE_KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f) );
					SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, PawnRotation, true, Hit);
					if ( Hit.Time == 0.f )
					{
						// if we are stuck then try to side step
						FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
						if ( SideDelta.IsNearlyZero() )
						{
							SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
						}
						SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, SideDelta, PawnRotation, true, Hit);
					}
						
					if ( bDitch || IsValidLandingSpot(CapsuleFrag, MoveFrag, MoveParams, UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f  )
					{
						RemainingTime = 0.f;
						ProcessLanded(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, Hit, RemainingTime);
						return;
					}
					else if (GetPerchRadiusThreshold(MoveParams) > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= MoveParams.WalkableFloorZ)
					{
						// We might be in a virtual 'ditch' within our perch radius. This is rare.
						const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
						const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
						const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
						if (ZMovedDist <= 0.2f * TimeTick && MovedDist2DSq <= 4.f * TimeTick)
						{
							float MaxSpeed = GetMaxSpeed(SpeedParams);
							
							Velocity.X += 0.25f * MaxSpeed * (MoveFrag.RandomStream.FRand() - 0.5f);
							Velocity.Y += 0.25f * MaxSpeed * (MoveFrag.RandomStream.FRand() - 0.5f);
							Velocity.Z = FMath::Max<float>(MoveParams.JumpZVelocity * 0.25f, 1.f);
							Delta = Velocity * TimeTick;
							SafeMoveUpdatedComponent(CapsuleFrag, MoveFrag, Delta, PawnRotation, true, Hit);
						}
					}
				}
			}
		}

		if (Velocity.SizeSquared2D() <= UE_KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
	}
}
