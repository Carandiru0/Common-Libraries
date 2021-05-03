/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#pragma once
#ifndef BIARC_H
#define BIARC_H
#include <stdint.h>
#include "superfastmath.h"

// public user struct
struct biarc_t
{
	XMFLOAT3A	center,		// center of the circle (or line)
				axis[2];	// 0 - vector from center to end point, 1 - vector from center edge perpendicular to axis 0
	float		radius,		// radius of the circule (zero for a line)
				angle,		// angle to rotate from axis 0 towards axis 1
				length;		// distance along the arc 
};

// public user functions (forward decl)
STATIC_INLINE_PURE void __vectorcall biarc_computeArcs(biarc_t& __restrict rArc0, biarc_t& __restrict rArc1,
													   FXMVECTOR const xmStart, FXMVECTOR const xmStartTangent,		 // start position & start tangent
													   FXMVECTOR const xmEnd, FXMVECTOR const xmEndTangent);		 // end position & end tangent



STATIC_INLINE_PURE XMVECTOR const __vectorcall biarc_interpolate(biarc_t const& __restrict rArc0, biarc_t const& __restrict rArc1, float const tNorm);




// private internal functions
namespace internal
{

	STATIC_INLINE_PURE void __vectorcall biarc_computeArc(XMFLOAT3A& __restrict rCenter, float& __restrict rRadius, float& __restrict rAngle,
														  FXMVECTOR const xmPoint, FXMVECTOR const xmTangent, FXMVECTOR const xmPointToMid)

	{
		// tangent must be normalized already
		constexpr float const EPSILON = 0.00001f;

		// normal to arc plane
		XMVECTOR const xmNormal = XMVector3Cross(xmPointToMid, xmTangent);

		// compute an axis within the arc plane that is perpendicular to the tangent. 
		// this will be colinear with the vector from the center to the end point.
		XMVECTOR const xmPerpAxis = XMVector3Cross(xmTangent, xmNormal);

		float const denominator = XMVectorGetX(XMVectorScale(XMVector3Dot(xmPerpAxis, xmPointToMid), 2.0f));

		if (SFM::abs(denominator) - EPSILON < 0.0f) {

			// the radius is infinite, so use a straight line. Place the center
			// in the middle of the line.
			XMStoreFloat3A(&rCenter, XMVectorAdd(xmPoint, XMVectorScale(xmPointToMid, 0.5f)));
			rRadius = 0.0f;
			rAngle = 0.0f;
		}
		else {

			// compute the distance to the center along perpAxis
			const float centerDist = XMVectorGetX(XMVectorScale(XMVector3Dot(xmPointToMid, xmPointToMid), 1.0f / denominator));
			XMStoreFloat3A(&rCenter, XMVectorAdd(xmPoint, XMVectorScale(xmPerpAxis, centerDist)));

			// compute the radius in absolute units
			float const perpAxisMagnitude = XMVectorGetX(XMVector3Length(xmPerpAxis));
			float const radius = SFM::abs(centerDist * perpAxisMagnitude);

			float angle(0.0f); // default to zero if radius is less than EPSILON

			if (radius - EPSILON >= 0.0f) {

				float const inv_radius(1.0f / radius);

				// compute normalized directions from the center to the connection point 
				// and from the center to he end point
				XMVECTOR xmCenterToMid, xmCenterToEnd;

				xmCenterToMid = XMVectorSubtract(xmPoint, XMLoadFloat3A(&rCenter));
				xmCenterToEnd = XMVectorScale(xmCenterToMid, inv_radius); // normalizing vector

				xmCenterToMid = XMVectorAdd(xmCenterToMid, xmPointToMid);
				xmCenterToMid = XMVectorScale(xmCenterToMid, inv_radius); // normalizing vector

				// compute the rotation direction
				const float twist = XMVectorGetX(XMVector3Dot(xmPerpAxis, xmPointToMid)) >= 0.0f ? 1.0f : -1.0f; // sign only required

				// compute the angle
				angle = SFM::__acos_approx(XMVectorGetX(XMVector3Dot(xmCenterToEnd, xmCenterToMid))) * twist;
			}

			// out radius & angle
			rRadius = radius;
			rAngle = angle;
		}

	}
} // end ns

// implementation of public user functions:

