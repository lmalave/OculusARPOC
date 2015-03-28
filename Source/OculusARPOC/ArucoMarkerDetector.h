// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "opencv2/highgui/highgui.hpp"
#include "aruco/aruco.h"

/**
 * 
 */
class OCULUSARPOC_API ArucoMarkerDetector
{
    
public:
    
	ArucoMarkerDetector();
	~ArucoMarkerDetector();
    
    void ProcessMarkerDetection(cv::Mat Frame);
    
    cv::vector<aruco::Marker>* GetDetectedMarkers();
    
	bool IsDetected();

    aruco::Board* GetDetectedBoard();

    FVector GetDetectedBoardTranslation();

	FRotator GetDetectedBoardRotation();
	
	FVector GetDetectedMarkerTranslation(uint16 markerId);

	FRotator GetDetectedMarkerRotation(uint16 markerId);

	FVector GetDetectedSingleMarkerTranslation();

	FRotator GetDetectedSingleMarkerRotation();
	
	FVector GetDetectedTranslation();

	FRotator GetDetectedRotation();

	FVector GetVectorFromTVec(cv::Mat Tvec);
    
    FRotator GetBoardRotatorFromRVec(cv::Mat Rvec);
    
	FRotator GetMarkerRotatorFromRVec(cv::Mat Rvec);

	FVector GetPlaneMarkersNormalVector();

	FRotator GetPlaneMarkersRotation();
		
	FVector GetPlaneMarkersMidpoint();

	void Init();

    bool DetectMarkers;

	bool DetectBoard;

	int DetectSingleMarkerId; // if looking for single marker instead of board

	bool DetectPlaneMarkers;
	int PlaneMarker1Id;
	int PlaneMarker2Id;
	int PlaneMarker3Id;
	int PlaneMarker4Id;
    
	FVector PlaneMarker1Translation;
	FVector PlaneMarker2Translation;
	FVector PlaneMarker3Translation;
	FVector PlaneMarker4Translation;
	FRotator PlaneMarker1Rotation;
	FRotator PlaneMarker2Rotation;
	FRotator PlaneMarker3Rotation;
	FRotator PlaneMarker4Rotation;

	bool UseAveragePlaneMarkerRoll; 

protected:
   		
	aruco::CameraParameters CameraParams;
    aruco::MarkerDetector MarkerDetector;
    aruco::BoardConfiguration BoardConfig;
    aruco::BoardDetector BoardDetector;

    cv::vector<aruco::Marker> DetectedMarkers;
    
    aruco::Board DetectedBoard;

    bool MarkersAreDetected;
    
    bool Detected;

	float AveragePlaneMarkerRoll;
};
