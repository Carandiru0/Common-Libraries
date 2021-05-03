//-------------------------------------------------------------------------------------
// DirectXCollision.aligned.h -- C++ Collision Math library
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//  
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma once
#endif

#ifndef DIRECTXCOLLISION_H
#define DIRECTXCOLLISION_H

#include <DirectXMath.h>

namespace DirectX
{

enum ContainmentType
{
    DISJOINT = 0,
    INTERSECTS = 1,
    CONTAINS = 2,
};

enum PlaneIntersectionType
{
    FRONT = 0,
    INTERSECTING = 1,
    BACK = 2,
};

struct BoundingBoxReferenceCenter;
struct BoundingOrientedBox;
struct BoundingFrustum;

#pragma warning(push)
#pragma warning(disable:4324 4820)

//-------------------------------------------------------------------------------------
// Bounding sphere
//-------------------------------------------------------------------------------------
struct BoundingSphereReferenceCenter
{
    XMFLOAT3A& Center;            // Center of the sphere.
    __declspec(align(16)) float Radius;               // Radius of the sphere.

    // Creators
	BoundingSphereReferenceCenter( XMFLOAT3A& center ) : Center(center), Radius( 1.f ) {}
	BoundingSphereReferenceCenter( _In_ XMFLOAT3A& center, _In_ float radius )
        : Center(center), Radius(radius) { assert( radius >= 0.f ); };
	BoundingSphereReferenceCenter( _In_ const BoundingSphereReferenceCenter& sp ) = delete;

    // Methods
	BoundingSphereReferenceCenter& operator=( _In_ const BoundingSphereReferenceCenter& sp ) = delete;

	void XM_CALLCONV Transform(_Out_ BoundingSphereReferenceCenter& Out, _In_ CXMMATRIX M) const;
	void XM_CALLCONV Transform(_Out_ BoundingSphereReferenceCenter& Out, _In_ float Scale, _In_ FXMVECTOR Rotation, _In_ FXMVECTOR Translation) const;
        // Transform the sphere

	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR Point) const;
	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
    ContainmentType Contains( _In_ const BoundingSphereReferenceCenter& sh ) const;
    ContainmentType Contains( _In_ const BoundingBoxReferenceCenter& box ) const;
    ContainmentType Contains( _In_ const BoundingOrientedBox& box ) const;
    ContainmentType Contains( _In_ const BoundingFrustum& fr ) const;

    bool Intersects( _In_ const BoundingSphereReferenceCenter& sh ) const;
    bool Intersects( _In_ const BoundingBoxReferenceCenter& box ) const;
    bool Intersects( _In_ const BoundingOrientedBox& box ) const;
    bool Intersects( _In_ const BoundingFrustum& fr ) const;
    
	bool XM_CALLCONV Intersects(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
        // Triangle-sphere test

	PlaneIntersectionType XM_CALLCONV Intersects(_In_ FXMVECTOR Plane) const;
        // Plane-sphere test
    
	bool XM_CALLCONV Intersects(_In_ FXMVECTOR Origin, _In_ FXMVECTOR Direction, _Out_ float& Dist) const;
        // Ray-sphere test

	ContainmentType XM_CALLCONV ContainedBy(_In_ FXMVECTOR Plane0, _In_ FXMVECTOR Plane1, _In_ FXMVECTOR Plane2,
                                 _In_ GXMVECTOR Plane3, _In_ CXMVECTOR Plane4, _In_ CXMVECTOR Plane5 ) const;
        // Test sphere against six planes (see BoundingFrustum::GetPlanes)

    // Static methods
	static void CreateMerged( _Out_ BoundingSphereReferenceCenter& Out, _In_ const BoundingSphereReferenceCenter& S1, _In_ const BoundingSphereReferenceCenter& S2 );

    static void CreateFromBoundingBox( _Out_ BoundingSphereReferenceCenter& Out, _In_ const BoundingBoxReferenceCenter& box );
    static void CreateFromBoundingBox( _Out_ BoundingSphereReferenceCenter& Out, _In_ const BoundingOrientedBox& box );

    static void CreateFromPoints( _Out_ BoundingSphereReferenceCenter& Out, _In_ size_t Count,
                                  _In_reads_bytes_(sizeof(XMFLOAT3)+Stride*(Count-1)) const XMFLOAT3* pPoints, _In_ size_t Stride );

    static void CreateFromFrustum( _Out_ BoundingSphereReferenceCenter& Out, _In_ const BoundingFrustum& fr );

};

struct BoundingSphere : public BoundingSphereReferenceCenter
{
	XMFLOAT3A InstanceCenter;

