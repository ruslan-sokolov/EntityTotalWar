// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_GameMode.h"
#include "ETW_PlayerCameraPawn.h"

AETW_GameMode::AETW_GameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = AETW_PlayerCameraPawn::StaticClass();
}
