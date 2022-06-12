/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#ifndef VECTOR_ROTATION_H
#define VECTOR_ROTATION_H
#include "superfastmath.h"
#include "point2D_t.h"
#include <numbers>

struct alignas(16) v2_rotation_t {

private:
	union alignas(16)
	{
		struct alignas(16)
		{
			float c, s, Angle;		// cos, sin & angle retained in radians
		};
		
		XMFLOAT3A vR;
	};

public:
	__forceinline float const cosine() const { return(c); }
	__forceinline float const sine() const { return(s); }

	__inline XMVECTOR const XM_CALLCONV data(void) const { return(XMLoadFloat3A(&vR)); } // for raw vector of values equal to cosine, sine, angle
	__inline XMVECTOR const XM_CALLCONV v2(void) const { return(XMLoadFloat2A((XMFLOAT2A const* const __restrict)&vR)); } // only want to return cos, sin in v2 x, y excluding saved angle
	__inline float const __vectorcall   angle(void) const { return(Angle); } // range is -XM_PI to XM_PI //
private:
	// keeping the range wrapped from -PI to PI is important to preserve the highest precision as input into sin / cos functions
	// get an extra bit of precision!
	// "Exploit symmetrical functions" section
	// https://developer.arm.com/solutions/graphics/developer-guides/understanding-numerical-precision/single-page
	// https://stackoverflow.com/questions/66401359/constrain-a-value-pi-to-pi-for-precision-buff/
	//
	STATIC_INLINE_PURE float const __vectorcall constrain(float const fAngle)
	{
		static constexpr double const 
			dPI(std::numbers::pi),
			d2PI(2.0 * std::numbers::pi),
			dResidue(-1.74845553146951715461909770965576171875e-07);	// abs difference between d2PI(double precision) and XM_2PI(float precision)
		
		double dAngle(fAngle);

		dAngle = std::remainder(dAngle, d2PI);

		if (dAngle > dPI) {
			dAngle = dAngle - d2PI - dResidue;
		}
		else if (dAngle < -dPI) {
			dAngle = dAngle + d2PI + dResidue;
		}

		return((float const)dAngle);
	}
	inline void __vectorcall recalculate( float fAngle ) // in radians
	{
		fAngle = constrain(fAngle);

		s = SFM::sincos(&c, fAngle);	// SVML of sincos is precise enough for trig identities


		//s = std::sin(fAngle);		// must use precise sinf and cosf C99 functions
		//c = std::cos(fAngle);		// precision must be maintained for the trig identity function optimizations to work properly
										// the trig identity functions do NOT accumulate properly - error also begins to accumulate
										// so they are really only an optimization for the most useful case where a discrete angle needs
		Angle = fAngle;					// to be added/subtracted from and existing angle without the recalculation of sin/cos
	}									// however cannot be used succesfively over many loops / frames / etc. as error begins to accumulate fast
										// kinda stupid if you as me, these are supposed to be identities!! but due to floating point rounding errors, the error accumulates

public:
	// for raw data initialization usage only, prefer other constructors for normal usage
	inline explicit v2_rotation_t(float const fCos, float const fSin, float const fAngle) // in radians
		: c(fCos), s(fSin), Angle(fAngle)
	{}
	
public:		
	inline v2_rotation_t()
		: c(1.0f), s(0.0f), Angle(0.0f)
	{}
	// *careful* do NOT ever bmi or initialize a v2_rotation_t object with 0.0f, use the default empty ctor instead !!!!

	inline explicit v2_rotation_t(float const fAngle)  // in radians // CTOR always requires (precise) calculation of sin/cos - must be the precise version for loadtime
		: c(1.0f), s(0.0f), Angle(0.0f)				   // or else the error introduced goes thru the roof on the trig sum / diff identity functions
	{													
		if (0.0f != fAngle) {
			recalculate(fAngle);
		}
	}

