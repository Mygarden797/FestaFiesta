// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Types/FF_SimulationSettings.h"

const UFF_SimulationSettings* UFF_SimulationSettings::Get()
{
	return GetDefault<UFF_SimulationSettings>();
}
