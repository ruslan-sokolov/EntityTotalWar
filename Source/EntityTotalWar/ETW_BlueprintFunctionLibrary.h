// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ETW_BlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class ENTITYTOTALWAR_API UETW_BlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta=(DisplayName="SetAutonomusProxy"))
	static void K2_SetAutonomusProxy(AActor* Actor, const bool bInAutonomousProxy, const bool bAllowForcePropertyCompare = true);
	
};