	inline bool const operator==(v2_rotation_t const& rhs) const {

		return(XMVector2Equal(v2(), rhs.v2()));
	}

	inline v2_rotation_t& __vectorcall operator=(v2_rotation_t const& rhs) = default;
	inline v2_rotation_t& __vectorcall operator=(float const fAngle) {
		
		recalculate(fAngle);

		return(*this);
	}
	inline v2_rotation_t& __vectorcall operator=(FXMVECTOR const xmDir) { //*** Must be a 2D normalized vector (direction), or resulting (*this) v2_rotation_t is undefined

		XMVECTOR const xmAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)); // w/respect to X+ Axis
		
		Angle = constrain(SFM::__acos_approx(XMVectorGetX(XMVector2Dot(xmAxis, xmDir))));
		
		XMStoreFloat2A((XMFLOAT2A* const __restrict) &vR, xmDir);

		return(*this);
	}
	inline void __vectorcall zero() { c = 1.0f; s = 0.0f; Angle = 0.0f; }
	inline bool const __vectorcall isZero() const {
		return(0.0f == Angle);
	}
	// #### following functions operate without recalculating sin / cos of angle
	
	inline v2_rotation_t const __vectorcall operator-() const	// negate
	{
		// only need to negate sin, cos is always positive
		return( v2_rotation_t(c, -s, -Angle) );
	}
	
	inline v2_rotation_t const __vectorcall operator+(v2_rotation_t const& __restrict rhs) const	// add two angles together (only good for one-shot - no accunulation)
	{
		float fAngle(Angle + rhs.Angle);
		fAngle = constrain(fAngle);

		// trig identities for sum 2 angles
		// cos(a + b) = cos(a) cos(b) - sin(a) sin(b)
		// sin(a + b) = sin(a) cos(b) + cos(a) sin(b)
		return( v2_rotation_t(SFM::__fms(c, rhs.c, s * rhs.s), SFM::__fma(s,rhs.c , c*rhs.s), fAngle) ); // --- leverages existing sincos computations yes!
	}
	
	inline v2_rotation_t const __vectorcall operator-(v2_rotation_t const& __restrict rhs) const // subtract *this angle by another angle (only good for one-shot - no accunulation)
	{
		float fAngle(Angle - rhs.Angle);
		fAngle = constrain(fAngle);

		// trig identities for diff 2 angles
		// cos(a - b) = cos(a) cos(b) + sin(a) sin(b)
		// sin(a - b) = sin(a) cos(b) - cos(a) sin(b)
		return( v2_rotation_t(SFM::__fma(c, rhs.c, s * rhs.s), SFM::__fms(s,rhs.c , c*rhs.s), fAngle) );	// --- leverages existing sincos computations yes!
	}
	
	/// ### the following function require recalculation of sin / cos but use the faster variants
	inline v2_rotation_t const __vectorcall operator+(float const angle_) const {

		return(v2_rotation_t(Angle + angle_));
	}

	inline v2_rotation_t const __vectorcall operator-(float const angle_) const {
		return(v2_rotation_t(Angle - angle_));
	}
	inline void __vectorcall operator+=(float const fAngle)
	{
		recalculate(Angle + fAngle);
	}
	inline void __vectorcall operator-=(float const fAngle)
	{
		recalculate(Angle - fAngle);
	}

	// FAST LERP - approximate angle interpolation (use when suitable)
	// quadtradic lerp between two v2_rotation_t's to approximate a circle interpolation that would be the result of sincos (updating the angle) usage
	// *** this only works in first quadrant 0 - 90 degrees, so a & b must be in this range, otherwise the lerp takes a shortcut across quadrants producing an incorrect result
	// *** this is useful in say rotating a cube, and repeating 0 - 90 degrees over and over again will visually appear as if the cube is rotating a full 360 degrees when it is only repetively rotating 90 degrees
	// its a hack and should only be used in situations where the approximation is suitable - otherwise use the normal sincos route for recalculating new angles
	STATIC_INLINE_PURE v2_rotation_t const __vectorcall lerp(v2_rotation_t const& __restrict a, v2_rotation_t const& __restrict b, float const t)
	{
		v2_rotation_t result{};

		// clamp to [0.0f ... 1.0f] range
		float tT(SFM::saturate(t));

		// apply quadratic easing to clamped input interpolation factor - applies a curve to approximate a circle as if using the real cos, sin & angle 
		tT = SFM::ease_inout_quadratic(0.0f, 1.0f, tT);

		// the v2_rotation structure is aligned 16, and contains 3 floats - hack here to speedy load of all 3 values be computed simultaneously (lerp of cos, sin & Angle)
		XMStoreFloat3A((XMFLOAT3A* const __restrict)&result, SFM::lerp(XMLoadFloat3A((XMFLOAT3A const* const __restrict)&a), XMLoadFloat3A((XMFLOAT3A const* const __restrict)&b), tT));
	
		return(result);
	}																		

};

