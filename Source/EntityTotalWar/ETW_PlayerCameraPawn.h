// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ETW_PlayerCameraPawn.generated.h"

// this class represents camera similar to camera in CnC Generals
UCLASS()
class ENTITYTOTALWAR_API AETW_PlayerCameraPawn : public APawn
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	USceneComponent* RootComp;

	UPROPERTY(Category = Camera, EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* SpringArmComp;

	UPROPERTY(Category = Camera, EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraComp;

	UPROPERTY(Category = Camera, EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UMassCommanderComponent* MassCommanderComp;	
	
	// movement ------------------------------------------------ /

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraMoveSpeedMax;

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraMoveStopTime;

	// rotation ------------------------------------------------ /

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraRotationSpeed;

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraPitchMin;

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraPitchMax;

	// pitch value on start
	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraPitchInitial;

	// if button is released should we register mouse event as click instead of hold
	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float MouseClickSpeed;

	// deg in seconds
	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraRotationRestoreSpeed;

	// zoom ---------------------------------------------------- /

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float CameraZoomSpeed;

	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float OnZoomSpringArmLenMin;
	
	UPROPERTY(Category = Camera, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	float OnZoomSpringArmLenMax;
	
public:
	/** Name of the CharacterMovement component. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName MassCommanderComponentName;

public:
	AETW_PlayerCameraPawn();

public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaTime) override;

private:

	// input --------------------------------------------------- /

	bool bCameraShouldRotate;
	bool bCameraShouldMove;
	bool bSelectIsPressed;
	bool bApplyActionIsPressed;

	bool MouseXYInputIsUsed() const { return bCameraShouldMove || bCameraShouldRotate || bSelectIsPressed; }

	void CameraUpInput(float Val);
	void CameraRightInput(float Val);

	void OnSelect_Pressed();
	void OnSelect_Released();

	void OnApplyAction_Pressed();
	void OnApplyAction_Released();

	void OnRotateCamera_Pressed();
	void OnRotateCamera_Released();

	void OnMoveCamera_Pressed();
	void OnMoveCamera_Released();

	void OnCameraZoomIn();
	void OnCameraZoomOut();

	// movement ------------------------------------------------ /
	
	FVector Velocity;
	float BreakingSpeed;
	FVector2D CameraMoveCursorAnchor;
	FVector CursorPointMoveTo; // if not zero then move quick on cursor pointed location (set on double click)
	void CalcMoveVelocity_Tick(float DeltaTime);

	// rotation ------------------------------------------------ /
	
	bool bRotationRestoreInProgress;
	float CamRotButtonPressedTime;
	FORCEINLINE void RestoreCameraRotation_Tick(float DeltaTime);

	// select ------------------------------------------------- /

	float SelectButtonReleasedTime;  // check LMB double click

};
