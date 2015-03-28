// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusARPOC.h"
#include "OculusARPOCCharacter.h"
#include "OculusARPOCPlayerController.h"
#include "OculusARPOCProjectile.h"
#include "OpenCVVideoSource.h"
#include "UISurfaceActor.h"
#include "VideoDisplaySurface.h"
#include "Animation/AnimInstance.h"
#include "Engine.h"
#include "IHeadMountedDisplay.h"
#include "Leap.h"
#include "GameFramework/InputSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AOculusARPOCCharacter

AOculusARPOCCharacter::AOculusARPOCCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->AttachParent = GetCapsuleComponent();
	FirstPersonCameraComponent->RelativeLocation = FVector(0, 0, 64.f); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 30.0f, 10.0f);

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	Mesh1P->AttachParent = FirstPersonCameraComponent;
	Mesh1P->RelativeLocation = FVector(0.f, 0.f, -150.f);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P are set in the
	// derived blueprint asset named MyCharacter (to avoid direct content references in C++)


	static ConstructorHelpers::FObjectFinder<UBlueprint> UISurfaceBlueprint(TEXT("Blueprint'/Game/Blueprints/UISurfaceBlueprint.UISurfaceBlueprint'"));
	if (UISurfaceBlueprint.Object){
		UISurfaceBlueprintClass = (UClass*)UISurfaceBlueprint.Object->GeneratedClass;
	}
	static ConstructorHelpers::FObjectFinder<UBlueprint> VideoSurfaceBlueprint(TEXT("Blueprint'/Game/Blueprints/VideoDisplaySurfaceBlueprint.VideoDisplaySurfaceBlueprint'"));
	if (VideoSurfaceBlueprint.Object){
		VideoSurfaceBlueprintClass = (UClass*)VideoSurfaceBlueprint.Object->GeneratedClass;
	}
	static ConstructorHelpers::FObjectFinder<UBlueprint> PropMeshBlueprint(TEXT("Blueprint'/Game/Blueprints/CapnBlueprint.CapnBlueprint'"));
	if (PropMeshBlueprint.Object){
		PropMeshBlueprintClass = (UClass*)PropMeshBlueprint.Object->GeneratedClass;
	}
	// Blueprint'/Game/Blueprints/CapnBlueprint.CapnBlueprint'
	// Blueprint'/Game/Blueprints/CarBlueprint.CarBlueprint'

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	BackgroundVideoSurface = ObjectInitializer.CreateDefaultSubobject<UChildActorComponent>(this, TEXT("BackgroundVideoSurface"));
	BackgroundVideoSurface->AttachParent = FirstPersonCameraComponent;
	BackgroundVideoSurface->ChildActorClass = VideoSurfaceBlueprintClass;
	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P are set in the
	// derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	PrimaryActorTick.bCanEverTick = true;

	LeapEnable = true;
	LeapDrawSimpleHands = true;
	RaytraceInputEnable = true;

	IsInWindowMoveMode = false;

	BoardWindowIsSpawned = false;
	FDateTime LastMarkerSpawnTime = FDateTime::Now();

	SpawnActorAtMarker = true;
	SpawnedActorFacesCharacter = true;
	SpawnedActorFollowsMarkerLocation = true;
	SpawnedActorFollowsMarkerRotation = true;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AOculusARPOCCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	InputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	
	//InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AOculusARPOCCharacter::TouchStarted);
	if( EnableTouchscreenMovement(InputComponent) == false )
	{
		InputComponent->BindAction("Fire", IE_Pressed, this, &AOculusARPOCCharacter::OnFire);
	}
	
	InputComponent->BindAxis("MoveForward", this, &AOculusARPOCCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AOculusARPOCCharacter::MoveRight);
	
	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &AOculusARPOCCharacter::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AOculusARPOCCharacter::LookUpAtRate);
}

void AOculusARPOCCharacter::OnFire()
{ 
	// try and fire a projectile
	if (ProjectileClass != NULL)
	{
		const FRotator SpawnRotation = GetControlRotation();
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		const FVector SpawnLocation = GetActorLocation() + SpawnRotation.RotateVector(GunOffset);

		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			// spawn the projectile at the muzzle
			World->SpawnActor<AOculusARPOCProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
		}
	}

	// try and play the sound if specified
	if (FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if(FireAnimation != NULL)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if(AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}

}

void AOculusARPOCCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if( TouchItem.bIsPressed == true )
	{
		return;
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void AOculusARPOCCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	if( ( FingerIndex == TouchItem.FingerIndex ) && (TouchItem.bMoved == false) )
	{
		OnFire();
	}
	TouchItem.bIsPressed = false;
}

void AOculusARPOCCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if ((TouchItem.bIsPressed == true) && ( TouchItem.FingerIndex==FingerIndex))
	{
		if (TouchItem.bIsPressed)
		{
			if (GetWorld() != nullptr)
			{
				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
				if (ViewportClient != nullptr)
				{
					FVector MoveDelta = Location - TouchItem.Location;
					FVector2D ScreenSize;
					ViewportClient->GetViewportSize(ScreenSize);
					FVector2D ScaledDelta = FVector2D( MoveDelta.X, MoveDelta.Y) / ScreenSize;									
					if (ScaledDelta.X != 0.0f)
					{
						TouchItem.bMoved = true;
						float Value = ScaledDelta.X * BaseTurnRate;
						AddControllerYawInput(Value);
					}
					if (ScaledDelta.Y != 0.0f)
					{
						TouchItem.bMoved = true;
						float Value = ScaledDelta.Y* BaseTurnRate;
						AddControllerPitchInput(Value);
					}
					TouchItem.Location = Location;
				}
				TouchItem.Location = Location;
			}
		}
	}
}

void AOculusARPOCCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AOculusARPOCCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AOculusARPOCCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AOculusARPOCCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool AOculusARPOCCharacter::EnableTouchscreenMovement(class UInputComponent* InputComponent)
{
	bool bResult = false;
	if(FPlatformMisc::GetUseVirtualJoysticks() || GetDefault<UInputSettings>()->bUseMouseForTouch )
	{
		bResult = true;
		InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AOculusARPOCCharacter::BeginTouch);
		InputComponent->BindTouch(EInputEvent::IE_Released, this, &AOculusARPOCCharacter::EndTouch);
		InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AOculusARPOCCharacter::TouchUpdate);
	}
	return bResult;
}

void AOculusARPOCCharacter::ResetHMD()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("================================================Resetting HMD"));
	GEngine->HMDDevice->ResetOrientationAndPosition(0.0);
}

void AOculusARPOCCharacter::ToggleWindowMoveMode()
{
	if (SelectedUISurfaceActor != nullptr) {
		if (IsInWindowMoveMode) {
			IsInWindowMoveMode = false;
			SelectedUISurfaceActor->EnableInput(UGameplayStatics::GetPlayerController(GetWorld(), 0)); // enable input?  raytrace should re-enable it, actually
		}
		else {
			IsInWindowMoveMode = true;
			SelectedUISurfaceActor->DisableInput(UGameplayStatics::GetPlayerController(GetWorld(), 0)); // disable input while we're moving
		}
	}
}

void AOculusARPOCCharacter::SpawnUIWindowAtCameraDirectionWithParams(FString URL, float WindowScaleX, float WindowScaleY, float PixelWidth, float DistanceInUnrealUnits) { // NOTE: base size is 1 M square
	UWorld* const World = GetWorld();
	if (World){
		FActorSpawnParameters SpawnParams;
		SpawnParams.Instigator = this;
		FVector SpawnLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * DistanceInUnrealUnits;
		// NOTE: Camera and ShapePlane mesh have different coordinate systems so can't just add rotators
		FRotator SpawnRotation = FRotator(0.f - FirstPersonCameraComponent->GetComponentRotation().Roll, 90.f + FirstPersonCameraComponent->GetComponentRotation().Yaw, 90.f + FirstPersonCameraComponent->GetComponentRotation().Pitch);

		AUISurfaceActor* NewUISurface = (AUISurfaceActor*)World->SpawnActor(UISurfaceBlueprintClass, &SpawnLocation, &SpawnRotation, SpawnParams);
		NewUISurface->SetActorRelativeScale3D(FVector(WindowScaleX, WindowScaleY, 0.f));
		NewUISurface->CoherentUIViewURL = URL;
		NewUISurface->CoherentUIViewPixelWidth = PixelWidth;
		NewUISurface->InitializeView();
	}
}

AActor* AOculusARPOCCharacter::SpawnActor(UClass* BlueprintClass, FVector Location, FRotator Rotation) { // NOTE: base size is 1 M square
	UWorld* const World = GetWorld();
	if (World){
		FActorSpawnParameters SpawnParams;
		SpawnParams.Instigator = this;
		AActor* NewActor = World->SpawnActor(BlueprintClass, &Location, &Rotation, SpawnParams);
		return NewActor;
	}
	return NULL;
}

