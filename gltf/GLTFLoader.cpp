#pragma warning(disable : 26812)

#define GLTF_IMPLEMENTATION
#include "GLTFLoader.h"
#include <iostream>
#include "Transform.h"
#include <algorithm>

namespace GLTFHelpers {
	Transform GetLocalTransform(cgltf_node& node) {
		Transform result;

		if (node.has_matrix) {
			mat4 mat(&node.matrix[0]);
			result = mat4ToTransform(mat);
		}

		if (node.has_translation) {
			result.position = vec3(node.translation[0], node.translation[1], node.translation[2]);
		}

		if (node.has_rotation) {
			result.rotation = quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
		}

		if (node.has_scale) {
			result.scale = vec3(node.scale[0], node.scale[1], node.scale[2]);
		}

		return result;
	}

	int GetNodeIndex(cgltf_node* target, cgltf_node* allNodes, unsigned int numNodes) {
		if (target == 0) {
			return(-1);
		}
		for (unsigned int i = 0; i < numNodes; ++i) {
			if (target == &allNodes[i]) {
				return((int)i);
			}
		}
		return(-1);
	}

	void GetScalarValues(std::vector<float>& outScalars, unsigned int inComponentCount, const cgltf_accessor& inAccessor) {
		outScalars.resize(inAccessor.count * inComponentCount);
		for (cgltf_size i = 0; i < inAccessor.count; ++i) {
			cgltf_accessor_read_float(&inAccessor, i, &outScalars[i * inComponentCount], inComponentCount);
		}
	}

	template<typename T, int N>
	void TrackFromChannel(Track<T, N>& inOutTrack, const cgltf_animation_channel& inChannel) {
		cgltf_animation_sampler& sampler = *inChannel.sampler;

		Interpolation interpolation = Interpolation::Constant;
		if (inChannel.sampler->interpolation == cgltf_interpolation_type_linear) {
			interpolation = Interpolation::Linear;
		}
		else if (inChannel.sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
			interpolation = Interpolation::Cubic;
		}
		bool isSamplerCubic = interpolation == Interpolation::Cubic;
		inOutTrack.SetInterpolation(interpolation);

		std::vector<float> timelineFloats;
		GetScalarValues(timelineFloats, 1, *sampler.input);

		std::vector<float> valueFloats;
		GetScalarValues(valueFloats, N, *sampler.output);

		unsigned int numFrames = (unsigned int)sampler.input->count;
		unsigned int numberOfValuesPerFrame = valueFloats.size() / timelineFloats.size();
		inOutTrack.Resize(numFrames);
		for (unsigned int i = 0; i < numFrames; ++i) {
			int baseIndex = i * numberOfValuesPerFrame;
			Frame<N>& frame = inOutTrack[i];
			int offset = 0;

			frame.mTime = timelineFloats[i];

			for (int component = 0; component < N; ++component) {
				frame.mIn[component] = isSamplerCubic ? valueFloats[baseIndex + offset++] : 0.0f;
			}

			for (int component = 0; component < N; ++component) {
				frame.mValue[component] = valueFloats[baseIndex + offset++];
			}

			for (int component = 0; component < N; ++component) {
				frame.mOut[component] = isSamplerCubic ? valueFloats[baseIndex + offset++] : 0.0f;
			}
		}
	} 

