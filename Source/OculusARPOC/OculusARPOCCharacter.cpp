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
	static ConstructorHelpers::FObjectFinder<UBlueprint> PortalBlueprint(TEXT("Blueprint'/Game/Blueprints/SM_DoorFrame_BP.SM_DoorFrame_BP'"));
	if (PortalBlueprint.Object){
		PortalBlueprintClass = (UClass*)PortalBlueprint.Object->GeneratedClass;
	}
	static ConstructorHelpers::FObjectFinder<UBlueprint> ChairBlueprint(TEXT("Blueprint'/Game/Blueprints/SM_Chair_Blueprint.SM_Chair_Blueprint'"));
	if (ChairBlueprint.Object){
		ChairBlueprintClass = (UClass*)ChairBlueprint.Object->GeneratedClass;
	}
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

	ARStarted = false;
	StartingCharacterLocation = FVector::ZeroVector;
	StartingMarkerLocation = FVector::ZeroVector;

	PreviousCharacterLocationIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AOculusARPOCCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAction("ResetHMD", IE_Pressed, this, &AOculusARPOCCharacter::ResetHMD);
	//InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AOculusARPOCCharacter::TouchStarted);
	if (EnableTouchscreenMovement(InputComponent) == false)
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
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("In HandleMarkerActor()"));
	if (MarkerDetector->IsDetected()) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Marker condition is detected!"));
		FVector DetectedTranslation = MarkerDetector->GetDetectedTranslation();
		float markerDistance = DetectedTranslation.Size();
		FRotator DetectedRotation = MarkerDetector->GetDetectedRotation();
		FVector DetectedWorldLocation = GetWorldLocationFromMarkerTranslation(DetectedTranslation);
		FVector MarkerNormalVector = MarkerDetector->GetPlaneMarkersNormalVector();
		FVector DetectedWorldNormalVector = FirstPersonCameraComponent->GetForwardVector() * MarkerNormalVector.X + FirstPersonCameraComponent->GetRightVector() * MarkerNormalVector.Y + FirstPersonCameraComponent->GetUpVector() * MarkerNormalVector.Z;
		FRotator DetectedNormalWorldRotation = DetectedWorldNormalVector.Rotation();
		//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("DetectedRotation: ") + DetectedRotation.ToCompactString());
		FVector ActorLocation = DetectedWorldLocation;

		// NOTE: Camera and ShapePlane mesh have different coordinate systems so can't just add rotators
		// Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// ShapePlane: plane is on "floor" so Z is normal to the plane. Assume textures like UI textures draw with same x/y orientation as browser window
		// Chair:  x = forward, y = right, z = up, same as character, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// Capn:  y = forward, x = right, z = up
		// Car: y = forward, x = right, z = up
		FRotator ActorRotation = FRotator::ZeroRotator;
		if (SpawnedActorFacesCharacter) {
			FRotator RotationToFaceCharacter = FRotator(0.f - FirstPersonCameraComponent->GetComponentRotation().Roll, 90.f + FirstPersonCameraComponent->GetComponentRotation().Yaw, 90.f + FirstPersonCameraComponent->GetComponentRotation().Pitch); // NOTE: this is for 2D shape plane mesh!  For other meshes may want different rotation
			ActorRotation = ActorRotation + RotationToFaceCharacter;
		}
		if (SpawnedActorFollowsMarkerRotation) {
			// For Capn or Car.  NOTE:  Don't know why for Capn seems to be taking absolute value of Yaw (but not for car)   
			FRotator RotationToFaceCharacter = FRotator(0.f, 90.f, 0.f); // NOTE: this is for Capn or Car mesh!  For other meshes may want different rotation
			FRotator AdditionalRotation(0.f, -DetectedNormalWorldRotation.Yaw, -DetectedNormalWorldRotation.Pitch);
			ActorRotation = RotationToFaceCharacter + AdditionalRotation;
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
				Prop->SetActorRelativeScale3D(FVector(0.05, 0.05, 0.05));
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
	// these below are on the board printout
	//MarkerDetector->PlaneMarker1Id = 985;
	//MarkerDetector->PlaneMarker2Id = 299;
	//MarkerDetector->PlaneMarker3Id = 760;
	//MarkerDetector->PlaneMarker4Id = 977;
	// these below are the H-U-G-E plane markers
	MarkerDetector->PlaneMarker1Id = 666;
	MarkerDetector->PlaneMarker2Id = 683;
	MarkerDetector->PlaneMarker3Id = 775;
	MarkerDetector->PlaneMarker4Id = 819;
	MarkerDetector->DetectBoard = false;
	MarkerDetector->DetectPlaneMarkers = true;
	MarkerDetector->Init();
	VideoSource->SetArucoMarkerDetector(MarkerDetector);
	
	BackgroundVideoDisplaySurface->Init(VideoSource);
	BackgroundVideoSurface->RelativeLocation = FVector(500.f, -0.f, 0.f);
	BackgroundVideoSurface->RelativeRotation = FRotator(0.f, 90.f, 90.f);
	BackgroundVideoSurface->RelativeScale3D = FVector(8.00, 4.50, 1.0); // This is for 1280x720
	//BackgroundVideoSurface->RelativeScale3D = FVector(7.11, 4.00, 1.0); // This is for 1280x720
	//BackgroundVideoSurface->RelativeScale3D = FVector(5.33, 3.00, 1.0); // This is for 1280x720
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
	//HandleMarkerActor(); 
	HandleMarkerCharacterMovement();
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

FVector AOculusARPOCCharacter::GetWorldLocationFromMarkerTranslation(FVector MarkerTranslation)
{
	FVector WorldLocation = FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * MarkerTranslation.X + FirstPersonCameraComponent->GetRightVector() * MarkerTranslation.Y + FirstPersonCameraComponent->GetUpVector() * MarkerTranslation.Z;
	return WorldLocation;
}

FVector AOculusARPOCCharacter::GetWorldMarkerNormalVector(FVector MarkerNormalVector)
{
	FVector WorldNormalVector = FirstPersonCameraComponent->GetForwardVector() * MarkerNormalVector.X + FirstPersonCameraComponent->GetRightVector() * MarkerNormalVector.Y + FirstPersonCameraComponent->GetUpVector() * MarkerNormalVector.Z;
	return WorldNormalVector;
}

void AOculusARPOCCharacter::StartAR()
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("In HandleMarkerActor()"));
	if (!ARStarted && MarkerDetector->IsDetected()) {
		ARStarted = true;
		StartingCharacterLocation = this->GetActorLocation();
		StartingCameraLocation = FirstPersonCameraComponent->GetComponentLocation();
		StartingCameraForwardVector = FirstPersonCameraComponent->GetForwardVector();
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Marker condition is detected!"));
		FVector DetectedTranslation = MarkerDetector->GetDetectedTranslation();
		StartingMarkerTranslation = DetectedTranslation;
		float markerDistance = DetectedTranslation.Size();
		StartingMarkerDistance = markerDistance;
		FRotator DetectedRotation = MarkerDetector->GetPlaneMarkersRotation();
		StartingMarkerRotation = DetectedRotation;
		FVector DetectedWorldLocation = GetWorldLocationFromMarkerTranslation(DetectedTranslation);
		StartingMarkerLocation = DetectedWorldLocation;
		FVector MarkerNormalVector = MarkerDetector->GetPlaneMarkersNormalVector();
		StartingMarkerNormalVector = MarkerNormalVector;
		FVector DetectedWorldNormalVector = GetWorldMarkerNormalVector(MarkerNormalVector);
		FRotator DetectedNormalWorldRotation = DetectedWorldNormalVector.Rotation();
		FTransform DetectedMarkerTransform(DetectedNormalWorldRotation, DetectedWorldLocation, *(new FVector(1.f, 1.f, 1.f)));
		StartingMarkerTransform = DetectedMarkerTransform;
		//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("DetectedRotation: ") + DetectedRotation.ToCompactString());
		FVector ActorLocation = DetectedWorldLocation;

		// NOTE: Camera and ShapePlane mesh have different coordinate systems so can't just add rotators
		// Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// ShapePlane: plane is on "floor" so Z is normal to the plane. Assume textures like UI textures draw with same x/y orientation as browser window
		// Chair:  x = forward, y = right, z = up, same as character, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
		// Capn:  y = forward, x = right, z = up
		// Car: y = forward, x = right, z = up
		// For Capn or Car.  NOTE:  Don't know why for Capn seems to be taking absolute value of Yaw (but not for car)   
		// NOTE: this RotationToFaceCharacter is correct.   If the image is flipped it's because of the AdditionalRotation component.	
		FRotator RotationToFaceCharacter = FRotator(0.f, 90.f, 0.f); // NOTE: this is for Capn or Car mesh!  For other meshes may want different rotation

		FRotator AdditionalRotation(0.f, -DetectedNormalWorldRotation.Yaw, -DetectedNormalWorldRotation.Pitch);
		//FRotator ActorRotation = RotationToFaceCharacter + DetectedRotation;
		FRotator ActorRotation = RotationToFaceCharacter;
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("About to spawn actor at location: ") + ActorLocation.ToCompactString() + TEXT(", rotation: ") + ActorRotation.ToCompactString());
		//AUISurfaceActor* NewUISurface = (AUISurfaceActor*)this->SpawnActor(UISurfaceBlueprintClass, ActorLocation, ActorRotation);
		//NewUISurface->SetActorRelativeScale3D(FVector(0.2, 0.2, 0.f));
		//NewUISurface->CoherentUIViewURL = TEXT("http://www.google.com");
		//NewUISurface->CoherentUIViewPixelWidth = 1024;
		//NewUISurface->InitializeView();
		//BoardFollowActor = NewUISurface;
		AActor* Prop = this->SpawnActor(PropMeshBlueprintClass, ActorLocation, ActorRotation);
		Prop->SetActorRelativeScale3D(FVector(0.50, 0.50, 0.50));
		BoardFollowActor = Prop;
		AActor* Portal = this->SpawnActor(PortalBlueprintClass, FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 30.f - FirstPersonCameraComponent->GetUpVector() * 100.f, FRotator::ZeroRotator);
		Portal->SetActorRelativeScale3D(FVector(0.5, 0.7   , 0.7));
		AActor* Chair1 = this->SpawnActor(ChairBlueprintClass, FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 75.f - FirstPersonCameraComponent->GetRightVector() * 75.f - FirstPersonCameraComponent->GetUpVector() * 64.f, FRotator(0.f, 90.f, 0.f));
		Chair1->SetActorRelativeScale3D(FVector(0.7, 0.7, 0.7));
		AActor* Chair2 = this->SpawnActor(ChairBlueprintClass, FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 75.f + FirstPersonCameraComponent->GetRightVector() * 75.f - FirstPersonCameraComponent->GetUpVector() * 64.f, FRotator(0.f, -90.f, 0.f));
		Chair2->SetActorRelativeScale3D(FVector(0.7, 0.7, 0.7));
	}
}

