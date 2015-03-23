// Fill out your copyright notice in the Description page of Project Settings.

#include "OculusARPOC.h"
#include "OculusARPOCPlayerController.h"
#include "Engine.h"
#include "IHeadMountedDisplay.h"


void AOculusARPOCPlayerController::UpdateRotation(float DeltaTime)
{
	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);

	FRotator NewControlRotation = GetControlRotation();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, NewControlRotation, DeltaRot);
	}

	SetControlRotation(NewControlRotation);

	if (!PlayerCameraManager || !PlayerCameraManager->bFollowHmdOrientation)
	{
		if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
		{
			FQuat HMDOrientation;
			FVector HMDPosition;

			// Disable bUpdateOnRT if using this method.
			if (!HMDInitialized) { // as soon as HMD is available reset the orientation
				GEngine->HMDDevice->ResetOrientationAndPosition(0.0);
				HMDInitialized = true;
			}
			GEngine->HMDDevice->GetCurrentOrientationAndPosition(HMDOrientation, HMDPosition);

			FRotator NewViewRotation = HMDOrientation.Rotator();

			// Only keep the yaw component from the controller.
			NewViewRotation.Yaw += NewControlRotation.Yaw;

			SetViewRotation(NewViewRotation);
		}
	}

	APawn* const P = GetPawnOrSpectator();
	if (P)
	{
		P->FaceRotation(NewControlRotation, DeltaTime);
	}
}

void AOculusARPOCPlayerController::SetControlRotation(const FRotator& NewRotation)
{
	ControlRotation = NewRotation;

	// Anything that is overriding view rotation will need to 
	// call SetViewRotation() after SetControlRotation().
	SetViewRotation(NewRotation);

	if (RootComponent && RootComponent->bAbsoluteRotation)
	{
		RootComponent->SetWorldRotation(GetControlRotation());
	}
}

void AOculusARPOCPlayerController::SetViewRotation(const FRotator& NewRotation)
{
	ViewRotation = NewRotation;
}

FRotator AOculusARPOCPlayerController::GetViewRotation() const
{
	return ViewRotation;
}



