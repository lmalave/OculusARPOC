// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusARPOC.h"
#include "OculusARPOCHUD.h"
#include "Engine.h"
#include "Engine/Canvas.h"
#include "TextureResource.h"
#include "CanvasItem.h"

AOculusARPOCHUD::AOculusARPOCHUD(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshiarTexObj(TEXT("/Game/FirstPerson/Textures/FirstPersonCrosshair"));
	CrosshairTex = CrosshiarTexObj.Object;
}


void AOculusARPOCHUD::DrawHUD()
{
	Super::DrawHUD();

	// Draw very simple crosshair

	// find center of the Canvas
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (Center.X - (CrosshairTex->GetSurfaceWidth() * 0.5)),
										   (Center.Y - (CrosshairTex->GetSurfaceHeight() * 0.5f)) );

	// draw the crosshair
	FCanvasTileItem TileItem( CrosshairDrawPosition, CrosshairTex->Resource, FLinearColor::White);
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );

	if (GEngine->HMDDevice.IsValid()) {
		FColor BorderColor(50, 50, 50);
		float BorderTopBottomThickness = 720.0; // best for 1280x720 with screen at 5m distance and 4.5 m height
		float BorderLeftRightThickness = 130.0; // best for 1280x720 with screen at 5m distance and 4.5m height

		FCanvasLineItem TopBorder(FVector2D(0, 0), FVector2D(Canvas->ClipX, 0));
		TopBorder.SetColor(BorderColor);
		TopBorder.LineThickness = BorderTopBottomThickness;
		FCanvasLineItem BottomBorder(FVector2D(0, Canvas->ClipY), FVector2D(Canvas->ClipX, Canvas->ClipY));
		BottomBorder.SetColor(BorderColor);
		BottomBorder.LineThickness = BorderTopBottomThickness;
		FCanvasLineItem LeftBorder(FVector2D(0, 0), FVector2D(0, Canvas->ClipY));
		LeftBorder.SetColor(BorderColor);
		LeftBorder.LineThickness = BorderLeftRightThickness;
		FCanvasLineItem RightBorder(FVector2D(Canvas->ClipX, 0), FVector2D(Canvas->ClipX, Canvas->ClipY));
		RightBorder.SetColor(BorderColor);
		RightBorder.LineThickness = BorderLeftRightThickness;
		Canvas->DrawItem(TopBorder);
		Canvas->DrawItem(BottomBorder);
		Canvas->DrawItem(LeftBorder);
		Canvas->DrawItem(RightBorder);
	}
}