void AOculusARPOCCharacter::HandleMarkerCharacterMovement()
{
	if (ARStarted && MarkerDetector->IsDetected())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("PlaneMarker1Translation: ") + MarkerDetector->PlaneMarker1Translation.ToCompactString());
		FVector PlaneMarker1Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker1Translation);
		FVector PlaneMarker2Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker2Translation);
		FVector PlaneMarker3Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker3Translation);
		FVector PlaneMarker4Location = GetWorldLocationFromMarkerTranslation(MarkerDetector->PlaneMarker4Translation);
		DrawDebugSphere(GetWorld(), PlaneMarker1Location, 0.5, 12, FColor::Magenta);
		DrawDebugSphere(GetWorld(), PlaneMarker2Location, 0.5, 12, FColor::Magenta);
		DrawDebugSphere(GetWorld(), PlaneMarker3Location, 0.5, 12, FColor::Magenta);
		DrawDebugSphere(GetWorld(), PlaneMarker4Location, 0.5, 12, FColor::Magenta);
		FVector MarkerTranslation = MarkerDetector->GetPlaneMarkersMidpoint();
		float MarkerDistance = MarkerTranslation.Size();
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================StartingMarkerTranslation: ") + MarkerTranslation.ToCompactString());
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================MarkerTranslation: ") + MarkerTranslation.ToCompactString());
		FVector MarkerLocation = GetWorldLocationFromMarkerTranslation(MarkerTranslation);
		DrawDebugSphere(GetWorld(), MarkerLocation, 0.5, 12, FColor::Red);
		FVector MarkerNormalVector = MarkerDetector->GetPlaneMarkersNormalVector();
		FVector WorldMarkerNormalVector = GetWorldMarkerNormalVector(MarkerNormalVector);
		FRotator MarkerRotation = MarkerDetector->GetPlaneMarkersRotation();
		FRotator WorldMarkerRotation = WorldMarkerNormalVector.Rotation();
		FTransform WorldMarkerTransform(WorldMarkerRotation, MarkerLocation, *(new FVector(1.f, 1.f, 1.f)));
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================MarkerRotation: ") + MarkerRotation.ToCompactString());
		FVector CylinderEnd = MarkerLocation + WorldMarkerNormalVector;
		FVector CylinderEnd2 = MarkerLocation + WorldMarkerNormalVector * 20.f;
		DrawDebugCylinder(GetWorld(), MarkerLocation, CylinderEnd, 15.f, 12, FColor::Cyan);
		DrawDebugCylinder(GetWorld(), MarkerLocation, CylinderEnd2, 1.f, 12, FColor::Magenta);
		FVector CameraLocation = FirstPersonCameraComponent->GetComponentLocation();
		FVector CameraLocationInMarkerSpace = WorldMarkerTransform.InverseTransformPositionNoScale(CameraLocation);
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================CameraLocation: ") + CameraLocation.ToCompactString());
		GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================CameraLocationInMarkerSpace: ") + CameraLocationInMarkerSpace.ToCompactString());
		//DrawDebugLine(GetWorld(), FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 50 + FirstPersonCameraComponent->GetRightVector() * 50, FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 100 + FirstPersonCameraComponent->GetRightVector() * 100, FColor::Red);
		//DrawDebugLine(GetWorld(), FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 50 - FirstPersonCameraComponent->GetRightVector() * 50, FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * 100 - FirstPersonCameraComponent->GetRightVector() * 100, FColor::Red);
		//FVector ForwardCylinderEnd = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorForwardVector() * 20;
		//FVector RightCylinderEnd = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorRightVector() * 20;
		//FVector UpCylinderEnd = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorUpVector() * 20;
		//DrawDebugCylinder(GetWorld(), BoardFollowActor->GetActorLocation(), ForwardCylinderEnd, 1.f, 12, FColor::Red);
		//DrawDebugCylinder(GetWorld(), BoardFollowActor->GetActorLocation(), RightCylinderEnd, 1.f, 12, FColor::Green);
		//DrawDebugCylinder(GetWorld(), BoardFollowActor->GetActorLocation(), UpCylinderEnd, 1.f, 12, FColor::Blue);
		if (!StartingCharacterLocation.IsZero() && !StartingMarkerLocation.IsZero())
		{
			FRotator MarkerRotationDelta = MarkerRotation - StartingMarkerRotation; // +Pitch is character looking down, +Yaw is character looking right
			// reverse pitch?  why?
			MarkerRotationDelta.Pitch = -MarkerRotationDelta.Pitch;
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================MarkerRotationDelta: ") + MarkerRotationDelta.ToCompactString());
			FVector AdjustedMarkerTranslation = MarkerRotationDelta.RotateVector(MarkerTranslation);
			FVector AdjustedMarkerTranslationMovingAverage = GetAdjustedMarkerTranslationMovingAverage(AdjustedMarkerTranslation);
			FVector AdjustedCameraPosition = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorRightVector() * AdjustedMarkerTranslationMovingAverage.X - BoardFollowActor->GetActorForwardVector() * AdjustedMarkerTranslationMovingAverage.Y - BoardFollowActor->GetActorUpVector() * AdjustedMarkerTranslationMovingAverage.Z;
			FVector AdjustedCameraTranslation = FVector(AdjustedMarkerTranslationMovingAverage.X, -AdjustedMarkerTranslationMovingAverage.Y, -AdjustedMarkerTranslationMovingAverage.Z);
			FVector AdjustedCameraLocation = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorRightVector() * AdjustedMarkerTranslationMovingAverage.X - BoardFollowActor->GetActorForwardVector() * AdjustedMarkerTranslationMovingAverage.Y - BoardFollowActor->GetActorUpVector() * AdjustedMarkerTranslationMovingAverage.Z;
			//FVector AdjustedCameraLocation = StartingMarkerTransform.TransformPosition(AdjustedCameraTranslation);
			FVector AdjustedMarkerLocationMovingAverage = GetWorldLocationFromMarkerTranslation(AdjustedMarkerTranslationMovingAverage);
			//DrawDebugSphere(GetWorld(), AdjustedMarkerLocationMovingAverage, 1.0, 12, FColor::Green);
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================StartingMarkerLocation: ") + StartingMarkerLocation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================StartingMarkerTranslation: ") + StartingMarkerTranslation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================AdjustedMarkerTranslation: ") + AdjustedMarkerTranslation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================StartingCameraLocation: ") + StartingCameraLocation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================AdjustedCameraTranslation: ") + AdjustedCameraTranslation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================AdjustedCameraLocation: ") + AdjustedCameraLocation.ToCompactString());
			FVector MarkerTranslationDelta = StartingMarkerTranslation - MarkerTranslation;
			FVector MarkerLocationDelta = StartingMarkerLocation - MarkerLocation;
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================MarkerTranslationDelta: ") + MarkerTranslationDelta.ToCompactString());
			//FVector NewCharacterLocation = StartingCharacterLocation + StartingCameraForwardVector * (StartingMarkerDistance - MarkerDistance);
			FVector NewCharacterLocation = BoardFollowActor->GetActorLocation() + BoardFollowActor->GetActorRightVector() * CameraLocationInMarkerSpace.X - BoardFollowActor->GetActorForwardVector() * CameraLocationInMarkerSpace.Y + BoardFollowActor->GetActorUpVector() * CameraLocationInMarkerSpace.Z;
			NewCharacterLocation.Z = NewCharacterLocation.Z - 64.f;
			FVector CharacterLocationMovingAverage = this->GetCharacterLocationMovingAverage(NewCharacterLocation);
			this->SetActorLocation(CharacterLocationMovingAverage);
			DrawDebugSphere(GetWorld(), MarkerLocation, 0.5, 12, FColor::Red);

			//UNavigationSystem* const NavSys = GetWorld()->GetNavigationSystem();
			//NavSys->SimpleMoveToLocation(Controller, StartingCharacterLocation + MarkerLocationDelta);
			FVector CharacterLocationDelta = NewCharacterLocation - StartingCharacterLocation;
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================NewCharacterLocation: ") + NewCharacterLocation.ToCompactString());
			//GEngine->AddOnScreenDebugMessage(-1, 50.0f, FColor::Yellow, TEXT("===========================================CharacterLocationDelta: ") + CharacterLocationDelta.ToCompactString());
		}
	}
}

