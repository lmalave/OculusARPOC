// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "GameFramework/HUD.h"
#include "OculusARPOCHUD.generated.h"

UCLASS()
class AOculusARPOCHUD : public AHUD
{
	GENERATED_BODY()

public:
	AOculusARPOCHUD(const FObjectInitializer& ObjectInitializer);

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

