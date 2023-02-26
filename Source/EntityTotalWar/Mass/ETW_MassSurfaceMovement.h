// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CharacterMovementComponentAsync.h"
#include "MassCommonTypes.h"
#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassMovementFragments.h"
#include "Collision/ETW_MassCollisionTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/MovementComponent.h"

#include "ETW_MassSurfaceMovement.generated.h"

#define ENABLE_STATS_MASS_SURFACE_MOVEMENT 0

#if ENABLE_STATS_MASS_SURFACE_MOVEMENT
	#define SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(Stat) \
		FScopeCycleCounter CycleCount_##Stat(GET_STATID(Stat), GET_STATFLAGS(Stat));
#else
	#define SCOPE_CYCLE_COUNTER_MASS_SURFACE_MOVEMENT(Stat) 
#endif

DECLARE_STATS_GROUP(TEXT("ETW Mass"), STATGROUP_ETWMass, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement"), STAT_SurfaceMovement, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Resolve Penetration"), STAT_SurfaceMovementResolvePenetration, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Step Up"), STAT_SurfaceMovementStepUp, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Find Floor"), STAT_SurfaceMovementFindFloor, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Walking"), STAT_SurfaceMovementWalking, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Adjust Floor"), STAT_SurfaceMovementCharAdjustFloorHeight, STATGROUP_ETWMass);
DECLARE_CYCLE_STAT(TEXT("ETW Mass Surface Movement Process Landed"), STAT_SurfaceMovementCharProcessLanded, STATGROUP_ETWMass);
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

enum class EMassSurfaceMovementMode : uint8
{
	None,
	Walking,
	Falling
};

USTRUCT()
struct FMassSurfaceMovementFloor
{
	GENERATED_BODY()
	
};

USTRUCT()
struct FMassSurfaceMovementFragment : public FMassFragment
{
	GENERATED_BODY()
	
	//FMassSurfaceMovementFloor Floor;
	FFindFloorResult Floor;
	FBasedMovementInfo BasedMovement;
	
	/** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
	FQuat OldBaseQuat;
	/** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
	FVector OldBaseLocation;

	FRandomStream RandomStream;

	float JumpForceTimeRemaining = 0.f;
	
	EMassSurfaceMovementMode MovementMode;
	EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;
	bool bJustTeleported = false;
	bool bForceNextFloorCheck = false;
};

USTRUCT()
struct ENTITYTOTALWAR_API FMassSurfaceMovementParams : public FMassSharedFragment
{
	GENERATED_BODY()
	
	/** Maximum height character can step up */
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0", ForceUnits=cm))
	float MaxStepHeight = 45.f;

	/** MaxFloorTraceDist  should be greater then Step Height!!!! cm */
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0", ForceUnits=cm))
	float MaxFloorTraceDist = 200.f;

	/**
	 * Minimum Z value for floor normal. If less, not a walkable surface. cos(WalkableFloorAngle) cost(45 deg) = 0.71f
	 */
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0", ForceUnits="cosine"))
	float WalkableFloorZ = 0.71f;
	
	/** Gravity */
	UPROPERTY(Category = "Movement: Falling", EditAnywhere, AdvancedDisplay, meta = (ForceUnits="cm / s^2"))
	float GravityZ = -980.f;