void AOculusARPOCCharacter::HandleZoomInWindow() {
	FVector CurrentLocation = SelectedUISurfaceActor->GetActorLocation();
	float CurrentDistance = (SelectedUISurfaceActor->GetActorLocation() - FirstPersonCameraComponent->GetComponentLocation()).Size();
	FVector NewLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * CurrentDistance * 0.9;
	SelectedUISurfaceActor->SetActorLocation(NewLocation);
}

void AOculusARPOCCharacter::HandleZoomOutWindow() {
	FVector CurrentLocation = SelectedUISurfaceActor->GetActorLocation();
	float CurrentDistance = (SelectedUISurfaceActor->GetActorLocation() - FirstPersonCameraComponent->GetComponentLocation()).Size();
	FVector NewLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * CurrentDistance * 1.1;
	SelectedUISurfaceActor->SetActorLocation(NewLocation);
}

void AOculusARPOCCharacter::HandleMoveWindow() {
	FVector CurrentLocation = SelectedUISurfaceActor->GetActorLocation();
	float CurrentDistance = (SelectedUISurfaceActor->GetActorLocation() - FirstPersonCameraComponent->GetComponentLocation()).Size();
	FVector NewLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * CurrentDistance;
	// NOTE: Camera and ShapePlane mesh have different coordinate systems so can't just add rotators
	FRotator NewRotation = FRotator(0.f - FirstPersonCameraComponent->GetComponentRotation().Roll, 90.f + FirstPersonCameraComponent->GetComponentRotation().Yaw, 90.f + FirstPersonCameraComponent->GetComponentRotation().Pitch);
	SelectedUISurfaceActor->SetActorLocation(NewLocation);
	SelectedUISurfaceActor->SetActorRotation(NewRotation);
}