	// Creators
	BoundingSphere() : BoundingSphereReferenceCenter( InstanceCenter ) {}
	BoundingSphere( _In_ const XMFLOAT3A& center, _In_ float radius )
		: InstanceCenter( center ), BoundingSphereReferenceCenter( InstanceCenter, radius ) {
		assert( radius >= 0.f );
	};
	
	BoundingSphere( _In_ const BoundingSphere& sp )
		: InstanceCenter( sp.Center ), BoundingSphereReferenceCenter( InstanceCenter, sp.Radius ) {}
	BoundingSphere( _In_ const BoundingSphereReferenceCenter& sp )
		: InstanceCenter( sp.Center ), BoundingSphereReferenceCenter( InstanceCenter, sp.Radius ) {}
	 // Methods
	BoundingSphere& operator=( _In_ const BoundingSphere& sp ) { InstanceCenter = sp.Center; Radius = sp.Radius; return *this; }
};
//-------------------------------------------------------------------------------------
// Axis-aligned bounding box
//-------------------------------------------------------------------------------------
struct BoundingBoxReferenceCenter
{
    static const size_t CORNER_COUNT = 8;

    XMFLOAT3A& Center;            // Center of the box.
    XMFLOAT3A Extents;           // Distance from the center to each side.

    // Creators
	BoundingBoxReferenceCenter( _In_ XMFLOAT3A& center ) : Center(center), Extents( 1.f, 1.f, 1.f ) {}
	BoundingBoxReferenceCenter( _In_ XMFLOAT3A& center, _In_ const XMFLOAT3A& extents )
        : Center(center), Extents(extents) { assert(extents.x >= 0 && extents.y >= 0 && extents.z >= 0); }
	BoundingBoxReferenceCenter( _In_ const BoundingBoxReferenceCenter& box ) = delete;
    
    // Methods
    BoundingBoxReferenceCenter& operator=( _In_ const BoundingBoxReferenceCenter& box) = delete;

	void XM_CALLCONV Transform(_Out_ BoundingBoxReferenceCenter& Out, _In_ CXMMATRIX M) const;
	void XM_CALLCONV Transform(_Out_ BoundingBoxReferenceCenter& Out, _In_ float Scale, _In_ FXMVECTOR Rotation, _In_ FXMVECTOR Translation) const;

    void GetCorners( _Out_writes_(8) XMFLOAT3A* Corners ) const;
        // Gets the 8 corners of the box

	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR Point) const;
	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
    ContainmentType Contains( _In_ const BoundingSphereReferenceCenter& sh ) const;
    ContainmentType Contains( _In_ const BoundingBoxReferenceCenter& box ) const;
    ContainmentType Contains( _In_ const BoundingOrientedBox& box ) const;
    ContainmentType Contains( _In_ const BoundingFrustum& fr ) const;
    
    bool Intersects( _In_ const BoundingSphereReferenceCenter& sh ) const;
    bool Intersects( _In_ const BoundingBoxReferenceCenter& box ) const;
    bool Intersects( _In_ const BoundingOrientedBox& box ) const;
    bool Intersects( _In_ const BoundingFrustum& fr ) const;

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
        // Triangle-Box test

