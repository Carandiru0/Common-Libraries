#pragma once
#ifndef GLTF_LIB
#define GLTF_LIB

#include <string_view>
#include <filesystem>
#include <vector>

// dependent includes
#include "Pose.h"
#include "Clip.h"
#include "Skeleton.h"
#include "Mesh.h"
#include "Image.h"

#define GLTF_FILE_EXT L".gltf"

// public structures
struct AnimationInstance {
	Pose mAnimatedPose;
	std::vector <mat4> mPosePalette;
	unsigned int mClip;
	float mPlayback;

	inline AnimationInstance() : mClip(0), mPlayback(0.0f) { }
};

typedef struct gltf {

	std::vector<Image>     mImages;
	std::vector<Texture>   mTextures;
	std::vector<Material>  mMaterials;
	std::vector<Mesh>      mMeshes;
	std::vector<Clip>      mClips;

	Skeleton             mSkeleton;
	AnimationInstance    mAnimInfo;

	std::string          uri;
} gltf;


// public functions / interface
int const LoadGLTF(std::filesystem::path const path, struct gltf&& __restrict model);



#endif // GLTF_LIB

