// Fill out your copyright notice in the Description page of Project Settings.


#include "ACFCameraPointComponent.h"


UACFCameraPointComponent::UACFCameraPointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	SetUsingAbsoluteScale(true);
}