	PlaneIntersectionType XM_CALLCONV Intersects(_In_ FXMVECTOR Plane) const;
        // Plane-box test

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR Origin, _In_ FXMVECTOR Direction, _Out_ float& Dist) const;
        // Ray-Box test

	ContainmentType XM_CALLCONV ContainedBy(_In_ FXMVECTOR Plane0, _In_ FXMVECTOR Plane1, _In_ FXMVECTOR Plane2,
                                 _In_ GXMVECTOR Plane3, _In_ CXMVECTOR Plane4, _In_ CXMVECTOR Plane5 ) const;
        // Test box against six planes (see BoundingFrustum::GetPlanes)

    // Static methods
    static void CreateMerged( _Out_ BoundingBoxReferenceCenter& Out, _In_ const BoundingBoxReferenceCenter& b1, _In_ const BoundingBoxReferenceCenter& b2 );

    static void CreateFromSphere( _Out_ BoundingBoxReferenceCenter& Out, _In_ const BoundingSphere& sh );

	static void XM_CALLCONV CreateFromPoints(_Out_ BoundingBoxReferenceCenter& Out, _In_ FXMVECTOR pt1, _In_ FXMVECTOR pt2);
    static void CreateFromPoints( _Out_ BoundingBoxReferenceCenter& Out, _In_ size_t Count,
                                  _In_reads_bytes_(sizeof(XMFLOAT3)+Stride*(Count-1)) const XMFLOAT3* pPoints, _In_ size_t Stride );
};

struct BoundingBox : public BoundingBoxReferenceCenter
{
	XMFLOAT3A InstanceCenter;

	// Creators
	BoundingBox() : BoundingBoxReferenceCenter( InstanceCenter ) {}
	BoundingBox( _In_ const XMFLOAT3A& center, _In_ const XMFLOAT3A& extents )
		: InstanceCenter( center ), BoundingBoxReferenceCenter( InstanceCenter, extents ) {
	};
	BoundingBox( _In_ const BoundingBox& sp )
		: InstanceCenter( sp.Center ), BoundingBoxReferenceCenter( InstanceCenter, sp.Extents ) {}
	BoundingBox( _In_ const BoundingBoxReferenceCenter& sp )
		: InstanceCenter( sp.Center ), BoundingBoxReferenceCenter( InstanceCenter, sp.Extents ) {}

	 // Methods
	BoundingBox& operator=( _In_ const BoundingBox& sp ) { InstanceCenter = sp.Center; Extents = sp.Extents; return *this; }

};
//-------------------------------------------------------------------------------------
// Oriented bounding box
//-------------------------------------------------------------------------------------
struct BoundingOrientedBox
{
    static const size_t CORNER_COUNT = 8;

    XMFLOAT3A Center;            // Center of the box.
    XMFLOAT3A Extents;           // Distance from the center to each side.
    XMFLOAT4A Orientation;       // Unit quaternion representing rotation (box -> world).

    // Creators
    BoundingOrientedBox() : Center(0,0,0), Extents( 1.f, 1.f, 1.f ), Orientation(0,0,0, 1.f ) {}
    BoundingOrientedBox( _In_ const XMFLOAT3A& _Center, _In_ const XMFLOAT3A& _Extents, _In_ const XMFLOAT4A& _Orientation )
        : Center(_Center), Extents(_Extents), Orientation(_Orientation)
    {
        assert(_Extents.x >= 0 && _Extents.y >= 0 && _Extents.z >= 0);
    }
    BoundingOrientedBox( _In_ const BoundingOrientedBox& box )
        : Center(box.Center), Extents(box.Extents), Orientation(box.Orientation) {}

    // Methods
    BoundingOrientedBox& operator=( _In_ const BoundingOrientedBox& box ) { Center = box.Center; Extents = box.Extents; Orientation = box.Orientation; return *this; }

	void XM_CALLCONV Transform(_Out_ BoundingOrientedBox& Out, _In_ CXMMATRIX M) const;
	void XM_CALLCONV Transform(_Out_ BoundingOrientedBox& Out, _In_ float Scale, _In_ FXMVECTOR Rotation, _In_ FXMVECTOR Translation) const;

