// Fill out your copyright notice in the Description page of Project Settings.



#include "GLTFHandler.h"


#include "Components/RectLightComponent.h" 
#include "Components/PointLightComponent.h" 
#include "Engine/PointLight.h" 
#include "Engine/RectLight.h" 

#include "Kismet/KismetMathLibrary.h" 


#include "UObject/UObjectGlobals.h" 
#include "Misc/FileHelper.h" 



#include "IImageWrapperModule.h" 
#include "Modules/ModuleManager.h" 
#include "Formats/HdrImageWrapper.h" 

#include "IImageWrapper.h" 
#include "Engine/TextureCube.h" 
#include "Factories/Factory.h" 
#include "Misc/SecureHash.h" 
#include "EditorFramework/AssetImportData.h" 

#include "UObject/ObjectMacros.h" 

#include "Misc/Paths.h" 



const float SceneScale = 100.0f;
FVector HDRIOffset;

FRotator LookAt(FVector Forward, FVector WorldUp = FVector::UpVector) {
    FRotator Rot = UKismetMathLibrary::MakeRotFromXZ(Forward, WorldUp);
    return Rot;
}


FTransform GetNodeTransform(const GLTF::FAsset* GLTFAsset, int NodeIdx) {

    FVector Translation = GLTF::ConvertVec3(GLTFAsset->Nodes[NodeIdx].Transform.GetTranslation());
    FQuat Quat = GLTF::ConvertQuat(GLTFAsset->Nodes[NodeIdx].Transform.GetRotation());

    FMatrix Matrix = FMatrix::Identity;

    Matrix *= FQuatRotationMatrix(Quat);
    Matrix *= FTranslationMatrix(Translation);
    Matrix.ScaleTranslation(FVector(SceneScale, SceneScale, SceneScale));

    //  [0 1 0 0] [0 0 1 0] [-1 0 0 0] [0 0 0 1]
    FMatrix SceneBasis{FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(-1, 0, 0, 0), FPlane(0, 0, 0, 1)};

    return FTransform(SceneBasis.Inverse() * Matrix * SceneBasis);
}



FVector UEWorldCoordinates(FVector Coords) {
    FMatrix Matrix = FMatrix::Identity;

    Matrix *= FTranslationMatrix(Coords);
    Matrix.ScaleTranslation(FVector(SceneScale, SceneScale, SceneScale));

    //  [0 1 0 0] [0 0 1 0] [-1 0 0 0] [0 0 0 1]
    FMatrix SceneBasis{FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(-1, 0, 0, 0), FPlane(0, 0, 0, 1)};

    FTransform Transform = FTransform(SceneBasis.Inverse() * Matrix * SceneBasis);

    return Transform.GetTranslation();
}


TPair<int, int> GetAreaLightDimIdxs(int DirIdx) {
    TPair<int, int> DimIdxs;
    if (DirIdx == 0) {
        // width index 
        DimIdxs.Key = 1;
        // height index
        DimIdxs.Value = 2;
    } else if (DirIdx == 1) {
        DimIdxs.Key = 0;
        DimIdxs.Value = 2;
    } else if (DirIdx == 2){
        DimIdxs.Key = 1;
        DimIdxs.Value = 0;
    } else {
        UE_LOG(LogTemp, Error, TEXT("Unsupported DirIdx %d"), DirIdx)
    }
    return DimIdxs;
}

void SpawnAreaLight(UWorld* World, GLTF::FNode Node, GLTF::FLight Light) {

    
    for(int i = 0; i < 3; i++) {
        FVector Dir = FVector(0, 0, 0);
        for(int j = 0; j < 2; j++) {
            Dir[i] = (j == 0) ? -1 : 1;

            // Area light dimensions
            FVector Coords = GLTF::ConvertVec3(Node.Transform.GetTranslation());
            FVector Scale = GLTF::ConvertVec3(Node.Transform.GetScale3D());

            FVector WorldCoords = UEWorldCoordinates(Coords);
            FVector BBRadius = UEWorldCoordinates(Scale).GetAbs();
            
            FRotator Rot = LookAt(Dir);
            FVector Trans = WorldCoords + Dir * BBRadius;

            ARectLight* RectLight = World->SpawnActor<ARectLight>(ARectLight::StaticClass());

            URectLightComponent* Root = Cast<URectLightComponent>(RectLight->GetComponentByClass(URectLightComponent::StaticClass()));
            Root->DestroyComponent();
    

            URectLightComponent* RLC = NewObject<URectLightComponent>(RectLight, URectLightComponent::StaticClass());
            TPair<int, int> DimIdxs = GetAreaLightDimIdxs(i);
            RLC->SetSourceWidth(2*BBRadius[DimIdxs.Key]);
            RLC->SetSourceHeight(2*BBRadius[DimIdxs.Value]);
            // RLC->SetIntensity(Light.Intensity);

            // RLC->SetMobility(EComponentMobility::Stationary);
            FLinearColor Color = FLinearColor(Light.Color/255.0f);
            RLC->SetLightColor(Color, false);
            RLC->OnComponentCreated();
            RLC->RegisterComponent();
            RectLight->SetRootComponent(RLC);
 
            RectLight->AddActorLocalRotation(Rot);
            RectLight->AddActorWorldOffset(Trans + HDRIOffset);

        }
    }

}