FVector AOculusARPOCCharacter::GetAdjustedMarkerTranslationMovingAverage(FVector LatestAdjustedMarkerTranslation) {
	uint8 numItems = 10;
	if (PreviousAdjustedMarkerTranslations.Num() >= numItems) {
		PreviousAdjustedMarkerTranslations.RemoveAt(0);
	}
	PreviousAdjustedMarkerTranslations.Add(&LatestAdjustedMarkerTranslation);
	FVector TotalVector = FVector::ZeroVector;
	for (int32 Index = 0; Index != PreviousAdjustedMarkerTranslations.Num(); ++Index)
	{
		TotalVector = TotalVector + *(PreviousAdjustedMarkerTranslations[Index]);
	}
	return TotalVector / PreviousAdjustedMarkerTranslations.Num();
}

FVector AOculusARPOCCharacter::GetCharacterLocationMovingAverage(FVector LatestCharacterLocation) {
	if (PreviousCharacterLocationIndex == -1)
	{
		PreviousCharacterLocations.Init(LatestCharacterLocation, 10);
		PreviousCharacterLocationIndex = 0;
	}
	if (PreviousCharacterLocationIndex == 10) {
		PreviousCharacterLocationIndex = 0;
	}
	PreviousCharacterLocations[PreviousCharacterLocationIndex] = LatestCharacterLocation;
	FVector TotalVector = FVector::ZeroVector;
	for (int32 Index = 0; Index != PreviousCharacterLocations.Num(); ++Index)
	{
		TotalVector = TotalVector + PreviousCharacterLocations[Index];
	}
	PreviousCharacterLocationIndex++;
	return TotalVector / PreviousCharacterLocations.Num();
}



