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
#include "v2_rotation_t.h"
#include <numbers>

struct alignas(16) quat_t { // *valid only for rotation quaternions*

private:
	XMFLOAT4A	_quaternion;

public:
	__forceinline XMVECTOR const __vectorcall v4() const { return(XMLoadFloat4A(&_quaternion)); }

public:
	// by another quaternion
	inline explicit quat_t(FXMVECTOR quat) 
	{
		XMStoreFloat4A(&_quaternion, quat);
	}	
	// *careful* do NOT ever bmi or initialize a quat_t object with 0.0f, use the default empty ctor instead !!!!
	inline quat_t()
		: _quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // identity
	{
	}
	
	// pitch = rotation around x axis
	// yaw = rotation around y axis
	// roll = rotation around z axis
	/*
	XMVECTOR const __vectorcall pitch_yaw_roll(float const xPitch, float const yYaw, float const zRoll) const // for updating quaternion from pitch, yaw and roll
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
		XMVECTOR const HalfAngles = XMVectorMultiply(XMVectorSet(xPitch, -yYaw, -zRoll, 0.0f), g_XMOneHalf.v);

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

		// do not change  // *** corrected (order & sign) angles match engine coordinate xyz system layout
		return(XMVectorMultiplyAdd(Q1, R1, Q0));
	}
	*/

	XMVECTOR const __vectorcall pitch_yaw_roll(float const xPitch, float const yYaw, float const zRoll) const
	{
		XMVECTOR SinAngles, CosAngles;
		SinAngles = SFM::sincos(&CosAngles, XMVectorScale(XMVectorSet(xPitch, yYaw, zRoll, 0.0f), 0.5f)); // sincos of half-angles, recalculation required.

		XMVECTOR xmQx(XMVectorZero()), xmQy(XMVectorZero()), xmQz(XMVectorZero());

		xmQx = XMVectorSetX(xmQx, XMVectorGetX(SinAngles));
		xmQx = XMVectorSetW(xmQx, XMVectorGetX(CosAngles));

		xmQy = XMVectorSetY(xmQy, XMVectorGetY(SinAngles));
		xmQy = XMVectorSetW(xmQy, XMVectorGetY(CosAngles));

		xmQz = XMVectorSetZ(xmQz, XMVectorGetZ(SinAngles));
		xmQz = XMVectorSetW(xmQz, XMVectorGetZ(CosAngles));

		xmQx = XMQuaternionNormalize(xmQx);
		xmQy = XMQuaternionNormalize(xmQy);
		xmQz = XMQuaternionNormalize(xmQz);

		// this defines the order used - ZXY
		return(XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionMultiply(xmQz, xmQx), xmQy))); // ZXY
	}

	inline explicit quat_t(float const xPitch, float const yYaw, float const zRoll) // secondary
	{
		XMStoreFloat4A(&_quaternion, pitch_yaw_roll(xPitch, yYaw, zRoll));
	}
	inline explicit quat_t(v2_rotation_t const& xPitch, v2_rotation_t const& yYaw, v2_rotation_t const& zRoll) // preferred
	{
		XMStoreFloat4A(&_quaternion, pitch_yaw_roll(xPitch.angle(), yYaw.angle(), zRoll.angle()));
	}
	inline bool const operator==(quat_t const& rhs) const {

		return(XMVector4Equal(v4(), rhs.v4()));
	}

	inline quat_t& __vectorcall operator=(quat_t const& rhs) = default;
	inline quat_t& __vectorcall operator=(FXMVECTOR rhs) {
		XMStoreFloat4A(&_quaternion, rhs);
		return(*this);
	}

	inline void zero() { XMStoreFloat4A(&_quaternion, XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)); }
	inline bool const isZero() const {
		return(XMVector4Equal(v4(), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)));
	}

	XMVECTOR const __vectorcall inverse() const
	{
		// For rotation quaternions, the inverse equals the conjugate
		return(XMQuaternionConjugate(XMLoadFloat4A(&_quaternion)));
	}
};

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate(FXMVECTOR const xmP, quat_t const& rotation)
{
	// p` = inverse(q) * p * q
	return( XMQuaternionMultiply(rotation.inverse(), XMQuaternionMultiply(XMVectorSetW(xmP, 0.0f), rotation.v4())) );
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate(FXMVECTOR p, XMVECTOR const origin, quat_t const& rotation)
{
	// translate point back to origin:
	XMVECTOR xmP = XMVectorSubtract(p, origin);

	// rotate point
	xmP = v3_rotate(xmP, rotation);

	// translate point back:
	return(XMVectorAdd(xmP, origin));
}

#endif // QUATERNION_H