    void GetCorners( _Out_writes_(8) XMFLOAT3A* Corners ) const;
        // Gets the 8 corners of the box

	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR Point) const;
	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
    ContainmentType Contains( _In_ const BoundingSphereReferenceCenter& sh ) const;
    ContainmentType Contains( _In_ const BoundingBoxReferenceCenter& box ) const;
    ContainmentType Contains( _In_ const BoundingOrientedBox& box ) const;
    ContainmentType Contains( _In_ const BoundingFrustum& fr ) const;

    bool Intersects( _In_ const BoundingSphereReferenceCenter& sh ) const;
    bool Intersects( _In_ const BoundingBoxReferenceCenter& box ) const;
    bool Intersects( _In_ const BoundingOrientedBox& box ) const;
    bool Intersects( _In_ const BoundingFrustum& fr ) const;

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
        // Triangle-OrientedBox test

	PlaneIntersectionType XM_CALLCONV Intersects(_In_ FXMVECTOR Plane) const;
        // Plane-OrientedBox test
    
	bool XM_CALLCONV Intersects(_In_ FXMVECTOR Origin, _In_ FXMVECTOR Direction, _Out_ float& Dist) const;
        // Ray-OrientedBox test

	ContainmentType XM_CALLCONV ContainedBy(_In_ FXMVECTOR Plane0, _In_ FXMVECTOR Plane1, _In_ FXMVECTOR Plane2,
                                 _In_ GXMVECTOR Plane3, _In_ CXMVECTOR Plane4, _In_ CXMVECTOR Plane5 ) const;
        // Test OrientedBox against six planes (see BoundingFrustum::GetPlanes)

    // Static methods
    static void CreateFromBoundingBox( _Out_ BoundingOrientedBox& Out, _In_ const BoundingBoxReferenceCenter& box );

    static void CreateFromPoints( _Out_ BoundingOrientedBox& Out, _In_ size_t Count,
                                  _In_reads_bytes_(sizeof(XMFLOAT3)+Stride*(Count-1)) const XMFLOAT3* pPoints, _In_ size_t Stride );
};

//-------------------------------------------------------------------------------------
// Bounding frustum
//-------------------------------------------------------------------------------------
struct BoundingFrustum
{
    static const size_t CORNER_COUNT = 8;

    XMFLOAT3A Origin;            // Origin of the frustum (and projection).
    XMFLOAT4A Orientation;       // Quaternion representing rotation.

    float RightSlope;           // Positive X slope (X/Z).
    float LeftSlope;            // Negative X slope.
    float TopSlope;             // Positive Y slope (Y/Z).
    float BottomSlope;          // Negative Y slope.
    float Near, Far;            // Z of the near plane and far plane.

    // Creators
    BoundingFrustum() : Origin(0,0,0), Orientation(0,0,0, 1.f), RightSlope( 1.f ), LeftSlope( -1.f ),
                        TopSlope( 1.f ), BottomSlope( -1.f ), Near(0), Far( 1.f ) {}
    BoundingFrustum( _In_ const XMFLOAT3A& _Origin, _In_ const XMFLOAT4A& _Orientation,
                     _In_ float _RightSlope, _In_ float _LeftSlope, _In_ float _TopSlope, _In_ float _BottomSlope,
                     _In_ float _Near, _In_ float _Far )
        : Origin(_Origin), Orientation(_Orientation),
          RightSlope(_RightSlope), LeftSlope(_LeftSlope), TopSlope(_TopSlope), BottomSlope(_BottomSlope),
          Near(_Near), Far(_Far) { assert( _Near <= _Far ); }
    BoundingFrustum( _In_ const BoundingFrustum& fr )
        : Origin(fr.Origin), Orientation(fr.Orientation), RightSlope(fr.RightSlope), LeftSlope(fr.LeftSlope),
          TopSlope(fr.TopSlope), BottomSlope(fr.BottomSlope), Near(fr.Near), Far(fr.Far) {}
    BoundingFrustum( _In_ CXMMATRIX Projection ) { CreateFromMatrixLH( *this, Projection ); }

    // Methods
    BoundingFrustum& operator=( _In_ const BoundingFrustum& fr ) { Origin=fr.Origin; Orientation=fr.Orientation;
                                                                   RightSlope=fr.RightSlope; LeftSlope=fr.LeftSlope;
                                                                   TopSlope=fr.TopSlope; BottomSlope=fr.BottomSlope;
                                                                   Near=fr.Near; Far=fr.Far; return *this; }

	void XM_CALLCONV Transform(_Out_ BoundingFrustum& Out, _In_ CXMMATRIX M) const;
	void XM_CALLCONV Transform(_Out_ BoundingFrustum& Out, _In_ float Scale, _In_ FXMVECTOR Rotation, _In_ FXMVECTOR Translation) const;