// compute a pair of arcs for biarc interpolation
STATIC_INLINE_PURE void __vectorcall biarc_computeArcs(biarc_t& __restrict rArc0, biarc_t& __restrict rArc1,
													   FXMVECTOR const xmStart, FXMVECTOR const xmStartTangent,  // start position & start tangent
													   FXMVECTOR const xmEnd, FXMVECTOR const xmEndTangent)		 // end position & end tangent
{
	// start and end tangents must be normalized already
	constexpr float const EPSILON = 0.00001f;

	XMVECTOR const xmV = XMVectorSubtract(xmEnd, xmStart);

	float const vDotV = XMVectorGetX(XMVector3Dot(xmV, xmV));

	// if the control points are equal, no need to interpolate
	if (vDotV - EPSILON < 0.0f) {

		XMStoreFloat3A(&rArc0.center, xmStart);
		XMStoreFloat3A(&rArc0.axis[0], xmV);
		XMStoreFloat3A(&rArc0.axis[1], xmV);
		rArc0.radius = 0.0f;
		rArc0.angle = 0.0f;
		rArc0.length = 0.0f;

		XMStoreFloat3A(&rArc1.center, xmStart);
		XMStoreFloat3A(&rArc1.axis[0], xmV);
		XMStoreFloat3A(&rArc1.axis[1], xmV);
		rArc1.radius = 0.0f;
		rArc1.angle = 0.0f;
		rArc1.length = 0.0f;

		return;
	}

	// compute the denominator for quadtratic formula
	XMVECTOR const xmT = XMVectorAdd(xmStartTangent, xmEndTangent);

	float const vDotT = XMVectorGetX(XMVector3Dot(xmV, xmT));
	float const t0Dott1 = XMVectorGetX(XMVector3Dot(xmStartTangent, xmEndTangent));
	float const denominator = 2.0f * (1.0f - t0Dott1);

	// if quadtratic formula denominator is zero, tangents are equal and
	// there is a special case:
	float d;
	if (denominator - EPSILON < 0.0f) {

		// special case //
		const float vDotT1 = XMVectorGetX(XMVector3Dot(xmV, xmEndTangent));

		// if the special case d is infinity, the only solution is to 
		// interpolate across the two semi-circles
		if (SFM::abs(vDotT1) - EPSILON < 0.0f) {

			const float vMag = SFM::__sqrt(vDotV);
			const float invMagSq = 1.0f / vDotV;

			// compute the normal to the plane containing the arcs
			// (this has length vMag)
			XMVECTOR const xmPlaneNormal = XMVector3Cross(xmV, xmEndTangent);

			// compute the axis perpendicular to the tangent direction & alignrf
			// with the circles (this has length vMag*vMag)
			XMVECTOR const xmPerpAxis = XMVector3Cross(xmPlaneNormal, xmV);

			float const radius = vMag * 0.25f;

			XMVECTOR const xmCenterToP0 = XMVectorScale(xmV, -0.25f);

			// interpolate across two semi-circles
			XMStoreFloat3A(&rArc0.center, XMVectorSubtract(xmStart, xmCenterToP0));
			XMStoreFloat3A(&rArc0.axis[0], xmCenterToP0);
			XMStoreFloat3A(&rArc0.axis[1], XMVectorScale(xmPerpAxis, radius*invMagSq));
			rArc0.radius = radius;
			rArc0.angle = XM_PI;
			rArc0.length = XM_PI * radius;
			
			XMStoreFloat3A(&rArc1.center, XMVectorSubtract(xmEnd, xmCenterToP0));
			XMStoreFloat3A(&rArc1.axis[0], XMVectorNegate(xmCenterToP0));
			XMStoreFloat3A(&rArc1.axis[1], XMVectorScale(xmPerpAxis, -radius * invMagSq));
			rArc1.radius = radius;
			rArc1.angle = XM_PI;
			rArc1.length = XM_PI * radius;

			return;
		}
		else {

			// compute the distance value for equal tangents
			d = vDotV / (4.0f * vDotT1);

		}
	}
	else {

		// use the positive result from quadratic formula
		const float discriminant = SFM::__fma(vDotT, vDotT, denominator * vDotV);
		d = (-vDotT + SFM::__sqrt(discriminant)) / denominator;
	}

	// compute the connection point (the midpoint)
	XMVECTOR xmPM = XMVectorSubtract(xmStartTangent, xmEndTangent);
	xmPM = XMVectorAdd(xmEnd, XMVectorScale(xmPM, d));
	xmPM = XMVectorAdd(xmPM, xmStart);
	xmPM = XMVectorScale(xmPM, 0.5f);

	// compute the vectors from end points to mid point
	XMVECTOR const xmP0ToPM = XMVectorSubtract(xmPM, xmStart);
	XMVECTOR const xmP1ToPM = XMVectorSubtract(xmPM, xmEnd);

	float radiusStart(0.0f), radiusEnd(0.0f);
	float angleStart(0.0f), angleEnd(0.0f);

	internal::biarc_computeArc(rArc0.center, radiusStart, angleStart,
							   xmStart, xmStartTangent, xmP0ToPM);

	internal::biarc_computeArc(rArc1.center, radiusEnd, angleEnd,
							   xmEnd, xmEndTangent, xmP1ToPM);

	// use the longer path around the circle if d is negative
	if (d < 0.0f) {

		angleStart = SFM::__fms((angleStart > 0.0f ? 1.0f : -1.0f), XM_2PI, angleStart);
		angleStart = SFM::__fms((angleEnd > 0.0f ? 1.0f : -1.0f), XM_2PI, angleEnd);
	}

	// output the arcs
	// (radius == 0.0f when arc is a straight line)
	XMStoreFloat3A(&rArc0.axis[0], XMVectorSubtract(xmStart, XMLoadFloat3A(&rArc0.center)));
	XMStoreFloat3A(&rArc0.axis[1], XMVectorScale(xmStartTangent, radiusStart));
	rArc0.radius = radiusStart;
	rArc0.angle = angleStart;
	rArc0.length = ((0.0f == radiusStart) ? XMVectorGetX(XMVector3Length(xmP0ToPM)) : SFM::abs(radiusStart * angleStart));

	XMStoreFloat3A(&rArc1.axis[0], XMVectorSubtract(xmEnd, XMLoadFloat3A(&rArc1.center)));
	XMStoreFloat3A(&rArc1.axis[1], XMVectorScale(xmEndTangent, -radiusEnd));
	rArc1.radius = radiusEnd;
	rArc1.angle = angleEnd;
	rArc1.length = ((0.0f == radiusEnd) ? XMVectorGetX(XMVector3Length(xmP1ToPM)) : SFM::abs(radiusEnd * angleEnd));

}

