// Fill out your copyright notice in the Description page of Project Settings.

#include "OculusARPOC.h"
#include "Engine.h"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "aruco/aruco.h"
#include "ArucoMarkerDetector.h"

ArucoMarkerDetector::ArucoMarkerDetector()
{
    DetectMarkers = false;
    DetectBoard = false;
    MarkersAreDetected = false;
    BoardIsDetected = false;
}

ArucoMarkerDetector::~ArucoMarkerDetector()
{
}

cv::vector<aruco::Marker>* ArucoMarkerDetector::GetDetectedMarkers() {
    return &DetectedMarkers;
}

aruco::Board* ArucoMarkerDetector::GetDetectedBoard() {
    return &DetectedBoard;
}

bool ArucoMarkerDetector::IsBoardDetected() {
    return BoardIsDetected;
}

void ArucoMarkerDetector::Init() {
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("About to read YAML file"));
    CameraParams.readFromXMLFile("D:/Projects/OculusARPOC/Config/camera_3.yml");
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Read YAML file!!"));
    CameraParams.resize(cv::Size(1280, 720));
    BoardConfig.readFromFile("D:/Projects/OculusARPOC/Config/board_meters.yml");
	
}

void ArucoMarkerDetector::ProcessMarkerDetection(cv::Mat Frame) {
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("In ProcessMarkerDetection"));
    BoardIsDetected = false;
    if (DetectMarkers) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("DetectMarkers is true"));
		MarkerDetector.detect(Frame, this->DetectedMarkers); // don't calculate extrinsics - should be done based on marker id
		for (uint16 i = 0; i < this->DetectedMarkers.size(); i++)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Found Marker!!"));
			this->DetectedMarkers[i].draw(Frame, cv::Scalar(0, 0, 255), 2);
			float markerSize = 0.034;
			if (this->DetectedMarkers[i].id < 10)  {
				markerSize = 0.13;
			}
			this->DetectedMarkers[i].calculateExtrinsics(markerSize, CameraParams);
			//aruco::CvDrawingUtils::draw3dAxis(Frame,this->DetectedMarkers[i],CameraParams);
			FVector TranslationVector(this->DetectedMarkers[i].Tvec.at<float>(0, 0), this->DetectedMarkers[i].Tvec.at<float>(1, 0), this->DetectedMarkers[i].Tvec.at<float>(2, 0));
			FVector RotationVector(this->DetectedMarkers[i].Rvec.at<float>(0, 0), this->DetectedMarkers[i].Rvec.at<float>(1, 0), this->DetectedMarkers[i].Rvec.at<float>(2, 0));
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("TranslationVector: ") + TranslationVector.ToString());
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("RotationVector: ") + RotationVector.ToString());
			//aruco::CvDrawingUtils::draw3dCube(Frame, this->DetectedMarkers[i], CameraParams); 
		}
		if (DetectBoard) {
            float probDetect= BoardDetector.detect( this->DetectedMarkers, BoardConfig,this->DetectedBoard, CameraParams,0.034);
            if (probDetect > 0.f) {
                BoardIsDetected = true;
               // aruco::CvDrawingUtils::draw3dAxis(Frame,this->DetectedBoard,CameraParams);
                FVector TranslationVector(this->DetectedBoard.Tvec.at<float>(0,0), this->DetectedBoard.Tvec.at<float>(1,0), this->DetectedBoard.Tvec.at<float>(2,0));
                FVector RotationVector(this->DetectedBoard.Rvec.at<float>(0,0), this->DetectedBoard.Rvec.at<float>(1,0), this->DetectedBoard.Rvec.at<float>(2,0));
                //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("TranslationVector: ") + TranslationVector.ToString());
                //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("RotationVector: ") + RotationVector.ToString());
				
				}
        } 
		
    }
    //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Num Markers found: ") + FString::FromInt(Markers.size()));
    // end aruco speed test
	
}

FVector ArucoMarkerDetector::GetDetectedBoardTranslation() {
    if (&DetectedBoard != NULL) {
        return GetVectorFromTVec(DetectedBoard.Tvec);
    }
    return FVector::ZeroVector;
}

FRotator ArucoMarkerDetector::GetDetectedBoardRotation() {
    if (&DetectedBoard != NULL) {
        return GetRotatorFromRVec(DetectedBoard.Rvec);
    }
    return FRotator::ZeroRotator;
}

FVector ArucoMarkerDetector::GetVectorFromTVec(cv::Mat Tvec) {
    FVector TranslationVector(Tvec.at<float>(2,0), -Tvec.at<float>(0,0), Tvec.at<float>(1,0)); // change it to be like character coordinates:  forward is x, right is y, up is z
    return TranslationVector * 100; // Aruco is in meters and Unreal in centimeters
}

FRotator ArucoMarkerDetector::GetRotatorFromRVec(cv::Mat Rvec) {
    // Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
    // Board:  z = forward, x = right, y = up, Pitch = rotation on X axis, Yaw = rotation on Y axis, Roll = Rotation on Z axis
    FRotator Rotation(Rvec.at<float>(0,0) * (180 / PI), Rvec.at<float>(1,0) * (180 / PI), Rvec.at<float>(2,0) * (180 / PI));
    return Rotation;
}