    void GetCorners( _Out_writes_(8) XMFLOAT3A* Corners ) const;
        // Gets the 8 corners of the frustum

	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR Point) const;
	ContainmentType XM_CALLCONV Contains(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
    ContainmentType Contains( _In_ const BoundingSphereReferenceCenter& sp ) const;
    ContainmentType Contains( _In_ const BoundingBoxReferenceCenter& box ) const;
    ContainmentType Contains( _In_ const BoundingOrientedBox& box ) const;
    ContainmentType Contains( _In_ const BoundingFrustum& fr ) const;
        // Frustum-Frustum test

    bool Intersects( _In_ const BoundingSphereReferenceCenter& sh ) const;
    bool Intersects( _In_ const BoundingBoxReferenceCenter& box ) const;
    bool Intersects( _In_ const BoundingOrientedBox& box ) const;
    bool Intersects( _In_ const BoundingFrustum& fr ) const;

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2) const;
        // Triangle-Frustum test

	PlaneIntersectionType XM_CALLCONV Intersects(_In_ FXMVECTOR Plane) const;
        // Plane-Frustum test

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR Origin, _In_ FXMVECTOR Direction, _Out_ float& Dist) const;
        // Ray-Frustum test

	ContainmentType XM_CALLCONV ContainedBy(_In_ FXMVECTOR Plane0, _In_ FXMVECTOR Plane1, _In_ FXMVECTOR Plane2,
                                 _In_ GXMVECTOR Plane3, _In_ CXMVECTOR Plane4, _In_ CXMVECTOR Plane5 ) const;
        // Test frustum against six planes (see BoundingFrustum::GetPlanes)

    void GetPlanes( _Out_opt_ XMVECTOR* NearPlane, _Out_opt_ XMVECTOR* FarPlane, _Out_opt_ XMVECTOR* RightPlane,
                    _Out_opt_ XMVECTOR* LeftPlane, _Out_opt_ XMVECTOR* TopPlane, _Out_opt_ XMVECTOR* BottomPlane ) const;
        // Create 6 Planes representation of Frustum

    // Static methods
	static void XM_CALLCONV CreateFromMatrixLH(_Out_ BoundingFrustum& Out, _In_ CXMMATRIX Projection);
	static void XM_CALLCONV CreateFromMatrixRH( _Out_ BoundingFrustum& Out, _In_ CXMMATRIX Projection );

};

//-----------------------------------------------------------------------------
// Triangle intersection testing routines.
//-----------------------------------------------------------------------------
namespace TriangleTests
{
	bool XM_CALLCONV Intersects(_In_ FXMVECTOR Origin, _In_ FXMVECTOR Direction, _In_ FXMVECTOR V0, _In_ GXMVECTOR V1, _In_ CXMVECTOR V2, _Out_ float& Dist);
        // Ray-Triangle

	bool XM_CALLCONV Intersects(_In_ FXMVECTOR A0, _In_ FXMVECTOR A1, _In_ FXMVECTOR A2, _In_ GXMVECTOR B0, _In_ CXMVECTOR B1, _In_ CXMVECTOR B2);
        // Triangle-Triangle

	PlaneIntersectionType XM_CALLCONV Intersects(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2, _In_ GXMVECTOR Plane);
        // Plane-Triangle

	ContainmentType XM_CALLCONV ContainedBy(_In_ FXMVECTOR V0, _In_ FXMVECTOR V1, _In_ FXMVECTOR V2,
                                 _In_ GXMVECTOR Plane0, _In_ CXMVECTOR Plane1, _In_ CXMVECTOR Plane2,
                                 _In_ CXMVECTOR Plane3, _In_ CXMVECTOR Plane4, _In_ CXMVECTOR Plane5 );
        // Test a triangle against six planes at once (see BoundingFrustum::GetPlanes)
};

#pragma warning(pop)

/****************************************************************************
 *
 * Implementation
 *
 ****************************************************************************/

#pragma warning(push)
#pragma warning(disable : 4068 4616 6001)

#pragma prefast(push)
#pragma prefast(disable : 25000, "FXMVECTOR is 16 bytes")

#include "DirectXCollision.aligned.inl"

#pragma prefast(pop)
#pragma warning(pop)

}; // namespace DirectX

#else
#error "DirectXCollision Header Duplicate (nonaligned) Include"
#endif

