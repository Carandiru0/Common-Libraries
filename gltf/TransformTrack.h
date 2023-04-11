#pragma once
#ifndef _H_TRANSFORMTRACK_
#define _H_TRANSFORMTRACK_

#include "Track.h"
#include "Transform.h"

class TransformTrack {
protected:
	unsigned int mId;
	VectorTrack mPosition;
	QuaternionTrack mRotation;
	VectorTrack mScale;
public:
	TransformTrack();
	unsigned int GetId();
	void SetId(unsigned int id);
	VectorTrack& GetPositionTrack();
	QuaternionTrack& GetRotationTrack();
	VectorTrack& GetScaleTrack();
	float GetStartTime();
	float GetEndTime();
	bool IsValid();
	Transform Sample(const Transform& ref, float time, bool looping);
};

#ifdef GLTF_IMPLEMENTATION
TransformTrack::TransformTrack() {
	mId = 0;
}

unsigned int TransformTrack::GetId() {
	return mId;
}

void TransformTrack::SetId(unsigned int id) {
	mId = id;
}

VectorTrack& TransformTrack::GetPositionTrack() {
	return mPosition;
}

QuaternionTrack& TransformTrack::GetRotationTrack() {
	return mRotation;
}

VectorTrack& TransformTrack::GetScaleTrack() {
	return mScale;
}

bool TransformTrack::IsValid() {
	return mPosition.Size() > 1 || mRotation.Size() > 1 || mScale.Size() > 1;
}

float TransformTrack::GetStartTime() {
	float result = 0.0f;
	bool isSet = false;

	if (mPosition.Size() > 1) {
		result = mPosition.GetStartTime();
		isSet = true;
	}
	if (mRotation.Size() > 1) {
		float rotationStart = mRotation.GetStartTime();
		if (rotationStart < result || !isSet) {
			result = rotationStart;
			isSet = true;
		}
	}
	if (mScale.Size() > 1) {
		float scaleStart = mScale.GetStartTime();
		if (scaleStart < result || !isSet) {
			result = scaleStart;
			isSet = true;
		}
	}

	return result;
}

float TransformTrack::GetEndTime() {
	float result = 0.0f;
	bool isSet = false;

	if (mPosition.Size() > 1) {
		result = mPosition.GetEndTime();
		isSet = true;
	}
	if (mRotation.Size() > 1) {
		float rotationEnd = mRotation.GetEndTime();
		if (rotationEnd > result || !isSet) {
			result = rotationEnd;
			isSet = true;
		}
	}
	if (mScale.Size() > 1) {
		float scaleEnd = mScale.GetEndTime();
		if (scaleEnd > result || !isSet) {
			result = scaleEnd;
			isSet = true;
		}
	}

	return result;
}

Transform TransformTrack::Sample(const Transform& ref,
	float time, bool looping) {
	Transform result = ref; // Assign default values
	if (mPosition.Size() > 1) { // Only assign if animated
		result.position = mPosition.Sample(time, looping);
	}
	if (mRotation.Size() > 1) { // Only assign if animated
		result.rotation = mRotation.Sample(time, looping);
	}
	if (mScale.Size() > 1) { // Only assign if animated
		result.scale = mScale.Sample(time, looping);
	}
	return result;
}
#endif
#endif
