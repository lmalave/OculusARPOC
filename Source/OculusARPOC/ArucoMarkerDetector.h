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
    
    bool IsBoardDetected();

    aruco::Board* GetDetectedBoard();

    FVector GetDetectedBoardTranslation();

    FRotator GetDetectedBoardRotation();

    FVector GetVectorFromTVec(cv::Mat Tvec);
    
    FRotator GetRotatorFromRVec(cv::Mat Rvec);
    
    void Init();

    bool DetectMarkers;
    
    bool DetectBoard;
    
protected:
    
    aruco::CameraParameters CameraParams;
    aruco::MarkerDetector MarkerDetector;
    aruco::BoardConfiguration BoardConfig;
    aruco::BoardDetector BoardDetector;

    cv::vector<aruco::Marker> DetectedMarkers;
    
    aruco::Board DetectedBoard;

    bool MarkersAreDetected;
    
    bool BoardIsDetected;
};
