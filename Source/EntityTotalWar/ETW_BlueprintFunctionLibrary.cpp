// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_BlueprintFunctionLibrary.h"

void UETW_BlueprintFunctionLibrary::K2_SetAutonomusProxy(AActor* Actor, const bool bInAutonomousProxy, const bool bAllowForcePropertyCompare)
{
	if (IsValid(Actor))
	{
		Actor->SetAutonomousProxy(bInAutonomousProxy, bAllowForcePropertyCompare);
	}
}