	/**
	* When perching on a ledge, add this additional distance to MaxStepHeight when determining how high above a walkable floor we can perch.
	* Note that we still enforce MaxStepHeight to start the step up; this just allows the character to hang off the edge or step slightly higher off the floor.
	* (@see PerchRadiusThreshold)
	*/
	UPROPERTY(Category="Movement: Collision", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float PerchAdditionalHeight = 40.f;

	/**
	 * Don't allow the character to perch on the edge of a surface if the contact is this close to the edge of the capsule.
	 * Note that characters will not fall off if they are within MaxStepHeight of a walkable surface below.
	 */
	UPROPERTY(Category="Movement: Collision", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float PerchRadiusThreshold = 15.f;

	/**
	 * Factor used to multiply actual value of friction used when braking.
	 * @note This is 2 by default for historical reasons, a value of 1 gives the true drag equation.
	 * @see GroundFriction
	 */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float BrakingFrictionFactor = 2.f;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * 
	 */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationWalking = 250.f;

	/**
	 * Lateral deceleration when falling and not applying acceleration.
	 * 
	 */
	UPROPERTY(Category="Movement: Falling", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationFalling = 0.f;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	* When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	* This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	*/
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float GroundFriction = 8.f;

	/**
	 * Time substepping when applying braking friction. Smaller time steps increase accuracy at the slight cost of performance, especially if there are large frame times.
	 */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0.0166", ClampMax="0.05", UIMin="0.0166", UIMax="0.05"))
	float BrakingSubStepTime = 1/33.f;
	
	/** Used in determining if pawn is going off ledge.  If the ledge is "shorter" than this value then the pawn will be able to walk off it. **/
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay, meta=(ForceUnits=cm))
	float LedgeCheckThreshold = 4.f;

	/**
	 * When falling, amount of lateral movement control available to the character.
	 * 0 = no control, 1 = full control at max speed of MaxWalkSpeed.
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float AirControl = 0.35;

	/**
	 * Friction to apply to lateral air movement when falling.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero).
	 * @see BrakingFriction, bUseSeparateBrakingFriction
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float FallingLateralFriction = 0.f;
	
	/**
	 * When falling, multiplier applied to AirControl when lateral velocity is less than AirControlBoostVelocityThreshold.
	 * Setting this to zero will disable air control boosting. Final result is clamped at 1.
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float AirControlBoostMultiplier = 2.f;

	/**
	 * When falling, if lateral velocity magnitude is less than this value, AirControl is multiplied by AirControlBoostMultiplier.
	 * Setting this to zero will disable air control boosting.
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float AirControlBoostVelocityThreshold = 25.f;

	/** Initial velocity (instantaneous vertical acceleration) when jumping. */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay, meta=(DisplayName="Jump Z Velocity", ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float JumpZVelocity = 700.f;

	/** Initial impulse force to apply when the player bounces into a blocking physics object. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(editcondition = "bEnablePhysicsInteraction"))
	float InitialPushForceFactor = 500.f;;

	/** Force to apply when the player collides with a blocking physics object. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(editcondition = "bEnablePhysicsInteraction"))
	float PushForceFactor = 750000.0f;

	/** Z-Offset for the position the force is applied to. 0.0f is the center of the physics object, 1.0f is the top and -1.0f is the bottom of the object. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(UIMin = "-1.0", UIMax = "1.0"), meta=(editcondition = "bEnablePhysicsInteraction"))
	float PushForcePointZOffsetFactor = -0.75f;
	
	/** If enabled, the PushForceFactor is applied per kg mass of the affected object. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(editcondition = "bEnablePhysicsInteraction"))
	uint8 bPushForceScaledToMass:1 = false;
	
	/** If enabled, the player will interact with physics objects when walking into them. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay)
	uint8 bEnablePhysicsInteraction:1 = true;

	/** If enabled, the PushForce location is moved using PushForcePointZOffsetFactor. Otherwise simply use the impact point. */
	UPROPERTY(Category = "Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta = (editcondition = "bEnablePhysicsInteraction"))
	uint8 bPushForceUsingZOffset:1 = false;

	/** If enabled, the applied push force will try to get the physics object to the same velocity than the player, not faster. This will only
	scale the force down, it will never apply more force than defined by PushForceFactor. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(editcondition = "bEnablePhysicsInteraction"))
	uint8 bScalePushForceToVelocity:1 = true;
	
	/**
	 *	Apply gravity while the character is actively jumping (e.g. holding the jump key).
	 *	Helps remove frame-rate dependent jump height, but may alter base jump height.
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	uint8 bApplyGravityWhileJumping:1 = true;
	
	/**
	 * Whether the character ignores changes in rotation of the base it is standing on.
	 * If true, the character maintains current world rotation.
	 * If false, the character rotates with the moving base.
	 */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay)
	uint8 bIgnoreBaseRotation:1 = false;
	
	/**
	* Performs floor checks as if the character is using a shape with a flat base.
	 * This avoids the situation where characters slowly lower off the side of a ledge (as their capsule 'balances' on the edge).
	*/
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay)
	uint8 bUseFlatBaseForFloorChecks:1 = false;

	/**
	 * If true, impart the base component's tangential components of angular velocity when jumping or falling off it.
	 * Only those components of the velocity allowed by the separate component settings (bImpartBaseVelocityX etc) will be applied.
	 * @see bImpartBaseVelocityX, bImpartBaseVelocityY, bImpartBaseVelocityZ
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	uint8 bImpartBaseAngularVelocity:1 = true;

	/** If true, impart the base actor's X velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	uint8 bImpartBaseVelocityX:1 = true;

	/** If true, impart the base actor's Y velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	uint8 bImpartBaseVelocityY:1 = true;

	/** If true, impart the base actor's Z velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	uint8 bImpartBaseVelocityZ:1 = true;
	
	/** If true, Character can walk off a ledge. */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay)
	uint8 bCanWalkOffLedges:1 = true;
		
	/** Floor */
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay)
	bool bAlwaysCheckFloor = false;

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
	FMassMovementParameters MovementParams;
	
