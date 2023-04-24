// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "CharacterMovementComponentAsync.h"
#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "GameFramework/Character.h"

#include "ETW_MassSurfaceMovementTypes.generated.h"


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

	/** Flag set in pre-physics update to indicate that based movement should be updated post-physics */
	bool bDeferUpdateBasedMovement = false;
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
	bool bPushForceScaledToMass = false;
	
	/** If enabled, the player will interact with physics objects when walking into them. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay)
	bool bEnablePhysicsInteraction = true;

	/** If enabled, the PushForce location is moved using PushForcePointZOffsetFactor. Otherwise simply use the impact point. */
	UPROPERTY(Category = "Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta = (editcondition = "bEnablePhysicsInteraction"))
	bool bPushForceUsingZOffset = false;

	/** If enabled, the applied push force will try to get the physics object to the same velocity than the player, not faster. This will only
	scale the force down, it will never apply more force than defined by PushForceFactor. */
	UPROPERTY(Category="Movement: Physics Interaction", EditAnywhere, AdvancedDisplay, meta=(editcondition = "bEnablePhysicsInteraction"))
	bool bScalePushForceToVelocity = true;
	
	/**
	 *	Apply gravity while the character is actively jumping (e.g. holding the jump key).
	 *	Helps remove frame-rate dependent jump height, but may alter base jump height.
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	bool bApplyGravityWhileJumping = true;
	
	/**
	 * Whether the character ignores changes in rotation of the base it is standing on.
	 * If true, the character maintains current world rotation.
	 * If false, the character rotates with the moving base.
	 */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay)
	bool bIgnoreBaseRotation = false;
	
	/**
	* Performs floor checks as if the character is using a shape with a flat base.
	 * This avoids the situation where characters slowly lower off the side of a ledge (as their capsule 'balances' on the edge).
	*/
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay)
	bool bUseFlatBaseForFloorChecks = false;

	/**
	 * If true, impart the base component's tangential components of angular velocity when jumping or falling off it.
	 * Only those components of the velocity allowed by the separate component settings (bImpartBaseVelocityX etc) will be applied.
	 * @see bImpartBaseVelocityX, bImpartBaseVelocityY, bImpartBaseVelocityZ
	 */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	bool bImpartBaseAngularVelocity = true;

	/** If true, impart the base actor's X velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	bool bImpartBaseVelocityX = true;

	/** If true, impart the base actor's Y velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	bool bImpartBaseVelocityY = true;

	/** If true, impart the base actor's Z velocity when falling off it (which includes jumping) */
	UPROPERTY(Category="Movement: Jumping / Falling", EditAnywhere, AdvancedDisplay)
	bool bImpartBaseVelocityZ = true;
	
	/** If true, Character can walk off a ledge. */
	UPROPERTY(Category="Movement: Walking", EditAnywhere, AdvancedDisplay)
	bool bCanWalkOffLedges = true;
		
	/** Floor */
	UPROPERTY(Category="Movement: Floor", EditAnywhere, AdvancedDisplay)
	bool bAlwaysCheckFloor = false;

	/**
	 * If true, high-level movement updates will be wrapped in a movement scope that accumulates updates and defers a bulk of the work until the end.
	 * When enabled, touch and hit events will not be triggered until the end of multiple moves within an update, which can improve performance.
	 *
	 * @see FScopedMovementUpdate
	 */
	UPROPERTY(Category="Movement", EditAnywhere, AdvancedDisplay)
	bool bEnableScopedMovementUpdates = true;

	/**
	* If true, rotate the Character toward the direction of acceleration, using RotationRate as the rate of rotation change. Overrides UseControllerDesiredRotation.
	* Normally you will want to make sure that other settings are cleared, such as bUseControllerRotationYaw on the Character.
	*/
	UPROPERTY(Category="Movement", EditAnywhere, AdvancedDisplay)
	bool bOrientRotationToMovement = true;

};