UTextureCube* CreateTextureCube( UObject* InParent, FName Name, EObjectFlags Flags )
{
	return Cast<UTextureCube>(NewObject<UObject>(InParent, UTextureCube::StaticClass(), Name, Flags));	
}

UTextureCube* importHDRFile(FString Filename) {
    FString PackageName = "/Game/198L";
    FString Name = "198L";
    UPackage* Pkg = CreatePackage( *PackageName);

    EObjectFlags Flags =  RF_Public | RF_Standalone | RF_Transactional;



    TArray<uint8> Data;
    if (!FFileHelper::LoadFileToArray(Data, *Filename))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file '%s' to array"), *Filename);
    }
    Data.Add(0);
    const uint8* Buffer = &Data[0];
    const uint8* BufferEnd = Buffer + Data.Num() - 1;
    const int32 Length = BufferEnd - Buffer;
    UTextureCube* TextureCube = nullptr; 

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	if (ImageWrapperModule.DetectImageFormat(Buffer, Length) == EImageFormat::HDR)
	{
		FHdrImageWrapper HdrImageWrapper;
		if (HdrImageWrapper.SetCompressedFromView(TArrayView64<const uint8>(Buffer, Length)))
		{
			TArray64<uint8> UnCompressedData;
			if (HdrImageWrapper.GetRaw(ERGBFormat::BGRE, 8, UnCompressedData))
			{
				// create the cube texture
				TextureCube = CreateTextureCube(Pkg, FName(*Name), Flags);
				if ( TextureCube )
				{
					TextureCube->Source.Init(
						HdrImageWrapper.GetWidth(),
						HdrImageWrapper.GetHeight(),
						/*NumSlices=*/ 1,
						/*NumMips=*/ 1,
						TSF_BGRE8,
						UnCompressedData.GetData()
						);
					// the loader can suggest a compression setting
					TextureCube->CompressionSettings = TC_HDR;
					TextureCube->SRGB = false;
                }
                FMD5Hash FileHash = FMD5Hash::HashFile(*Filename);
                TextureCube->AssetImportData->Update(Filename, FileHash.IsValid() ? &FileHash : nullptr);

                TextureCube->PostEditChange();
            }
        }
    }
    return TextureCube;
}


// TODO @mswamy : invoke the blueprint functions to change envmap for HDRI backdrop in C++
UTextureCube* SpawnEnvMap(GLTF::FAsset* GLTFAsset, FString DirPath) {
    UTextureCube* TextureCube = nullptr;
    for(int LightIdx = 0; LightIdx < GLTFAsset->Lights.Num(); LightIdx++) {
        GLTF::FLight Light = GLTFAsset->Lights[LightIdx];

        if (Light.Type == GLTF::FLight::EType::EnvMap) {
            TextureCube = importHDRFile(DirPath + "/" + Light.Hdri);
        }
    }
    return TextureCube;
}



UTextureCube* UGLTFHandler::ReadFile(FString Filename, AActor* WorldActor, FVector HdriOffset) {

    // UTextureCube* TextureCube = importHDRFile("/home/mswamy/Downloads/198L.hdr");

    HDRIOffset = HdriOffset;
    GLTF::FAsset* GLTFAsset = new GLTF::FAsset();
    GLTF::FFileReader* GLTFReader = new GLTF::FFileReader();

    GLTFReader->ReadFile(Filename, true, true, *GLTFAsset);

    for(int NodeIdx = 0; NodeIdx < GLTFAsset->Nodes.Num(); NodeIdx++) {
        GLTF::FNode Node = GLTFAsset->Nodes[NodeIdx];
        
        // ignore non-light nodes
        if (Node.LightIndex < 0) continue;

        UE_LOG(LogTemp, Warning, TEXT("LightIdx is %d"), Node.LightIndex)   


        GLTF::FLight Light = GLTFAsset->Lights[Node.LightIndex];

        if (Light.Type == GLTF::FLight::EType::RectArea) {
            SpawnAreaLight(WorldActor->GetWorld(), Node, Light);
        }

    }

    
    return SpawnEnvMap(GLTFAsset, FPaths::GetPath(Filename));

}