void AOculusARPOCCharacter::HandleMarkerActor() {
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("In HandleMarkerActor()"));
	if (MarkerDetector->IsDetected()) {
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Marker condition is detected!"));
		FVector DetectedTranslation = MarkerDetector->GetDetectedTranslation();
		FRotator DetectedRotation = MarkerDetector->GetDetectedRotation();
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("DetectedRotation: ") + DetectedRotation.ToCompactString());
		FVector ActorLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * DetectedTranslation.X + FirstPersonCameraComponent->GetRightVector() * DetectedTranslation.Y + FirstPersonCameraComponent->GetUpVector() * DetectedTranslation.Z;

		// NOTE: Camera and ShapePlane mesh have different coordinate systems so can't just add rotators
		// Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// ShapePlane: plane is on "floor" so Z is normal to the plane. Assume textures like UI textures draw with same x/y orientation as browser window
		// Chair:  x = forward, y = right, z = up, same as character, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// NOTE:  FRotator is (pitch, yaw, roll)

		//FRotator BoardRotation = FRotator(0.f - FirstPersonCameraComponent->GetComponentRotation().Roll - (DetectedBoardRotation.Roll + 180.f), 90.f + FirstPersonCameraComponent->GetComponentRotation().Yaw - DetectedBoardRotation.Yaw * 0.f, 90.f + FirstPersonCameraComponent->GetComponentRotation().Pitch + DetectedBoardRotation.Pitch);
		FRotator ActorRotation = FRotator::ZeroRotator;
		if (SpawnedActorFacesCharacter) {
			FRotator RotationToFaceCharacter = FRotator(0.f - FirstPersonCameraComponent->GetComponentRotation().Roll, 90.f + FirstPersonCameraComponent->GetComponentRotation().Yaw, 90.f + FirstPersonCameraComponent->GetComponentRotation().Pitch); // NOTE: this is for 2D shape plane mesh!  For other meshes may want different rotation
			ActorRotation = ActorRotation + RotationToFaceCharacter;
		}
		if (SpawnedActorFollowsMarkerRotation) {
			// general definitions:  pitch is rotation on Y axis, yaw is rotation on Z axis, roll is rotation on Y axis
			// For board: -Y axis points up (z forward and -X to right), so rotating  on vertical axis = rotating on Y axis = pitch  (for character Y = right vector)
			//FRotator RotationToFollowBoard(-(DetectedBoardRotation.Roll + 180.f),  DetectedBoardRotation.Pitch, DetectedBoardRotation.Yaw * 0.f); // this is for 2D UI surface as well?
			// FRotator RotationToFollowBoard(-DetectedBoardRotation.Yaw + 180.f, DetectedBoardRotation.Pitch + 0.f, -DetectedBoardRotation.Roll); // This is for board marker and chair actor.  X = forward, Y = right, Z = up (like character
			 //FRotator RotationToFollowBoard(-DetectedRotation.Roll + 180.f, DetectedRotation.Pitch + 0.f, -DetectedRotation.Yaw - 90.f); // This is for board marker and Capn actor. -X = right, Y = forward, Z = up 
			 //FRotator RotationToFollowBoard(-DetectedRotation.Roll + 180.f, DetectedRotation.Pitch + 0.f, -DetectedRotation.Yaw - 90.f); // This is for plane markers and Capn actor. -X = right, Y = forward, Z = up 
			 FRotator RotationToFollowBoard(-DetectedRotation.Roll + 180.f, DetectedRotation.Pitch + 90.f, -DetectedRotation.Yaw - 90.f); // This is for plane markers and Capn actor. -X = right, Y = forward, Z = up 
			 // for single marker: -X axis points up, Z to the right, and Y forward
			//FRotator RotationToFollowBoard(DetectedBoardRotation.Pitch - 90.f /* roll */, -DetectedBoardRotation.Yaw + 90.f /* yaw */, -DetectedBoardRotation.Roll + 180.f /* pitch */); // This is for board marker and Capn actor. -X = right, Y = forward, Z = up 
			ActorRotation = ActorRotation + RotationToFollowBoard;
		}
		if (BoardFollowActor != NULL) {
			if (SpawnedActorFollowsMarkerLocation) {
				BoardFollowActor->SetActorLocation(ActorLocation);
			}
			if (SpawnedActorFollowsMarkerRotation) {
				BoardFollowActor->SetActorRotation(ActorRotation);
			}
		}
		else {
			if (SpawnActorAtMarker) {
				GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("About to spawn actor at location: ") + ActorLocation.ToCompactString() + TEXT(", rotation: ") + ActorRotation.ToCompactString());
				//AUISurfaceActor* NewUISurface = (AUISurfaceActor*)this->SpawnActor(UISurfaceBlueprintClass, ActorLocation, ActorRotation);
				//NewUISurface->SetActorRelativeScale3D(FVector(0.2, 0.2, 0.f));
				//NewUISurface->CoherentUIViewURL = TEXT("http://www.google.com");
				//NewUISurface->CoherentUIViewPixelWidth = 1024;
				//NewUISurface->InitializeView();
				//BoardFollowActor = NewUISurface;
				AActor* Prop = this->SpawnActor(PropMeshBlueprintClass, ActorLocation, ActorRotation);
				Prop->SetActorRelativeScale3D(FVector(0.2, 0.2, 0.2));
				BoardFollowActor = Prop;
			}
		}
	}
}

void AOculusARPOCCharacter::BeginPlay()
{
	AVideoDisplaySurface* BackgroundVideoDisplaySurface = (AVideoDisplaySurface*)BackgroundVideoSurface->ChildActor;
	VideoSource = new OpenCVVideoSource(0, 1280, 720);
	VideoSource->SetIsCameraUpsideDown(false);
	VideoSource->Init();
	
	MarkerDetector = new ArucoMarkerDetector();
	MarkerDetector->DetectMarkers = true;
	MarkerDetector->DetectSingleMarkerId = -1;
	///MarkerDetector->PlaneMarker1Id = 985;
	//MarkerDetector->PlaneMarker2Id = 299;
	//MarkerDetector->PlaneMarker3Id = 175;
	//MarkerDetector->PlaneMarker4Id = 461;
	MarkerDetector->PlaneMarker1Id = 698;
	MarkerDetector->PlaneMarker2Id = 683;
	MarkerDetector->PlaneMarker3Id = 795;
	MarkerDetector->PlaneMarker4Id = 819;
	MarkerDetector->DetectBoard = false;
	MarkerDetector->DetectPlaneMarkers = true;
	MarkerDetector->Init();
	VideoSource->SetArucoMarkerDetector(MarkerDetector);
	
	BackgroundVideoDisplaySurface->Init(VideoSource);
	BackgroundVideoSurface->RelativeLocation = FVector(500.f, -0.f, 0.f);
	BackgroundVideoSurface->RelativeRotation = FRotator(0.f, 90.f, 90.f);
	BackgroundVideoSurface->RelativeScale3D = FVector(8.89, 5.00, 1.0); // This is for 1280x720
	LeapController = new Leap::Controller();
	LeapController->setPolicy(Leap::Controller::POLICY_OPTIMIZE_HMD);
	LeapInput = new LeapInputReader(LeapController, this);
	UISurfaceRaytraceHandler = new UISurfaceRaytraceInputHandler(this, FirstPersonCameraComponent);
}

void AOculusARPOCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsInWindowMoveMode) {
		HandleMoveWindow();
	}
	else {
		UISurfaceRaytraceHandler->HandleRaytrace();
		HandleLeap();
		//HandleMarker();
	}
	HandleMarkerActor(); 
	//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("PlaneMarker1Translation: ") + MarkerDetector->PlaneMarker1Translation.ToCompactString());
	FVector PlaneMarker1Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker1Translation);
	FVector PlaneMarker2Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker2Translation);
	FVector PlaneMarker3Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker3Translation);
	FVector PlaneMarker4Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker4Translation);
	DrawDebugSphere(GetWorld(), PlaneMarker1Location, 0.5, 12, FColor::Magenta);
	DrawDebugSphere(GetWorld(), PlaneMarker2Location, 0.5, 12, FColor::Magenta);
	DrawDebugSphere(GetWorld(), PlaneMarker3Location, 0.5, 12, FColor::Magenta);
	DrawDebugSphere(GetWorld(), PlaneMarker4Location, 0.5, 12, FColor::Magenta);
	FVector CylinderTranslation = MarkerDetector->GetPlaneMarkersMidpoint();
	FVector CylinderStart = GetWorldLocationFromMarkerTranslation(CylinderTranslation);
	FVector MarkerNormalVector = MarkerDetector->GetPlaneMarkersNormalVector();
	FVector CylinderEnd = CylinderStart - FirstPersonCameraComponent->GetForwardVector() * MarkerNormalVector.X + FirstPersonCameraComponent->GetRightVector() * MarkerNormalVector.Y + FirstPersonCameraComponent->GetUpVector() * MarkerNormalVector.Z;
	FVector CylinderEnd2 = CylinderStart - FirstPersonCameraComponent->GetForwardVector() * MarkerNormalVector.X * 10.f + FirstPersonCameraComponent->GetRightVector() * MarkerNormalVector.Y * 10.f + FirstPersonCameraComponent->GetUpVector() * MarkerNormalVector.Z * 10.f;
	// draw inner "donut hole" cylinder only in activated case
	DrawDebugCylinder(GetWorld(), CylinderStart, CylinderEnd, 15.f, 12, FColor::Cyan);
	DrawDebugCylinder(GetWorld(), CylinderStart, CylinderEnd2, 2.f, 12, FColor::Magenta);

}

void AOculusARPOCCharacter::HandleLeap()
{
	if (LeapEnable == true) {
		LeapInput->UpdateHandLocations();
		// handle UI input
		if (LeapInput->IsValidInputLastFrame()) {
			ActionHandPalmLocation = LeapInput->GetRightPalmLocation_WorldSpace();
			ActionHandFingerLocation = LeapInput->GetRightFingerLocation_WorldSpace();
			AUISurfaceActor* SelectedUISurface = UISurfaceRaytraceHandler->GetSelectedUISurfaceActor();
			if (SelectedUISurface) {
				SelectedUISurface->HandleVirtualTouchInput(ActionHandPalmLocation, ActionHandFingerLocation);
			}
		}
	}
}


FRotator AOculusARPOCCharacter::GetViewRotation() const
{
	if (AOculusARPOCPlayerController* MYPC = Cast<AOculusARPOCPlayerController>(Controller))
	{
		return MYPC->GetViewRotation();
	}
	else if (Role < ROLE_Authority)
	{
		// check if being spectated
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController && PlayerController->PlayerCameraManager->GetViewTargetPawn() == this)
			{
				return PlayerController->BlendedTargetViewRotation;
			}
		}
	}

	return GetActorRotation();
}

FVector AOculusARPOCCharacter::GetWorldLocationFromMarkerTranslation(FVector MarkerTranslation) {
	FVector Location = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * MarkerTranslation.X + FirstPersonCameraComponent->GetRightVector() * MarkerTranslation.Y + FirstPersonCameraComponent->GetUpVector() * MarkerTranslation.Z;
	return Location;
}
