// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OculusARPOC : ModuleRules
{
	public OculusARPOC(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "RHI", "RenderCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "CoherentUIPlugin", "CoherentUI" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.AddRange(new string[] { "D:/LeapSDK/lib/x64/Leap.lib", 
                //"D:/aruco1.2.5-win_32bits_msvc10/lib/aruco125.lib",
                "D:/opencv/build/x64/vc12/lib/opencv_core2411.lib", 
                "D:/opencv/build/x64/vc12/lib/opencv_highgui2411.lib", 
                "D:/opencv/build/x64/vc12/lib/opencv_imgproc2411.lib", 
                "D:/opencv/build/x64/vc12/lib/opencv_calib3d2411.lib"});
            PublicIncludePaths.AddRange(new string[] { "D:/LeapSDK/include", "D:/opencv/build/include" });
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            //PublicFrameworks.Add("/Library/Frameworks/OvrvisionSDK.framework");
            PublicAdditionalLibraries.AddRange(new string[] { "/Users/lmalave/Documents/LeapSDK/lib/libLeap.dylib", "/Library/Frameworks/OvrvisionSDK.framework/Versions/A/OvrvisionSDK" });
            PublicIncludePaths.AddRange(new string[] { "/Users/lmalave/Documents/LeapSDK/include", "/Library/Frameworks/OvrvisionSDK.framework/Versions/A/Headers" });
            PrivateIncludePaths.Add("/usr/local/include");
            PublicAdditionalLibraries.Add("/usr/local/lib/libopencv_core.2.4.10.dylib"); //these are all the Mac version of opencv
            PublicAdditionalLibraries.Add("/usr/local/lib/libopencv_highgui.2.4.10.dylib");
            PublicAdditionalLibraries.Add("/usr/local/lib/libopencv_imgproc.2.4.10.dylib");
            //PublicAdditionalLibraries.Add("/usr/local/lib/libopencv_imgcodecs.2.4.10.dylib");
            PublicAdditionalLibraries.Add("/usr/local/lib/libaruco.1.2.5.dylib");
        }
    }
}