	void MeshFromAttribute(Mesh& outMesh, cgltf_attribute& attribute, cgltf_skin* skin, cgltf_node* nodes, unsigned int nodeCount) {
		cgltf_attribute_type attribType = attribute.type;
		cgltf_accessor& accessor = *attribute.data;

		unsigned int componentCount = 0;
		if (accessor.type == cgltf_type_vec2) {
			componentCount = 2;
		}
		else if (accessor.type == cgltf_type_vec3) {
			componentCount = 3;
		}
		else if (accessor.type == cgltf_type_vec4) {
			componentCount = 4;
		}
		std::vector<float> values;
		GetScalarValues(values, componentCount, accessor);
		unsigned int acessorCount = (unsigned int)accessor.count;

		std::vector<vec3>& positions = outMesh.GetPosition();
		std::vector<vec2>& texCoords = outMesh.GetTexCoord();
		std::vector<ivec4>& influences = outMesh.GetInfluences();
		std::vector<vec4>& weights = outMesh.GetWeights();

		// only for attribute types we care about
		for (unsigned int i = 0; i < acessorCount; ++i) {
			int index = i * componentCount;
			switch (attribType) {
			case cgltf_attribute_type_position:
				positions.push_back(vec3(values[index + 0], values[index + 1], values[index + 2]));
				break;
			case cgltf_attribute_type_texcoord:
				texCoords.push_back(vec2(values[index + 0], values[index + 1]));
				break;
			case cgltf_attribute_type_weights:
				weights.push_back(vec4(values[index + 0], values[index + 1], values[index + 2], values[index + 3]));
				break;
			case cgltf_attribute_type_joints:
			{
				// These indices are skin relative. This function has no information about the
				// skin that is being parsed. Add +0.5f to round, since we can't read ints
				ivec4 joints(
					(int)(values[index + 0] + 0.5f),
					(int)(values[index + 1] + 0.5f),
					(int)(values[index + 2] + 0.5f),
					(int)(values[index + 3] + 0.5f)
				);

				joints.x = std::max(0, GetNodeIndex(skin->joints[joints.x], nodes, nodeCount));
				joints.y = std::max(0, GetNodeIndex(skin->joints[joints.y], nodes, nodeCount));
				joints.z = std::max(0, GetNodeIndex(skin->joints[joints.z], nodes, nodeCount));
				joints.w = std::max(0, GetNodeIndex(skin->joints[joints.w], nodes, nodeCount));

				influences.push_back(joints);
			}
			break;
			} // End switch statement
		}
	}// End of MeshFromAttribute function

} // End of GLTFHelpers

cgltf_data const* LoadGLTFFile(char const* const path) {
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	cgltf_data* data(nullptr);
	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success) {
		std::cout << "Could not load input file: " << path << "\n";
		return 0;
	}
	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success) {
		cgltf_free(data);
		std::cout << "Could not load buffers for: " << path << "\n";
		return 0;
	}
	result = cgltf_validate(data);
	if (result != cgltf_result_success) {
		cgltf_free(data);
		std::cout << "Invalid gltf file: " << path << "\n";
		return 0;
	}
	return data;
}

void FreeGLTFFile(cgltf_data const* data) {
	if (data) {
		cgltf_free(const_cast<cgltf_data*>(data));
	}
}

Pose LoadRestPose(cgltf_data const* data) {
	unsigned int boneCount = (unsigned int)data->nodes_count;
	Pose result(boneCount);

	for (unsigned int i = 0; i < boneCount; ++i) {
		cgltf_node* node = &(data->nodes[i]);

		Transform transform = GLTFHelpers::GetLocalTransform(data->nodes[i]);
		result.SetLocalTransform(i, transform);

		int parent = GLTFHelpers::GetNodeIndex(node->parent, data->nodes, boneCount);
		result.SetParent(i, parent);
	}

	return result;
}

std::vector<std::string> LoadJointNames(cgltf_data const* data) {
	unsigned int boneCount = (unsigned int)data->nodes_count;
	std::vector<std::string> result(boneCount, "Not Set");

	for (unsigned int i = 0; i < boneCount; ++i) {
		cgltf_node* node = &(data->nodes[i]);

		if (node->name == 0) {
			result[i] = "EMPTY NODE";
		}
		else {
			result[i] = node->name;
		}
	}

	return result;
}

