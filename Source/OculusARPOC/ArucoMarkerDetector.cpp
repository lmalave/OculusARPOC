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
	DetectSingleMarkerId = -1;
    DetectBoard = false;
    MarkersAreDetected = false;
	Detected = false;
	PlaneMarker1Id = -1;
	PlaneMarker2Id = -1;
	PlaneMarker3Id = -1;
	PlaneMarker4Id = -1;
	PlaneMarker1Translation = FVector::ZeroVector;
	PlaneMarker2Translation = FVector::ZeroVector;
	PlaneMarker3Translation = FVector::ZeroVector;
	PlaneMarker4Translation = FVector::ZeroVector;
	UseAveragePlaneMarkerRoll = false;
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

bool ArucoMarkerDetector::IsDetected() {
	return Detected;
}

void ArucoMarkerDetector::Init() {
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("About to read YAML file"));
    CameraParams.readFromXMLFile("D:/Projects/OculusARPOC/Config/camera.yml");
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Read YAML file!!"));
    CameraParams.resize(cv::Size(1280, 720));
    BoardConfig.readFromFile("D:/Projects/OculusARPOC/Config/board_meters.yml");
	
}

void ArucoMarkerDetector::ProcessMarkerDetection(cv::Mat Frame) {
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("In ProcessMarkerDetection"));
	Detected = false;
    if (DetectMarkers) {
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("DetectMarkers is true"));
		MarkerDetector.detect(Frame, this->DetectedMarkers); // don't calculate extrinsics - should be done based on marker id
		uint16 numPlaneMarkersDetected = 0;
		AveragePlaneMarkerRoll = 0.f;
		for (uint16 i = 0; i < this->DetectedMarkers.size(); i++)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Found Marker: ") + FString::FromInt(this->DetectedMarkers[i].id));
			float markerSize = 0.034;
			int markerId = this->DetectedMarkers[i].id;
			if (markerId == 698 || markerId == 683 || markerId == 795 || markerId == 819)  {
				markerSize = 0.1056;
			}
			this->DetectedMarkers[i].calculateExtrinsics(markerSize, CameraParams);
			if (this->DetectedMarkers[i].id == DetectSingleMarkerId) {
				Detected = true;
				aruco::CvDrawingUtils::draw3dAxis(Frame, this->DetectedMarkers[i], CameraParams);
			}
			if (this->DetectPlaneMarkers) {
				if (this->DetectedMarkers[i].id == PlaneMarker1Id) {
					aruco::CvDrawingUtils::draw3dAxis(Frame, this->DetectedMarkers[i], CameraParams);
					PlaneMarker1Translation = GetVectorFromTVec(this->DetectedMarkers[i].Tvec);
					//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("PlaneMarker1Translation: ") + PlaneMarker1Translation.ToCompactString());
					PlaneMarker1Rotation = GetMarkerRotatorFromRVec(this->DetectedMarkers[i].Rvec);
					AveragePlaneMarkerRoll += PlaneMarker1Rotation.Roll;
					numPlaneMarkersDetected++;
				}
				else if (this->DetectedMarkers[i].id == PlaneMarker2Id) {
					aruco::CvDrawingUtils::draw3dAxis(Frame, this->DetectedMarkers[i], CameraParams);
					PlaneMarker2Translation = GetVectorFromTVec(this->DetectedMarkers[i].Tvec);
					//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("PlaneMarker2Translation: ") + PlaneMarker2Translation.ToCompactString());
					PlaneMarker2Rotation = GetMarkerRotatorFromRVec(this->DetectedMarkers[i].Rvec);
					AveragePlaneMarkerRoll += PlaneMarker2Rotation.Roll;
					numPlaneMarkersDetected++;
				}
				else if (this->DetectedMarkers[i].id == PlaneMarker3Id) {
					aruco::CvDrawingUtils::draw3dAxis(Frame, this->DetectedMarkers[i], CameraParams);
					PlaneMarker3Translation = GetVectorFromTVec(this->DetectedMarkers[i].Tvec);
					//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("PlaneMarker3Translation: ") + PlaneMarker3Translation.ToCompactString());
					PlaneMarker3Rotation = GetMarkerRotatorFromRVec(this->DetectedMarkers[i].Rvec);
					AveragePlaneMarkerRoll += PlaneMarker3Rotation.Roll;
					numPlaneMarkersDetected++;
				}
				else if (this->DetectedMarkers[i].id == PlaneMarker4Id) {
					aruco::CvDrawingUtils::draw3dAxis(Frame, this->DetectedMarkers[i], CameraParams);
					PlaneMarker4Translation = GetVectorFromTVec(this->DetectedMarkers[i].Tvec);
					//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("PlaneMarker4Translation: ") + PlaneMarker4Translation.ToCompactString());
					PlaneMarker4Rotation = GetMarkerRotatorFromRVec(this->DetectedMarkers[i].Rvec);
					AveragePlaneMarkerRoll += PlaneMarker4Rotation.Roll;
					numPlaneMarkersDetected++;
				}
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("numPlaneMarkersDetected: ") + FString::FromInt(numPlaneMarkersDetected));
			}
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Found Marker!!"));
			this->DetectedMarkers[i].draw(Frame, cv::Scalar(0, 0, 255), 2);
			//aruco::CvDrawingUtils::draw3dAxis(Frame,this->DetectedMarkers[i],CameraParams);
			// TODO: put these calculated values into a hashmap
			FVector TranslationVector(this->DetectedMarkers[i].Tvec.at<float>(0, 0), this->DetectedMarkers[i].Tvec.at<float>(1, 0), this->DetectedMarkers[i].Tvec.at<float>(2, 0));
			FVector RotationVector(this->DetectedMarkers[i].Rvec.at<float>(0, 0), this->DetectedMarkers[i].Rvec.at<float>(1, 0), this->DetectedMarkers[i].Rvec.at<float>(2, 0));
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("TranslationVector: ") + TranslationVector.ToString());
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("RotationVector: ") + RotationVector.ToString());
			//aruco::CvDrawingUtils::draw3dCube(Frame, this->DetectedMarkers[i], CameraParams); 
		}
		AveragePlaneMarkerRoll = AveragePlaneMarkerRoll / numPlaneMarkersDetected;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("AveragePlaneMarkerRoll: ") + FString::SanitizeFloat(AveragePlaneMarkerRoll));
		if (numPlaneMarkersDetected >= 3) {
			Detected = true;
		}
		if (DetectBoard) {
            float probDetect= BoardDetector.detect( this->DetectedMarkers, BoardConfig,this->DetectedBoard, CameraParams,0.034);
            if (probDetect > 0.f) {
				Detected = true;
				aruco::CvDrawingUtils::draw3dAxis(Frame,this->DetectedBoard,CameraParams);
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
        return GetBoardRotatorFromRVec(DetectedBoard.Rvec);
    }
    return FRotator::ZeroRotator;
}

