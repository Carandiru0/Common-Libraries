#pragma once
#ifndef _H_GLTFLOADER_
#define _H_GLTFLOADER_

#include "cgltf.h"
#include "Pose.h"
#include "Skeleton.h"
#include "Mesh.h"
#include "Clip.h"
#include "Image.h"
#include <vector>
#include <string>

cgltf_data const* LoadGLTFFile(char const* const path);
void FreeGLTFFile(cgltf_data const* handle);

Pose LoadRestPose(cgltf_data const* data);
std::vector<std::string> LoadJointNames(cgltf_data const* data);
std::vector<Clip> LoadAnimationClips(cgltf_data const* data);
Pose LoadBindPose(cgltf_data const* data);
Skeleton LoadSkeleton(cgltf_data const* data);
std::vector<Mesh> LoadMeshes(cgltf_data const* data);
std::vector<Image> LoadImages(cgltf_data const* data, std::string_view const model_uri);
std::vector<Texture> LoadTextures(cgltf_data const* data);
std::vector<Material> LoadMaterials(cgltf_data const* data);

// internal
std::string const get_relative_file_path(std::string_view const base_path, std::string_view const new_path);

#endif
