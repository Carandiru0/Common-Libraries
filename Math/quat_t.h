/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef QUATERNION_H
#define QUATERNION_H
#include "superfastmath.h"
#include <numbers>

struct quat_t {

private:
	XMFLOAT4	_quaternion;

public:
	__forceinline XMVECTOR const __vectorcall v4() const { return(XMLoadFloat4(&_quaternion)); }

public:
	// by another quaternion
	inline explicit quat_t(FXMVECTOR quat) 
		//: _quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // identity
	{
		XMStoreFloat4(&_quaternion, quat);
	}
public:		
	// *careful* do NOT ever bmi or initialize a quat_t object with 0.0f, use the default empty ctor instead !!!!
	inline quat_t()
		: _quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // identity
	{
	}
	
	// roll = rotation around x axis
	// yaw = rotation around y axis
	// pitch = rotation around z axis
	quat_t const& __vectorcall roll_yaw_pitch(float const xRoll, float const yYaw, float const zPitch) // for updating quaternion from roll yaw and pitch
	{
		// cheaper to actually calculate again - single sincos vectorizxed for all 3 angles

		// XYZ + - + - (corrected order) <-----
		// YXZ + - - + (original order)
		// ZXY - + + -
		// ZYX - + - +
		// YZX + + - -
		// XZY - - + +

		//                                                 *do not change* extremely sensitive to order & sign
		constinit static const XMVECTORF32  Sign = { { { 1.0f, -1.0f, 1.0f, -1.0f } } };

		//                                                 *do not change* extremely sensitive to order & sign
		XMVECTOR HalfAngles = XMVectorMultiply(XMVectorSet(xRoll, -yYaw, -zPitch, 0.0f), g_XMOneHalf.v);

		XMVECTOR SinAngles, CosAngles;
		SinAngles = SFM::sincos(&CosAngles, HalfAngles);

		XMVECTOR const P0 = XMVectorPermute<XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1X, XM_PERMUTE_1X>(SinAngles, CosAngles);
		XMVECTOR const Y0 = XMVectorPermute<XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Y>(SinAngles, CosAngles);
		XMVECTOR const R0 = XMVectorPermute<XM_PERMUTE_1Z, XM_PERMUTE_1Z, XM_PERMUTE_0Z, XM_PERMUTE_1Z>(SinAngles, CosAngles);
		XMVECTOR const P1 = XMVectorPermute<XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1X, XM_PERMUTE_1X>(CosAngles, SinAngles);
		XMVECTOR const Y1 = XMVectorPermute<XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Y>(CosAngles, SinAngles);
		XMVECTOR const R1 = XMVectorPermute<XM_PERMUTE_1Z, XM_PERMUTE_1Z, XM_PERMUTE_0Z, XM_PERMUTE_1Z>(CosAngles, SinAngles);

		XMVECTOR Q1 = XMVectorMultiply(P1, Sign.v);
		XMVECTOR Q0 = XMVectorMultiply(P0, Y0);
		Q1 = XMVectorMultiply(Q1, Y1);
		Q0 = XMVectorMultiply(Q0, R0);

		XMStoreFloat4(&_quaternion, XMVectorMultiplyAdd(Q1, R1, Q0));   /* do not change */ // *** corrected (order & sign) angles match engine coordinate xyz system layout

		return(*this);
	}
	inline explicit quat_t(float const xRoll, float const yYaw, float const zPitch) // preferred (euler angles)
	{
		roll_yaw_pitch(xRoll, yYaw, zPitch);
	}

	inline bool const operator==(quat_t const& rhs) const {

		return(XMVector4Equal(v4(), rhs.v4()));
	}

	inline quat_t& __vectorcall operator=(quat_t const& rhs) = default;

	inline void __vectorcall zero() { XMStoreFloat4(&_quaternion, XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)); }
	inline bool const __vectorcall isZero() const {
		return(XMVector4Equal(v4(), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)));
	}

	quat_t const& __vectorcall inverse()
	{
		XMStoreFloat4(&_quaternion, XMQuaternionInverse(XMLoadFloat4(&_quaternion)));
		return(*this);
	}
};

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate(FXMVECTOR const xmP, quat_t const rotation)
{
	XMVECTOR const xmQ(rotation.v4());

	// p + 2.0 * cross(q.xyz, cross(q.xyz, p) + q.w * p);
	return(SFM::__fma(XMVector3Cross(xmQ, XMVectorAdd(XMVector3Cross(xmQ, xmP), XMVectorScale(xmP, XMVectorGetW(xmQ)))), XMVectorReplicate(2.0f), xmP));
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate(FXMVECTOR p, XMVECTOR const origin, quat_t const angle)
{
	// translate point back to origin:
	XMVECTOR xmP = XMVectorSubtract(p, origin);

	// rotate point
	xmP = v3_rotate(xmP, angle);

	// translate point back:
	return(XMVectorAdd(xmP, origin));
}

#endif // QUATERNION_H