std::vector<Clip> LoadAnimationClips(cgltf_data const* data) {
	unsigned int numClips = (unsigned int)data->animations_count;
	unsigned int numNodes = (unsigned int)data->nodes_count;

	std::vector<Clip> result;
	result.resize(numClips);

	for (unsigned int i = 0; i < numClips; ++i) {
		result[i].SetName(data->animations[i].name);

		unsigned int numChannels = (unsigned int)data->animations[i].channels_count;
		for (unsigned int j = 0; j < numChannels; ++j) {
			cgltf_animation_channel& channel = data->animations[i].channels[j];
			cgltf_node* target = channel.target_node;
			int nodeId = GLTFHelpers::GetNodeIndex(target, data->nodes, numNodes);
			if (channel.target_path == cgltf_animation_path_type_translation) {
				VectorTrack& track = result[i][nodeId].GetPositionTrack();
				GLTFHelpers::TrackFromChannel<vec3, 3>(track, channel);
			}
			else if (channel.target_path == cgltf_animation_path_type_scale) {
				VectorTrack& track = result[i][nodeId].GetScaleTrack();
				GLTFHelpers::TrackFromChannel<vec3, 3>(track, channel);
			}
			else if (channel.target_path == cgltf_animation_path_type_rotation) {
				QuaternionTrack& track = result[i][nodeId].GetRotationTrack();
				GLTFHelpers::TrackFromChannel<quat, 4>(track, channel);
			}
		}
		result[i].RecalculateDuration();
	} 

	return result;
}

Pose LoadBindPose(cgltf_data const* data) {
	Pose restPose = LoadRestPose(data);
	unsigned int numBones = restPose.Size();
	std::vector<Transform> worldBindPose(numBones);
	for (unsigned int i = 0; i < numBones; ++i) {
		worldBindPose[i] = restPose.GetGlobalTransform(i);
	}
	unsigned int numSkins = (unsigned int)data->skins_count;
	for (unsigned int i = 0; i < numSkins; ++i) {
		cgltf_skin* skin = &(data->skins[i]);
		std::vector<float> invBindAccessor;
		GLTFHelpers::GetScalarValues(invBindAccessor, 16, *skin->inverse_bind_matrices);

		unsigned int numJoints = (unsigned int)skin->joints_count;
		for (unsigned int j = 0; j < numJoints; ++j) {
			// Read the ivnerse bind matrix of the joint
			float* matrix = &(invBindAccessor[j * 16]);
			mat4 invBindMatrix = mat4(matrix);
			// invert, convert to transform
			mat4 bindMatrix = inverse(invBindMatrix);
			Transform bindTransform = mat4ToTransform(bindMatrix);
			// Set that transform in the worldBindPose.
			cgltf_node* jointNode = skin->joints[j];
			int jointIndex = GLTFHelpers::GetNodeIndex(jointNode, data->nodes, numBones);
			worldBindPose[jointIndex] = bindTransform;
		} // end for each joint
	} // end for each skin
	// Convert the world bind pose to a regular bind pose
	Pose bindPose = restPose;
	for (unsigned int i = 0; i < numBones; ++i) {
		Transform current = worldBindPose[i];
		int p = bindPose.GetParent(i);
		if (p >= 0) { // Bring into parent space
			Transform parent = worldBindPose[p];
			current = combine(inverse(parent), current);
		}
		bindPose.SetLocalTransform(i, current);
	}

	return bindPose;
} // End LoadBindPose function

Skeleton LoadSkeleton(cgltf_data const* data) {
	return Skeleton(
		LoadRestPose(data),
		LoadBindPose(data),
		LoadJointNames(data)
	);
}

