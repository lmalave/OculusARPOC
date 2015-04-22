// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/Character.h"
#include "ArucoMarkerDetector.h"
#include "Leap.h"
#include "LeapInputReader.h"
#include "UISurfaceRaytraceInputHandler.h"
#include "VideoDisplaySurface.h"
#include "OculusARPOCCharacter.generated.h"

class UInputComponent;

UCLASS(config=Game)
class AOculusARPOCCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	class USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FirstPersonCameraComponent;

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	class UChildActorComponent* BackgroundVideoSurface;

	UClass* UISurfaceBlueprintClass;

	UClass* VideoSurfaceBlueprintClass;

	UClass* MarkerPlaneBlueprintClass;

	UClass* PropMeshBlueprintClass;

	UClass* PortalBlueprintClass;

	UClass* ChairBlueprintClass;

	FDateTime LastMarkerSpawnTime;

	IVideoSource* VideoSource;

	ArucoMarkerDetector* MarkerDetector;

	LeapInputReader* LeapInput;

	UISurfaceRaytraceInputHandler* UISurfaceRaytraceHandler;

	AActor* BoardFollowActor;


public:
	AOculusARPOCCharacter(const FObjectInitializer& ObjectInitializer);

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector GunOffset;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class AOculusARPOCProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	class USoundBase* FireSound;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	class UAnimMontage* FireAnimation;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		bool LeapEnable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		bool LeapDrawSimpleHands;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		FString ImageSource;  // TODO: should be enum

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Raytrace)
		bool RaytraceInputEnable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Raytrace)
		float RaytraceForwardOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Raytrace)
		float RaytraceUpOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Raytrace)
		float RaytraceDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Raytrace)
		float RaytraceDrawFraction;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
	class AUISurfaceActor* SelectedUISurfaceActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
	class AActor* SpawnedPropActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
		bool IsInWindowMoveMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		float LeapToUnrealScalingFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		FVector LeapHandOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Leap)
		bool LeapAttachedToHead;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
		FVector ActionHandPalmLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
		FVector ActionHandFingerLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
		FVector ActionHandPreviousPalmLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CoherentUI)
		FVector ActionHandPreviousFingerLocation;

	Leap::Controller* LeapController;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Aruco)
		bool SpawnActorAtMarker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Aruco)
		bool SpawnedActorFacesCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Aruco)
		bool SpawnedActorFollowsMarkerLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Aruco)
		bool SpawnedActorFollowsMarkerRotation;

public:

	virtual FRotator GetViewRotation() const override;

	UFUNCTION(BlueprintCallable, Category = Camera)
		void ResetHMD();

	UFUNCTION(BlueprintCallable, Category = CoherentUI)
		void ToggleWindowMoveMode();

	UFUNCTION(BlueprintCallable, Category = CoherentUI)
		void SpawnUIWindowAtCameraDirectionWithParams(FString URL, float WindowScaleX, float WindowScaleY, float PixelWidth, float DistanceInUnrealUnits);

	UFUNCTION(BlueprintCallable, Category = Aruco)
		AActor* SpawnActor(UClass* BlueprintClass, FVector Location, FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category = CoherentUI)
		void HandleZoomInWindow();

	UFUNCTION(BlueprintCallable, Category = CoherentUI)
		void HandleZoomOutWindow();

	UFUNCTION(BlueprintCallable, Category = Aruco)
		void StartAR();

protected:
	
	/** Fires a projectile. */
	void OnFire();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	struct TouchData
	{
		TouchData() { bIsPressed = false;Location=FVector::ZeroVector;}
		bool bIsPressed;
		ETouchIndex::Type FingerIndex;
		FVector Location;
		bool bMoved;
	};
	void BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location);
	void EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location);
	void TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location);
	TouchData	TouchItem;
	
protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

	/* 
	 * Configures input for touchscreen devices if there is a valid touch interface for doing so 
	 *
	 * @param	InputComponent	The input component pointer to bind controls to
	 * @returns true if touch controls were enabled.
	 */
	bool EnableTouchscreenMovement(UInputComponent* InputComponent);


	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	void HandleLeap();

	void HandleMoveWindow();

	void HandleMarkerActor();

	void HandleMarkerCharacterMovement();
		
	FVector LeapPositionToUnrealLocation(Leap::Vector LeapPosition, FVector UnrealOffset);

	FVector GetWorldLocationFromMarkerTranslation(FVector MarkerTranslation);

	FVector GetWorldMarkerNormalVector(FVector MarkerNormalVector);
		
	FVector GetAdjustedMarkerTranslationMovingAverage(FVector LatestAdjustedMarkerTranslation);

	FVector GetCharacterLocationMovingAverage(FVector LatestCharacterLocation);

	bool BoardWindowIsSpawned;

	bool ARStarted;

	FVector StartingCharacterLocation;
	FVector StartingCameraLocation;
	FVector StartingCameraForwardVector;
	FVector StartingMarkerTranslation;
	FRotator StartingMarkerRotation;
	FVector StartingMarkerLocation;
	float StartingMarkerDistance;
	FVector StartingMarkerNormalVector;
	FTransform StartingMarkerTransform;

	TArray<FVector*> PreviousAdjustedMarkerTranslations; // used to calculate running average

	TArray<FVector> PreviousCharacterLocations; // used to calculate moving average
	int PreviousCharacterLocationIndex;

public:
	/** Returns Mesh1P subobject **/
	FORCEINLINE class USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	/** Returns BackgroundVideoSurface subobject **/
	FORCEINLINE class UChildActorComponent* GetBackgroundVideoSurface() const { return BackgroundVideoSurface; }

};

