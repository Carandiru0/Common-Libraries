// gltf.cpp : Defines the functions for the static library.
//
#include "gltf.h"

#include "GLTFLoader.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

int const LoadGLTF(std::filesystem::path const path, struct gltf&& __restrict model)
{
	cgltf_data const* gltf_data = LoadGLTFFile(path.string().c_str());
	
	if (gltf_data) {
		model.uri = path.string(); // save path to gltf, used for relative path for images

		// order matters
		model.mImages = LoadImages(gltf_data, model.uri);
		model.mTextures = LoadTextures(gltf_data);
		model.mMaterials = LoadMaterials(gltf_data);
		model.mMeshes = LoadMeshes(gltf_data);
		model.mSkeleton = LoadSkeleton(gltf_data);
		model.mClips = LoadAnimationClips(gltf_data);

		FreeGLTFFile(gltf_data);

		return(1);
	}

	return(0);
}