std::vector<Mesh> LoadMeshes(cgltf_data const* data) {
	std::vector<Mesh> result;
	cgltf_node* nodes = data->nodes;
	unsigned int nodeCount = (unsigned int)data->nodes_count;

	for (unsigned int i = 0; i < nodeCount; ++i) {
		cgltf_node* node = &nodes[i];
		if (node->mesh == 0 || node->skin == 0) {
			continue;
		}
		unsigned int numPrims = (unsigned int)node->mesh->primitives_count;
		for (unsigned int j = 0; j < numPrims; ++j) {
			result.push_back(Mesh());
			Mesh& mesh = result[result.size() - 1];

			cgltf_primitive* primitive = &node->mesh->primitives[j];
			
			// vertices, texture coordinates, joints, weights
			unsigned int numAttributes = (unsigned int)primitive->attributes_count;
			for (unsigned int k = 0; k < numAttributes; ++k) {
				cgltf_attribute* attribute = &primitive->attributes[k];
				GLTFHelpers::MeshFromAttribute(mesh, *attribute, node->skin, nodes, nodeCount);
			}

			// indices
			if (nullptr != primitive->indices) {
				
				unsigned int const indexCount((unsigned int)primitive->indices->count);
				
				std::vector<unsigned int>& indices(mesh.GetIndices());
				indices.reserve(indexCount);
				indices.resize(indexCount);

				for (unsigned int k = 0; k < indexCount; ++k) {
					indices[k] = (unsigned int)cgltf_accessor_read_index(primitive->indices, k);
				}
			}

			// material indices
			uint32_t const material_index(primitive->material - data->materials);
			mesh.GetMaterialIndices().emplace_back(material_index);


			mesh.UpdateBuffers();
		}
	}

	return result;
} // End of the LoadMeshes function

std::string const get_relative_file_path(std::string_view const base_path, std::string_view const new_path)
{
	size_t insert_point = base_path.find_last_of('/');
	if (insert_point) {
		insert_point++;
	}
	return(std::string(base_path.substr(0, insert_point)) + std::string(new_path));
}

std::vector<Image> LoadImages(cgltf_data const* data, std::string_view const model_uri) {

	std::vector<Image> result;

	uint32_t const image_count(data->images_count);

	result.reserve(image_count);

	cgltf_image const* const images = data->images;

	for (unsigned int i = 0; i < image_count; ++i) {

		cgltf_image const* const image = &images[i];
		if (nullptr == image) {
			continue;
		}

		result.emplace_back(get_relative_file_path(model_uri, image->uri));
	}

	return result;
} // End of the LoadImages function

std::vector<Texture> LoadTextures(cgltf_data const* data) {

	std::vector<Texture> result;

	uint32_t const texture_count(data->textures_count);

	result.reserve(texture_count);

	cgltf_texture const* const textures = data->textures;

	for (unsigned int i = 0; i < texture_count; ++i) {

		cgltf_texture const* const texture = &textures[i];
		if (nullptr == texture) {
			continue;
		}

		// index to reference image
		result.emplace_back((uint32_t)(texture->image - data->images));
	}

	return result;
} // End of the LoadTextures function

std::vector<Material> LoadMaterials(cgltf_data const* data) {

	std::vector<Material> result;

	uint32_t const material_count(data->materials_count);

	result.reserve(material_count);

	cgltf_material const* const materials = data->materials;

	for (unsigned int i = 0; i < material_count; ++i) {

		cgltf_material const* const material = &materials[i];
		if (nullptr == material) {
			continue;
		}

		Material mat{};

		if (material->has_pbr_metallic_roughness) {

			cgltf_pbr_metallic_roughness const mr = material->pbr_metallic_roughness;

			mat.metallic = mr.metallic_factor;
			mat.roughness = mr.roughness_factor;
			
			if (mr.base_color_texture.texture) {
				mat.image_index = (uint32_t)(mr.base_color_texture.texture->image - data->images);
			}
		}
		// emission color -> emission only (greyscale)
		mat.emission = bool(material->emissive_factor[0] + material->emissive_factor[1] + material->emissive_factor[2]);
		mat.transparent = (cgltf_alpha_mode_opaque != material->alpha_mode);
		  
		// index to reference image
		result.emplace_back(mat);
	}

	return result;
} // End of the LoadTextures function

