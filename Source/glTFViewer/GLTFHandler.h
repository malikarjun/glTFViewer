// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GLTFAsset.h"
#include "GLTFReader.h"

#include "GLTFHandler.generated.h"



namespace GLTF
{
	// math conversion

	inline FVector ConvertVec3(const FVector& Vec)
	{
		// glTF uses a right-handed coordinate system, with Y up.
		// Unreal uses a left-handed coordinate system, with Z up.
		return {Vec.X, Vec.Z, Vec.Y};
	}

	template<typename T>
	UE::Math::TQuat<T> ConvertQuat(const UE::Math::TQuat<T>& Quat)
	{
		// glTF uses a right-handed coordinate system, with Y up.
		// Unreal uses a left-handed coordinate system, with Z up.
		// Quat = (qX, qY, qZ, qW) = (sin(angle/2) * aX, sin(angle/2) * aY, sin(angle/2) * aZ, cons(angle/2))
		// where (aX, aY, aZ) - rotation axis, angle - rotation angle
		// Y swapped with Z between these coordinate systems
		// also, as handedness is changed rotation is inversed - hence negation
		// therefore QuatUE = (-qX, -qZ, -qY, qw)

		UE::Math::TQuat<T> Result(-Quat.X, -Quat.Z, -Quat.Y, Quat.W);
		// Not checking if quaternion is normalized
		// e.g. some sources use non-unit Quats for rotation tangents
		return Result;
	}

}  // namespace GLTF


/**
 * 
 */
UCLASS()
class GLTFVIEWER_API UGLTFHandler : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "glTF I/O")
	static UTextureCube* ReadFile(FString Filename, AActor* WorldActor, FVector HdriOffset);
	
	
	
	
};