STATIC_INLINE_PURE XMVECTOR const __vectorcall biarc_interpolate(biarc_t const& __restrict rArc0, biarc_t const& __restrict rArc1, float const tNorm)
{
	// tNorm should be between 0.0f and 1.0f (inclusive)
	constexpr float const EPSILON = 0.00001f;

	// pull common values from memory to registers
	float const length0(rArc0.length), length1(rArc1.length);

	// compute distance along biarc
	float const totalDistance = length0 + length1;
	float const fracDistance = tNorm * totalDistance;

	if (fracDistance < length0) {

		if (length0 - EPSILON < 0.0f) {

			// output the end point
			return(XMVectorAdd(XMLoadFloat3A(&rArc0.center), XMLoadFloat3A(&rArc0.axis[0])));
		}
		else {

			float const arcFrac = fracDistance / length0;
			if (0.0f == rArc0.radius) {

				// interpolate along the line
				return(XMVectorAdd(XMLoadFloat3A(&rArc0.center), XMVectorScale(XMLoadFloat3A(&rArc0.axis[0]),
					   -arcFrac * 2.0f + 1.0f)));
			}
			else {

				// interpolate along the arc
				float const angle(rArc0.angle * arcFrac);
				float c;
				float const s(SFM::sincos(&c, angle));

				XMVECTOR const xmR = XMVectorAdd(XMLoadFloat3A(&rArc0.center), XMVectorScale(XMLoadFloat3A(&rArc0.axis[0]),
										         c));
				return(XMVectorAdd(xmR, XMVectorScale(XMLoadFloat3A(&rArc0.axis[1]),
								   s)));
			}
		}
	}
	else {

		if (length1 - EPSILON < 0.0f) {

			// output the end point
			return(XMVectorAdd(XMLoadFloat3A(&rArc1.center), XMLoadFloat3A(&rArc1.axis[1])));
		}
		else {

			float const arcFrac = (fracDistance - length0) / length1;
			if (0.0f == rArc1.radius) {

				// interpolate along the line
				return(XMVectorAdd(XMLoadFloat3A(&rArc1.center), XMVectorScale(XMLoadFloat3A(&rArc1.axis[0]),
					   arcFrac * 2.0f + 1.0f)));
			}
			else {

				// interpolate along the arc
				float const angle(rArc1.angle * (1.0f - arcFrac));
				float c;
				float const s(SFM::sincos(&c, angle));

				XMVECTOR const xmR = XMVectorAdd(XMLoadFloat3A(&rArc1.center), XMVectorScale(XMLoadFloat3A(&rArc1.axis[0]),
												 c));
				return(XMVectorAdd(xmR, XMVectorScale(XMLoadFloat3A(&rArc1.axis[1]),
								   s)));
			}
		}
	}

	return(XMVectorZero()); // not reachable
}

#endif // BIARC_H