namespace v2_rotation_constants		// good for leverage trig identity sum/diff functions. However values lower than 1 degree or if the angle is accumulated over time
{									// the trig identity functions and these constants should not be used
	 extern const __declspec(selectany) inline v2_rotation_t const 	
		
		v270(XMConvertToRadians(270.0f)),
		v225(XMConvertToRadians(225.0f)),
		v180(XMConvertToRadians(180.0f)),		// a good example:
		v90(XMConvertToRadians(90.0f)),			// v2_rotation_t vNegatedAndOffset = -vR + RotationConstants.v45  // once...
		v45(XMConvertToRadians(45.0f)),			
		v23(XMConvertToRadians(23.0f)),	        // a bad example:
		v15(XMConvertToRadians(15.0f)),			// vR = vR + RotationConstants.v1   // and repeating this operation over and over
		v10(XMConvertToRadians(10.0f)),
		 v9(XMConvertToRadians(9.0f)),
		 v8(XMConvertToRadians(8.0f)),
		 v7(XMConvertToRadians(7.0f)),
		 v6(XMConvertToRadians(6.0f)),
		 v5(XMConvertToRadians(5.0f)),
		 v4(XMConvertToRadians(4.0f)),
		 v3(XMConvertToRadians(3.0f)),
		 v2(XMConvertToRadians(2.0f)),
		 v1(XMConvertToRadians(1.0f));	
};