FVector ArucoMarkerDetector::GetDetectedMarkerTranslation(uint16 markerId) {  // TODO: fix horrible implementation
	for (uint16 i = 0; i < this->DetectedMarkers.size(); i++)
	{
		if (this->DetectedMarkers[i].id == markerId) {
			return GetVectorFromTVec(this->DetectedMarkers[i].Tvec);
		}
	}
	return FVector::ZeroVector;
}

FRotator ArucoMarkerDetector::GetDetectedMarkerRotation(uint16 markerId) {
	for (uint16 i = 0; i < this->DetectedMarkers.size(); i++)
	{
		if (this->DetectedMarkers[i].id == markerId) {
			return GetMarkerRotatorFromRVec(this->DetectedMarkers[i].Rvec);
		}
	}
	return FRotator::ZeroRotator;
}

FVector ArucoMarkerDetector::GetDetectedSingleMarkerTranslation() {
	return GetDetectedMarkerTranslation(DetectSingleMarkerId);
}

FRotator ArucoMarkerDetector::GetDetectedSingleMarkerRotation() {
	return GetDetectedMarkerRotation(DetectSingleMarkerId);
}

FVector ArucoMarkerDetector::GetDetectedTranslation() {
	if (DetectBoard) {
		return GetDetectedBoardTranslation();
	}
	else if (DetectPlaneMarkers) {
		return GetPlaneMarkersMidpoint();
	}
	else if (DetectSingleMarkerId >= 0) {
		return GetDetectedMarkerTranslation(DetectSingleMarkerId);
	}
	return FVector::ZeroVector;
}

