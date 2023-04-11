#pragma once
#ifndef _H_MESH_
#define _H_MESH_

#include "vec2.h"
#include "vec3.h"
#include "vec4.h"
#include "mat4.h"
#include <vector>
#include "Attribute.h"
#include "Skeleton.h"
#include "Pose.h"

class Mesh {
protected:
	std::vector<vec3> mPosition;
	std::vector<vec2> mTexCoord;
	std::vector<vec4> mWeights;
	std::vector<ivec4> mInfluences;
	std::vector<unsigned int> mIndices;
	std::vector<unsigned int> mMaterialIndices;
protected:
	Attribute<vec3>* mPosAttrib;
	Attribute<vec2>* mUvAttrib;
	Attribute<vec4>* mWeightAttrib;
	Attribute<ivec4>* mInfluenceAttrib;
	Attribute<unsigned int>* mIndicesAttrib;
	Attribute<unsigned int>* mMaterialIndicesAttrib;
protected:
	std::vector<vec3> mSkinnedPosition;
	std::vector<mat4> mPosePalette;
public:
	Mesh();
	Mesh(const Mesh&);
	Mesh& operator=(const Mesh&);
	~Mesh();

	std::vector<vec3> const& GetSkinnedPosition() const { return(mSkinnedPosition); }
	std::vector<vec3> const& GetPosition() const { return(mPosition); } // always the base mesh vertices
	std::vector<vec2> const& GetTexCoord() const { return(mTexCoord); }
	std::vector<vec4> const& GetWeights() const { return(mWeights); }
	std::vector<ivec4> const& GetInfluences() const { return(mInfluences); }
	std::vector<uint32_t> const& GetIndices() const { return(mIndices); }
	std::vector<uint32_t> const& GetMaterialIndices() const { return(mMaterialIndices); }

	std::vector<vec3>& GetPosition(); // always the base mesh vertices
	std::vector<vec2>& GetTexCoord();
	std::vector<vec4>& GetWeights();
	std::vector<ivec4>& GetInfluences();
	std::vector<uint32_t>& GetIndices();
	std::vector<uint32_t>& GetMaterialIndices();

	void CPUSkin(Skeleton& skeleton, Pose& pose);
	void UpdateBuffers();
};


#endif // !_H_MESH_
