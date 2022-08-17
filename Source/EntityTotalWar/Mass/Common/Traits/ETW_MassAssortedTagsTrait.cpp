// Fill out your copyright notice in the Description page of Project Settings.


#include "ETW_MassAssortedTagsTrait.h"
#include "MassEntityTemplateRegistry.h"


void UETW_MassAssortedTagsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	for (const FInstancedStruct& Tag : Tags)
	{
		if (Tag.IsValid())
		{
			const UScriptStruct* Type = Tag.GetScriptStruct();
			CA_ASSUME(Type);
			if (Type->IsChildOf(FMassTag::StaticStruct()))
			{
				BuildContext.AddTag(*Type);
			}
			else
			{
				UE_LOG(LogMass, Error, TEXT("Invalid tag type found: '%s'"), *GetPathNameSafe(Type));
			}
		}
	}
}
