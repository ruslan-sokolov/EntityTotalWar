// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"

#include "ETW_MassCollisionTypes.generated.h"


inline FCollisionProfileName MassCapsuleCollisionDefaultProfileName = FCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

class UCapsuleComponent;

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassCapsuleCollisionParams final : public FMassSharedFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Collision", meta = (ClampMin = "1", ForceUnits="cm/s"))
	float CapsuleHalfHeight = 96.0f;

	UPROPERTY(EditAnywhere, Category = "Collision", meta = (ClampMin = "1", ForceUnits="cm/s"))
	float CapsuleRadius = 42.0f;

	UPROPERTY(EditAnywhere, Category = "Collision")
	FCollisionProfileName CollisionProfleName = MassCapsuleCollisionDefaultProfileName;
};

USTRUCT()
struct ENTITYTOTALWAR_API FETW_MassCopsuleFragment final : public FMassFragment
{
	GENERATED_BODY()

	void SetCapsuleComponent(UCapsuleComponent* Capsule) { CapsuleComponent = Capsule; }
	
	UCapsuleComponent* GetMutableCapsuleComponent() const { return CapsuleComponent; }
	UCapsuleComponent* const GetCapsuleComponent() const { return CapsuleComponent; }
	
	
protected:
	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CapsuleComponent;
};