	UPROPERTY(EditAnywhere, Category = "Mass|Movement")
	FMassSurfaceMovementParams SurfaceMovementParams;
};


/**
 * 
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


/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UMassApplySurfaceMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

// UMassProcessor BEGIN
	
public:
	UMassApplySurfaceMovementProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;

// UMassProcessor END

// UCharacterMovement BEGIN

	inline bool MoveUpdatedComponent(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, ETeleportType Teleport = ETeleportType::None) const
	{
		return CapsuleFrag.GetMutableCapsuleComponent()->MoveComponent(Delta, NewRotation, bSweep, OutHit);
	}

	bool SafeMoveUpdatedComponent(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport = ETeleportType::None) const;

	inline FVector UMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
	{
		if (!Hit.bStartPenetrating)
		{
			return FVector::ZeroVector;
		}

		FVector Result;
		const float PullBackDistance = FMath::Abs(MovementComponentCVars::PenetrationPullbackDistance);
		const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);

		Result = Hit.Normal * (PenetrationDepth + PullBackDistance);

		return Result;
	}

	bool ResolvePenetration(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat) const;

	inline bool OverlapTest(const FETW_MassCopsuleFragment& CapsuleFrag, const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape) const
	{
		UPrimitiveComponent* PrimitiveComponent = CapsuleFrag.GetCapsuleComponent();
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(STAT_SurfaceMovementOverlapTest), false);
		FCollisionResponseParams ResponseParam;
		PrimitiveComponent->InitSweepCollisionParams(QueryParams, ResponseParam);
		return PrimitiveComponent->GetWorld()->OverlapBlockingTestByChannel(Location, RotationQuat, CollisionChannel, CollisionShape, QueryParams, ResponseParam);
	}

	inline FVector ComputeGroundMovementDelta(const FMassSurfaceMovementParams& MoveParams, const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const
	{
		const FVector FloorNormal = RampHit.ImpactNormal;
		const FVector ContactNormal = RampHit.Normal;

		if (FloorNormal.Z < (1.f - UE_KINDA_SMALL_NUMBER) && FloorNormal.Z > UE_KINDA_SMALL_NUMBER && ContactNormal.Z > UE_KINDA_SMALL_NUMBER && !bHitFromLineTrace && IsWalkable(MoveParams, RampHit))
		{
			// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
			const float FloorDotDelta = (FloorNormal | Delta);
			FVector RampMovement(Delta.X, Delta.Y, -FloorDotDelta / FloorNormal.Z);
		
			return RampMovement.GetSafeNormal() * Delta.Size();
		}

		return Delta;
	}

	inline bool IsWalkable(const FMassSurfaceMovementParams& MoveParams, const FHitResult& Hit) const
	{
		if (!Hit.IsValidBlockingHit())
		{
			// No hit, or starting in penetration
			return false;
		}

		// Never walk up vertical surfaces.
		if (Hit.ImpactNormal.Z < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		float TestWalkableZ = MoveParams.WalkableFloorZ;

		// See if this component overrides the walkable floor z.
		const UPrimitiveComponent* HitComponent = Hit.Component.Get();
		if (HitComponent)
		{
			const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
			TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
		}

		// Can't walk on this surface if it is too steep.
		if (Hit.ImpactNormal.Z < TestWalkableZ)
		{
			return false;
		}

		return true;
	}
	
	inline FVector ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
	{
		return FVector::VectorPlaneProject(Delta, Normal) * Time;
	}
	
	inline void HandleImpact(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FHitResult& Impact, float TimeSlice=0.f, const FVector& MoveDelta = FVector::ZeroVector) const
	{
		// @todo: notify path following;
	}

	inline void OnCharacterStuckInGeometry(FMassSurfaceMovementFragment& MoveFrag, const FHitResult* Hit) const
	{
		// Don't update velocity based on our (failed) change in position this update since we're stuck.
		MoveFrag.bJustTeleported = true;
	}

	inline bool CanStepUp(const FMassSurfaceMovementFragment& MoveFrag, const FHitResult& Hit) const
	{
		if (!Hit.IsValidBlockingHit() || MoveFrag.MovementMode == EMassSurfaceMovementMode::Falling)
		{
			return false;
		}

		// No component for "fake" hits when we are on a known good base.
		const UPrimitiveComponent* HitComponent = Hit.Component.Get();
		if (!HitComponent)
		{
			return true;
		}

		// No actor for "fake" hits when we are on a known good base.
	
		if (!Hit.HitObjectHandle.IsValid())
		{
			return true;
		}

		return true;
	}

	inline float GetPerchRadiusThreshold(const FMassSurfaceMovementParams& MoveParams) const
	{
		// Don't allow negative values.
		return FMath::Max(0.f, MoveParams.PerchRadiusThreshold);
	}

	inline float GetValidPerchRadius(const FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams) const
	{
		const float PawnRadius = CapsuleFrag.GetCapsuleComponent()->GetScaledCapsuleRadius();
		return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(MoveParams), 0.11f, PawnRadius);
	}

	inline bool ShouldComputePerchResult(const FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams, const FHitResult& InHit, bool bCheckRadius) const
	{
		if (!InHit.IsValidBlockingHit())
		{
			return false;
		}

		// Don't try to perch if the edge radius is very small.
		if (GetPerchRadiusThreshold(MoveParams) <= SWEEP_EDGE_REJECT_DISTANCE)
		{
			return false;
		}

		if (bCheckRadius)
		{
			const float DistFromCenterSq = (InHit.ImpactPoint - InHit.Location).SizeSquared2D();
			const float StandOnEdgeRadius = GetValidPerchRadius(CapsuleFrag, MoveParams);
			if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
			{
				// Already within perch radius.
				return false;
			}
		}
	
		return true;
	}

	bool ComputePerchResult(const FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const;

	bool FloorSweepTest(const FMassSurfaceMovementParams& MoveParams, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel,
	                    const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam) const;

	void ComputeFloorDist(const FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult = NULL) const;
	
	bool StepUp(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& GravDir, const FVector& Delta, const FHitResult &InHit, FStepDownResult* OutStepDownResult) const;
	
	void TwoWallAdjust(FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const;

	inline bool IsMovingOnGround(FMassSurfaceMovementFragment& MoveFrag) const
	{
		return MoveFrag.MovementMode == EMassSurfaceMovementMode::Walking;
	}

	inline bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const
	{
		const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
		const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
		return DistFromCenterSq < ReducedRadiusSq;
	}

	void FindFloor(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = nullptr) const;
	
	inline bool IsFalling(FMassSurfaceMovementFragment& MoveFrag) const
	{
		return MoveFrag.MovementMode == EMassSurfaceMovementMode::Falling;
	}
	
	float SlideAlongSurface(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact) const;

	void MoveAlongFloor(FMassVelocityFragment& VelocityFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult) const;

	void PhysWalking(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime) const;
	void PhysFalling(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime) const;

	inline void SetMovementMode(FMassVelocityFragment& VelocityFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, EMassSurfaceMovementMode NewMovementMode) const
	{
		EMassSurfaceMovementMode PrevModeMode = MoveFrag.MovementMode;
		MoveFrag.MovementMode = NewMovementMode;

		OnMovementModeChanged(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, PrevModeMode);
	}

	void OnMovementModeChanged(FMassVelocityFragment& VelocityFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, EMassSurfaceMovementMode PrevMovementMode) const;

	float GetSimulationTimeStep(float RemainingTime) const
	{
		return FMath::Max(MIN_TICK_TIME, RemainingTime);
	}

	void CalcVelocity(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag,  FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime, float Friction, const bool bFluid, float BrakingDeceleration) const;

	inline float GetMaxAcceleration(const FMassMovementParameters& SpeedParams) const
	{
		return SpeedParams.MaxAcceleration;
	}

	inline float GetMaxSpeed(const FMassMovementParameters& SpeedParams) const
	{
		return SpeedParams.MaxSpeed;
	}
	
	inline float GetMaxBrakingDeceleration(FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const
	{
		switch (MoveFrag.MovementMode)
		{
		case EMassSurfaceMovementMode::Walking:
			return MoveParams.BrakingDecelerationWalking;
		case EMassSurfaceMovementMode::Falling:
			return MoveParams.BrakingDecelerationFalling;
		default:
			return 0.f;
		}
	}
	
	inline bool IsExceedingMaxSpeed(const FVector& Velocity, float MaxSpeed) const
	{
		MaxSpeed = FMath::Max(0.f, MaxSpeed);
		const float MaxSpeedSquared = FMath::Square(MaxSpeed);
	
		// Allow 1% error tolerance, to account for numeric imprecision.
		const float OverVelocityPercent = 1.01f;
		return (Velocity.SizeSquared() > MaxSpeedSquared * OverVelocityPercent);
	}

	void ApplyVelocityBraking(FMassVelocityFragment& VelocityFrag, const FMassSurfaceMovementParams& MoveParams, float DeltaTime, float Friction, float BrakingDeceleration) const;
	
	inline bool CanWalkOffLedges(const FMassSurfaceMovementParams& MoveParams) const
	{
		return MoveParams.bCanWalkOffLedges;
	}

	FVector GetLedgeMove(FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const;

	bool CheckLedgeDirection(FETW_MassCopsuleFragment& CapsuleFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const;
	
	inline FCollisionShape GetPawnCapsuleCollisionShape(const UCapsuleComponent* Capsule, const EShrinkCapsuleExtent ShrinkMode, const float CustomShrinkAmount = 0.f) const
	{
		FVector Extent = GetPawnCapsuleExtent(Capsule, ShrinkMode, CustomShrinkAmount);
		return FCollisionShape::MakeCapsule(Extent);
	}

	inline FVector GetPawnCapsuleExtent(const UCapsuleComponent* Capsule, const EShrinkCapsuleExtent ShrinkMode, const float CustomShrinkAmount = 0.f) const
	{
		float Radius, HalfHeight;
		Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
		FVector CapsuleExtent(Radius, Radius, HalfHeight);

		float RadiusEpsilon = 0.f;
		float HeightEpsilon = 0.f;

		switch(ShrinkMode)
		{
		case SHRINK_None:
			return CapsuleExtent;

		case SHRINK_RadiusCustom:
			RadiusEpsilon = CustomShrinkAmount;
			break;

		case SHRINK_HeightCustom:
			HeightEpsilon = CustomShrinkAmount;
			break;
		
		case SHRINK_AllCustom:
			RadiusEpsilon = CustomShrinkAmount;
			HeightEpsilon = CustomShrinkAmount;
			break;

		default:
			break;
		}

		// Don't shrink to zero extent.
		const FVector::FReal MinExtent = UE_KINDA_SMALL_NUMBER * 10;
		CapsuleExtent.X = FMath::Max<FVector::FReal>(CapsuleExtent.X - RadiusEpsilon, MinExtent);
		CapsuleExtent.Y = CapsuleExtent.X;
		CapsuleExtent.Z = FMath::Max<FVector::FReal>(CapsuleExtent.Z - HeightEpsilon, MinExtent);

		return CapsuleExtent;
	}

	/** 
	*  Revert to previous position OldLocation, return to being based on OldBase.
	*  if bFailMove, stop movement and notify controller
	*/	
	void RevertMove(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& PreviousBaseLocation, const FFindFloorResult& OldFloor, bool bFailMove) const;
	/** Check if pawn is falling */
	inline bool CheckFall(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float RemainingTime, float TimeTick, bool bMustJump) const
	{
		if (bMustJump || CanWalkOffLedges(MoveParams))
		{
			//HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
			if (IsMovingOnGround(MoveFrag))
			{
				// If still walking, then fall. If not, assume the user set a different mode they want to keep.
				StartFalling(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, RemainingTime, TimeTick, Delta, OldLocation);
			}
			return true;
		}
		return false;
	}

	inline void StartFalling(FMassVelocityFragment& VelocityFrag, FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, float RemainingTime, float TimeTick, const FVector& Delta, const FVector& SubLoc) const
	{
		// start falling 
		const float DesiredDist = Delta.Size();
		const float ActualDist = (CapsuleFrag.GetMutableCapsuleComponent()->GetComponentLocation() - SubLoc).Size2D();
		RemainingTime = (DesiredDist < UE_KINDA_SMALL_NUMBER)
						? 0.f
						: RemainingTime + TimeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));

		if ( IsMovingOnGround(MoveFrag) )
		{
			// This is to catch cases where the first frame of PIE is executed, and the
			// level is not yet visible. In those cases, the player will fall out of the
			// world... So, don't set MOVE_Falling straight away.
			if ( !GIsEditor || (GetWorld()->HasBegunPlay() && (GetWorld()->GetTimeSeconds() >= 1.f)) )
			{
				SetMovementMode(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, EMassSurfaceMovementMode::Falling); //default behavior if script didn't change physics
			}
			else
			{
				// Make sure that the floor check code continues processing during this delay.
				MoveFrag.bForceNextFloorCheck = true;
			}
		}
		StartNewPhysics(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, RemainingTime);
	}

	/** Adjust distance from floor, trying to maintain a slight offset from the floor when walking (based on CurrentFloor). */
	void AdjustFloorHeight(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const;

	void SaveBaseLocation(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const;
	void SetBase(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, UPrimitiveComponent* NewBaseComponent, const FName BoneName = NAME_None) const;
	/** Save a new relative location in BasedMovement and a new rotation with is either relative or absolute. */
	inline void SaveRelativeBasedMovement(FMassSurfaceMovementFragment& MoveFrag, const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation) const
	{
		FBasedMovementInfo& BasedMovement = MoveFrag.BasedMovement;
		
		//checkSlow(BasedMovement.HasRelativeLocation());
		BasedMovement.Location = NewRelativeLocation;
		BasedMovement.Rotation = NewRotation;
		BasedMovement.bRelativeRotation = bRelativeRotation;
	}

	/**
	 * Update the base of the character, using the given floor result if it is walkable, or null if not. Calls SetBase().
	 */
	inline void SetBaseFromFloor(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FFindFloorResult& FloorResult) const
	{
		if (FloorResult.IsWalkableFloor())
		{
			SetBase(CapsuleFrag, MoveFrag, MoveParams, FloorResult.HitResult.GetComponent(), FloorResult.HitResult.BoneName);
		}
		else
		{
			SetBase(CapsuleFrag, MoveFrag, MoveParams, nullptr);
		}
	}

	inline FVector GetImpartedMovementBaseVelocity(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams) const
	{
		FBasedMovementInfo& BasedMovement = MoveFrag.BasedMovement;
		UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
		FVector Result = FVector::ZeroVector;
		UPrimitiveComponent* MovementBase = BasedMovement.MovementBase;

		if (MovementBaseUtility::IsDynamicBase(MovementBase))
		{
			FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(MovementBase, BasedMovement.BoneName);
		
			if (MoveParams.bImpartBaseAngularVelocity)
			{
				const FVector CharacterBasePosition = (UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, UpdatedComponent->GetScaledCapsuleHalfHeight()));
				const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, BasedMovement.BoneName, CharacterBasePosition);
				BaseVelocity += BaseTangentialVel;
			}

			if (MoveParams.bImpartBaseVelocityX)
			{
				Result.X = BaseVelocity.X;
			}
			if (MoveParams.bImpartBaseVelocityY)
			{
				Result.Y = BaseVelocity.Y;
			}
			if (MoveParams.bImpartBaseVelocityZ)
			{
				Result.Z = BaseVelocity.Z;
			}
		}
	
		return Result;
	}


	inline void UMassApplySurfaceMovementProcessor::StartNewPhysics(FMassVelocityFragment& VelocityFrag,
		FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag,
		const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams,
		const float DeltaTime) const
	{
		//if ((deltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations) || !HasValidData())
		if ((DeltaTime < MIN_TICK_TIME))
		{
			return;
		}

		//UPrimitiveComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
		
		//if (UpdatedComponent->IsSimulatingPhysics())
		//{
		//	UE_LOG(LogCharacterMovement, Log, TEXT("UCharacterMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."), *UpdatedComponent->GetPathName());
		//	return;
		//}

		//const bool bSavedMovementInProgress = bMovementInProgress;
		//bMovementInProgress = true;

		switch ( MoveFrag.MovementMode )
		{
		case EMassSurfaceMovementMode::None:
			break;
		case EMassSurfaceMovementMode::Walking:
			PhysWalking(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, DeltaTime);
			break;
		//case MOVE_NavWalking:
		//	PhysNavWalking(deltaTime, Iterations);
		//	break;
		case EMassSurfaceMovementMode::Falling:
			PhysFalling(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, DeltaTime);
			break;
		//case MOVE_Flying:
		//	PhysFlying(deltaTime, Iterations);
		//	break;
		//case MOVE_Swimming:
		//	PhysSwimming(deltaTime, Iterations);
		//	break;
		//case MOVE_Custom:
		//	PhysCustom(deltaTime, Iterations);
		//	break;
		default:
			//UE_LOG(LogCharacterMovement, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
			SetMovementMode(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, EMassSurfaceMovementMode::None);
			break;
		}

		//bMovementInProgress = bSavedMovementInProgress;
		//if ( bDeferUpdateMoveComponent )
		//{
		//	SetUpdatedComponent(DeferredUpdatedMoveComponent);
		//}
	}

	inline FVector GetFallingLateralAcceleration(const FMassVelocityFragment& VelocityFrag, const FMassForceFragment& ForceFrag, const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, float DeltaTime) const
	{
		const FVector& Acceleration = ForceFrag.Value;
		
		// No acceleration in Z
		FVector FallAcceleration = FVector(Acceleration.X, Acceleration.Y, 0.f);

		// bound acceleration, falling object has minimal ability to impact acceleration
		//if (!HasAnimRootMotion() && FallAcceleration.SizeSquared2D() > 0.f)
		if (FallAcceleration.SizeSquared2D() > 0.f)
		{
			FallAcceleration = GetAirControl(VelocityFrag, MoveParams, DeltaTime, MoveParams.AirControl, FallAcceleration);
			FallAcceleration = FallAcceleration.GetClampedToMaxSize(GetMaxAcceleration(SpeedParams));
		}

		return FallAcceleration;
	}

	inline FVector GetAirControl(const FMassVelocityFragment& VelocityFrag, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime, float TickAirControl, const FVector& FallAcceleration) const
	{
		// Boost
		if (TickAirControl != 0.f)
		{
			TickAirControl = BoostAirControl(VelocityFrag, MoveParams, DeltaTime, TickAirControl, FallAcceleration);
		}

		return TickAirControl * FallAcceleration;
	}

	inline float BoostAirControl(const FMassVelocityFragment& VelocityFrag, const FMassSurfaceMovementParams& MoveParams, const float DeltaTime, float TickAirControl, const FVector& FallAcceleration) const
	{
		// Allow a burst of initial acceleration
		if (MoveParams.AirControlBoostMultiplier > 0.f && VelocityFrag.Value.SizeSquared2D() < FMath::Square(MoveParams.AirControlBoostVelocityThreshold))
		{
			TickAirControl = FMath::Min(1.f, MoveParams.AirControlBoostMultiplier * TickAirControl);
		}

		return TickAirControl;
	}

	inline FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
	{
		//FVector Result = InitialVelocity;
		//
		//if (DeltaTime > 0.f)
		//{
		//	// Apply gravity.
		//	Result += Gravity * DeltaTime;
		//
		//	// Don't exceed terminal velocity.
		//	const float TerminalLimit = FMath::Abs(GetPhysicsVolume()->TerminalVelocity);
		//	if (Result.SizeSquared() > FMath::Square(TerminalLimit))
		//	{
		//		const FVector GravityDir = Gravity.GetSafeNormal();
		//		if ((Result | GravityDir) > TerminalLimit)
		//		{
		//			Result = FVector::PointPlaneProject(Result, FVector::ZeroVector, GravityDir) + GravityDir * TerminalLimit;
		//		}
		//	}
		//}
		//
		//return Result;

		return InitialVelocity + Gravity * DeltaTime;
	}

	//inline void DecayFormerBaseVelocity(float DeltaTime, const FMassSurfaceMovementParams& MoveParams)
	//{
	//	if (!CharacterMovementCVars::bAddFormerBaseVelocityToRootMotionOverrideWhenFalling || FormerBaseVelocityDecayHalfLife == 0.f)
	//	{
	//		DecayingFormerBaseVelocity = FVector::ZeroVector;
	//	}
	//	else if (FormerBaseVelocityDecayHalfLife > 0.f)
	//	{
	//		DecayingFormerBaseVelocity *= FMath::Exp2(-DeltaTime * 1.f / FormerBaseVelocityDecayHalfLife);
	//	}
	//}

	/** Verify that the supplied hit result is a valid landing spot when falling. */
	bool IsValidLandingSpot(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, const FVector& CapsuleLocation, const FHitResult& Hit) const;

	inline void ProcessLanded(FMassVelocityFragment& VelocityFrag,
		FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag,
		const FMassMovementParameters& SpeedParams,const FMassSurfaceMovementParams& MoveParams, const FHitResult& Hit, float RemainingTime) const
	{
		SCOPE_CYCLE_COUNTER(STAT_SurfaceMovementCharProcessLanded);

		//if( CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit) )
		//{
		//	CharacterOwner->Landed(Hit);
		//}
		if( IsFalling(MoveFrag) )
		{
			//if (GroundMovementMode == MOVE_NavWalking)
			//{
			//	// verify navmesh projection and current floor
			//	// otherwise movement will be stuck in infinite loop:
			//	// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....
			//
			//	const FVector TestLocation = GetActorFeetLocation();
			//	FNavLocation NavLocation;
			//
			//	const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			//	if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			//	{
			//		GroundMovementMode = MOVE_Walking;
			//		UE_LOG(LogNavMeshMovement, Verbose, TEXT("ProcessLanded(): %s tried to go to NavWalking but couldn't find NavMesh! Using Walking instead."), *GetNameSafe(CharacterOwner));
			//	}
			//}

			SetPostLandedPhysics(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, Hit);
		}
	
		//IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
		//if (PFAgent)
		//{
		//	PFAgent->OnLanded();
		//}

		StartNewPhysics(VelocityFrag, ForceFrag, CapsuleFrag, MoveFrag, SpeedParams, MoveParams, RemainingTime);
	}

	inline void SetPostLandedPhysics(FMassVelocityFragment& VelocityFrag,
		FMassForceFragment& ForceFrag, FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag,
		const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const FHitResult& Hit) const
	{
		//if (CanEverSwim() && IsInWater())
		//{
		//	SetMovementMode(MOVE_Swimming);
		//}
		//else
		FVector& Acceleration = ForceFrag.Value;
		FVector& Velocity = VelocityFrag.Value;
		
		{
			const FVector PreImpactAccel = Acceleration + (IsFalling(MoveFrag) ? FVector(0.f, 0.f, MoveParams.GravityZ) : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			//if (DefaultLandMovementMode == MOVE_Walking ||
			//	DefaultLandMovementMode == MOVE_NavWalking ||
			//	DefaultLandMovementMode == MOVE_Falling)
			//{
			//	SetMovementMode(GroundMovementMode);
			//}
			//else
			//{
			//	SetDefaultMovementMode();
			//}
			SetMovementMode(VelocityFrag, CapsuleFrag, MoveFrag, MoveParams, EMassSurfaceMovementMode::Walking);
			
			ApplyImpactPhysicsForces(SpeedParams, MoveParams, Hit, PreImpactAccel, PreImpactVelocity);
		}
	}

	inline FVector LimitAirControl(FETW_MassCopsuleFragment& CapsuleFrag, FMassSurfaceMovementFragment& MoveFrag, const FMassSurfaceMovementParams& MoveParams, float DeltaTime, const FVector& FallAcceleration, const FHitResult& HitResult, bool bCheckForValidLandingSpot) const
	{
		FVector Result(FallAcceleration);

		if (HitResult.IsValidBlockingHit() && HitResult.Normal.Z > MassSurfaceMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
		{
			if (!bCheckForValidLandingSpot || !IsValidLandingSpot(CapsuleFrag, MoveFrag, MoveParams, HitResult.Location, HitResult))
			{
				// If acceleration is into the wall, limit contribution.
				if (FVector::DotProduct(FallAcceleration, HitResult.Normal) < 0.f)
				{
					// Allow movement parallel to the wall, but not into it because that may push us up.
					const FVector Normal2D = HitResult.Normal.GetSafeNormal2D();
					Result = FVector::VectorPlaneProject(FallAcceleration, Normal2D);
				}
			}
		}
		else if (HitResult.bStartPenetrating)
		{
			// Allow movement out of penetration.
			return (FVector::DotProduct(Result, HitResult.Normal) > 0.f ? Result : FVector::ZeroVector);
		}

		return Result;
	}

	void ApplyImpactPhysicsForces(const FMassMovementParameters& SpeedParams, const FMassSurfaceMovementParams& MoveParams, const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity) const;

	inline bool ShouldCheckForValidLandingSpot(FETW_MassCopsuleFragment& CapsuleFrag, float DeltaTime, const FVector& Delta, const FHitResult& Hit) const
	{
		const UCapsuleComponent* UpdatedComponent = CapsuleFrag.GetMutableCapsuleComponent();
		
		// See if we hit an edge of a surface on the lower portion of the capsule.
		// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
		if (Hit.Normal.Z > UE_KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
		{
			const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
			if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, UpdatedComponent->GetScaledCapsuleRadius()))
			{						
				return true;
			}
		}

		return false;
	}

	
	
	// UCharacterMovement END
	
};
