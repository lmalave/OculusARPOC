// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerController.h"
#include "OculusARPOCPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class OCULUSARPOC_API AOculusARPOCPlayerController : public APlayerController
{
	GENERATED_BODY()
	
	virtual void UpdateRotation(float DeltaTime) override;

	virtual void SetControlRotation(const FRotator& NewRotation) override;

protected:

	/**
	*  View & Movement direction are now separate.
	*  The controller rotation will determine which direction we will move.
	*  ViewRotation represents where we are looking.
	*/
	UPROPERTY()
	FRotator ViewRotation;

	UPROPERTY()
	bool HMDInitialized = false;

public:

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	FRotator GetViewRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void SetViewRotation(const FRotator& NewRotation);


	
	
};