FRotator ArucoMarkerDetector::GetDetectedRotation() {
	if (DetectBoard) {
		return GetDetectedBoardRotation();
	}
	else if (DetectPlaneMarkers) {
		return GetPlaneMarkersRotation();
	}
	else if (DetectSingleMarkerId >= 0) {
		return GetDetectedMarkerRotation(DetectSingleMarkerId);
	}
	return FRotator::ZeroRotator;
}


FVector ArucoMarkerDetector::GetVectorFromTVec(cv::Mat Tvec) {
    FVector TranslationVector(Tvec.at<float>(2,0), -Tvec.at<float>(0,0), Tvec.at<float>(1,0)); // change it to be like character coordinates:  forward is x, right is y, up is z
    return TranslationVector * 100; // Aruco is in meters and Unreal in centimeters
}

FRotator ArucoMarkerDetector::GetBoardRotatorFromRVec(cv::Mat Rvec) {
    // Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
    // Board:  z = forward, x = right, y = up, Pitch = rotation on X axis, Yaw = rotation on Y axis, Roll = Rotation on Z axis
    FRotator Rotation(Rvec.at<float>(0,0) * (180 / PI), Rvec.at<float>(1,0) * (180 / PI), Rvec.at<float>(2,0) * (180 / PI));
    return Rotation;
}

FRotator ArucoMarkerDetector::GetMarkerRotatorFromRVec(cv::Mat Rvec) {
	// Character:  x = forward, y = right, z = up, Pitch = rotation on Y axis, Yaw = rotation on Z axis, Roll = Rotation on X axis
	// Single Marker: y = forward, z = right, x = up.  Pitch = rotation on Z axis, Yaw = Rotation on X axis, Roll = Rotation on Y axis 
	FRotator Rotation(Rvec.at<float>(2, 0) * (180 / PI), Rvec.at<float>(0, 0) * (180 / PI), Rvec.at<float>(1, 0) * (180 / PI));
	return Rotation;
}

FVector ArucoMarkerDetector::GetPlaneMarkersNormalVector() {
	FVector Vector12 = PlaneMarker2Translation - PlaneMarker1Translation;
	FVector Vector13 = PlaneMarker3Translation - PlaneMarker1Translation;
	FVector NormalVector = FVector::CrossProduct(Vector13, Vector12);
	NormalVector.Normalize();
	return NormalVector;
}

FRotator ArucoMarkerDetector::GetPlaneMarkersRotation() {
	FVector Vector12 = PlaneMarker2Translation - PlaneMarker1Translation;
	FVector Vector13 = PlaneMarker3Translation - PlaneMarker1Translation;
	FVector NormalVector = FVector::CrossProduct(Vector12, Vector13);
	FRotator Rotation = NormalVector.Rotation(); // not sure if this is equivalent to the rotation calculated from Rvec;
	if (UseAveragePlaneMarkerRoll) {
		Rotation.Roll = AveragePlaneMarkerRoll;
	}
	return Rotation;
}

FVector ArucoMarkerDetector::GetPlaneMarkersMidpoint() {
	FVector Vector23 = PlaneMarker3Translation - PlaneMarker2Translation;
	return PlaneMarker2Translation + (Vector23 * 0.5f);
}