// #### the following functions operate in any given "space" used, rotating a point explicitly
STATIC_INLINE_PURE point2D_t const p2D_rotate(point2D_t const p, v2_rotation_t const angle)
{
	XMFLOAT2A fP;
	XMStoreFloat2A(&fP, p2D_to_v2(p));

	// rotate point
	return(v2_to_p2D(XMVectorSet(SFM::__fms(fP.x, angle.cosine(), fP.y * angle.sine()),
								 SFM::__fma(fP.x, angle.sine(),   fP.y * angle.cosine()), 0.0f, 0.0f)));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v2_rotate(FXMVECTOR const xmP, v2_rotation_t const angle)
{
	// rotate point
	XMFLOAT2A p;
	XMStoreFloat2A(&p, xmP);
	return(XMVectorSet(SFM::__fms(p.x, angle.cosine(), p.y * angle.sine()),
					   SFM::__fma(p.x, angle.sine(),   p.y * angle.cosine()), 0.0f, 0.0f));
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate_pitch(FXMVECTOR const xmP, FXMVECTOR const xmCS)
{
	XMFLOAT3A p;
	XMFLOAT2A cs;
	XMStoreFloat3A(&p, xmP);
	XMStoreFloat2A(&cs, xmCS);

	// rotate point ( Pitch = Z Axis )
	// xmAxis.r[0] = XMVectorSet( c,  s, 0.0f, 1.0f );
	// xmAxis.r[1] = XMVectorSet(-s, c, 0.0f, 1.0f);
	return(XMVectorSet(SFM::__fma(p.x, cs.x, p.y * cs.y),
					   SFM::__fma(p.x, -cs.y, p.y * cs.x),
					   p.z, 0.0f));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate_pitch(FXMVECTOR const xmP, v2_rotation_t const angle)
{
	return(v3_rotate_pitch(xmP, angle.v2()));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate_azimuth(FXMVECTOR const xmP, FXMVECTOR const xmCS)
{
	XMFLOAT3A p;
	XMFLOAT2A cs;
	XMStoreFloat3A(&p, xmP);
	XMStoreFloat2A(&cs, xmCS);

	// rotate point ( Azimuth = Y Axis )
	// xmAxis.r[0] = XMVectorSet( c, 0.0f, -s, 1.0f );
	// xmAxis.r[2] = XMVectorSet( s, 0.0f,  c, 1.0f );

	return(XMVectorSet(SFM::__fms(p.x, cs.x, p.z * cs.y),
					   p.y,
					   SFM::__fma(p.x, cs.y, p.z * cs.x), 0.0f));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate_azimuth(FXMVECTOR const xmP, v2_rotation_t const angle)
{
	return(v3_rotate_azimuth(xmP, angle.v2()));
}

/// #### the following functions operate in any given "space" used, rotating with respect to an origin
STATIC_INLINE_PURE point2D_t const p2D_rotate(point2D_t p, point2D_t const origin, v2_rotation_t const angle)
{
	// translate point back to origin:
	p = p2D_sub(p, origin);

	// rotate point
	p = p2D_rotate(p, angle);

	// translate point back:
	return(p2D_add(p, origin));
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v2_rotate(FXMVECTOR p, XMVECTOR const origin, v2_rotation_t const angle)
{
	// translate point back to origin:
	XMVECTOR xmP = XMVectorSubtract(p, origin);

	// rotate point
	xmP = v2_rotate(xmP, angle);

	// translate point back:
	return(XMVectorAdd(xmP, origin));
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v3_rotate_azimuth(FXMVECTOR p, XMVECTOR const origin, v2_rotation_t const angle)
{
	// translate point back to origin:
	XMVECTOR xmP = XMVectorSubtract(p, origin);

	// rotate point
	xmP = v3_rotate_azimuth(xmP, angle);

	// translate point back:
	return(XMVectorAdd(xmP, origin));
}


/// #### following rotation functions operate in screenspace, rotation is done isometrically
STATIC_INLINE_PURE point2D_t const p2D_rotate_iso(point2D_t const p, v2_rotation_t const angle)
{
	XMFLOAT2A fP;
	XMStoreFloat2A(&fP, p2D_to_v2(p));

	// rotate point
	return(v2_to_p2D(SFM::v2_CartToIso(XMVectorSet(SFM::__fms(fP.x, angle.cosine(), fP.y * angle.sine()),
												   SFM::__fma(fP.x, angle.sine(),   fP.y * angle.cosine()), 0.0f, 0.0f))));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v2_rotate_iso(FXMVECTOR const p, v2_rotation_t const angle)
{
	// rotate point
	return(SFM::v2_CartToIso(v2_rotate(p, angle)));
}

STATIC_INLINE_PURE point2D_t const p2D_rotate_iso(point2D_t p, point2D_t const origin, v2_rotation_t const angle)
{
	// translate point back to origin:
	p = p2D_sub(p, origin);

	// rotate point
	p = p2D_rotate_iso(p, angle);

	// translate point back:
	return(p2D_add(p, origin));
}

STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV v2_rotate_iso(FXMVECTOR p, FXMVECTOR const origin, v2_rotation_t const angle)
{
	// translate point back to origin:
	XMVECTOR xmP = XMVectorSubtract(p, origin);

	// rotate point
	xmP = v2_rotate_iso(xmP, angle);

	// translate point back:
	return(XMVectorAdd(xmP, origin));
}

#endif // VECTOR_ROTATION_H

