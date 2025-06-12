#pragma once
#define NOMINMAX
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>

namespace DX12Engine
{
    class PhysicsComponent;

	enum class CollisionMeshType
	{
		None,
		Sphere,
		Box,
		Plane
	};

	struct OBB
	{
		DirectX::XMVECTOR Center;
		DirectX::XMVECTOR Axis[3];
		DirectX::XMVECTOR Extents;
        float Width;
		float Height;
	};

	struct Sphere
	{
		DirectX::XMVECTOR Center;
		float Radius;
	};

    struct Plane
    {
        DirectX::XMVECTOR Center;
        DirectX::XMVECTOR Normal;
    };

	struct ContactPoint
	{
		DirectX::XMVECTOR Point;
		DirectX::XMVECTOR Normal;
		float PenetrationDepth;
	};

	struct ContactManifold
	{
		std::vector<ContactPoint> Contacts;
        DirectX::XMVECTOR Normal = DirectX::XMVectorZero();
		float PenetrationDepth = 0.0f;
		PhysicsComponent* A = nullptr;
		PhysicsComponent* B = nullptr;

		void AddContact(const ContactPoint& contact)
		{
			Contacts.emplace_back(contact);
			Normal = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(Normal, contact.Normal));
			PenetrationDepth += contact.PenetrationDepth;
		}
	};

    struct CollisionMesh
    {
		CollisionMeshType Type = CollisionMeshType::None;
		OBB OBBData;
		Sphere SphereData;
		Plane PlaneData;

		bool Intersects(CollisionMesh& other, ContactManifold* outContact)
		{
			if (Type == CollisionMeshType::Box && other.Type == CollisionMeshType::Box)
				return OBBvsOBB(OBBData, other.OBBData, outContact);
			else if (Type == CollisionMeshType::Sphere && other.Type == CollisionMeshType::Box)
				return SphereVsOBB(SphereData, other.OBBData, outContact);
			else if (Type == CollisionMeshType::Box && other.Type == CollisionMeshType::Sphere)
				return SphereVsOBB(other.SphereData, OBBData, outContact);
            else if (Type == CollisionMeshType::Sphere && other.Type == CollisionMeshType::Sphere)
                return SphereVsSphere(SphereData, other.SphereData, outContact);
            else if (Type == CollisionMeshType::Plane && other.Type == CollisionMeshType::Sphere)
                return SphereVsPlane(other.SphereData, PlaneData, outContact);
            else if (Type == CollisionMeshType::Sphere && other.Type == CollisionMeshType::Plane)
                return SphereVsPlane(SphereData, other.PlaneData, outContact);
            else if (Type == CollisionMeshType::Plane && other.Type == CollisionMeshType::Box)
                return BoxVsPlaneContacts(other.OBBData, PlaneData, outContact);
            else if (Type == CollisionMeshType::Box && other.Type == CollisionMeshType::Plane)
                return BoxVsPlaneContacts(OBBData, other.PlaneData, outContact);
			return false;
		}

        bool BoxVsPlaneContacts(OBB& box, const Plane& plane, ContactManifold* outManifold) 
        {
            if (OBBVsPlane(box, plane, outManifold))
            {
                int refFace = GetReferenceFaceIndex(box, plane.Normal);

                std::vector<DirectX::XMVECTOR> faceVerts;
                GetBoxFaceVertices(box, refFace, faceVerts);

                ClipFaceAgainstPlane(faceVerts, plane, *outManifold);
                outManifold->PenetrationDepth /= outManifold->Contacts.size();
                return true;
            }
            return false;
        }

		bool OBBvsOBB(const OBB& a, const OBB& b, ContactManifold* contact = nullptr)
		{
            constexpr float EPSILON = 1e-6f;
            float minOverlap = FLT_MAX;
            DirectX::XMVECTOR smallestAxis = DirectX::XMVectorZero();

            DirectX::XMVECTOR axes[15];
            int axisCount = 0;

            for (int i = 0; i < 3; ++i)
                axes[axisCount++] = a.Axis[i];

            for (int i = 0; i < 3; ++i)
                axes[axisCount++] = b.Axis[i];

            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j) 
                {
                    DirectX::XMVECTOR axis = DirectX::XMVector3Cross(a.Axis[i], b.Axis[j]);
                    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(axis)) > EPSILON)
                        axes[axisCount++] = DirectX::XMVector3Normalize(axis);
                }
            }

            DirectX::XMVECTOR d = DirectX::XMVectorSubtract(b.Center, a.Center);

            for (int i = 0; i < axisCount; ++i) 
            {
                DirectX::XMVECTOR axis = axes[i];

                float projA = 0.0f;
                float projB = 0.0f;
                for (int j = 0; j < 3; ++j) 
                {
                    projA += fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(a.Axis[j], axis))) * DirectX::XMVectorGetByIndex(a.Extents, j);
                    projB += fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(b.Axis[j], axis))) * DirectX::XMVectorGetByIndex(b.Extents, j);
                }

                float distance = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, axis)));
                float overlap = projA + projB - distance;

                if (overlap <= 0.0f)
                    return false;

                if (overlap < minOverlap) 
                {
                    minOverlap = overlap;
                    smallestAxis = DirectX::XMVector3Less(DirectX::XMVector3Dot(axis, d), DirectX::XMVectorZero()) ? DirectX::XMVectorNegate(axis) : axis;
                }
            }

            ContactPoint contactPoint;
			contactPoint.Normal = smallestAxis;
			contactPoint.PenetrationDepth = minOverlap;
			contactPoint.Point = DirectX::XMVectorAdd(a.Center, DirectX::XMVectorScale(smallestAxis, 0.5f * minOverlap));

            if (contact) 
				contact->AddContact(contactPoint);

            return true;
		}

        bool SphereVsOBB(const Sphere& sphere, const OBB& obb, ContactManifold* contact = nullptr)
        {
            DirectX::XMVECTOR d = DirectX::XMVectorSubtract(sphere.Center, obb.Center);
            DirectX::XMVECTOR closest = obb.Center;

            for (int i = 0; i < 3; ++i) 
            {
                float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, obb.Axis[i]));
                float clamped = (std::max)(-DirectX::XMVectorGetByIndex(obb.Extents, i), (std::min)(dist, DirectX::XMVectorGetByIndex(obb.Extents, i)));
                closest = DirectX::XMVectorAdd(closest, DirectX::XMVectorScale(obb.Axis[i], clamped));
            }

            DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(sphere.Center, closest);
            float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff));

            if (distSq > sphere.Radius * sphere.Radius)
                return false;

            float dist = sqrtf(distSq);
            ContactPoint contactPoint;
            contactPoint.Normal = dist > 1e-5f ? DirectX::XMVectorScale(diff, 1.0f / dist) : DirectX::XMVectorSet(0, 1, 0, 0);
            contactPoint.PenetrationDepth = sphere.Radius - dist;
            contactPoint.Point = closest;

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

        bool SphereVsSphere(const Sphere& a, const Sphere& b, ContactManifold* contact = nullptr)
        {
            DirectX::XMVECTOR delta = DirectX::XMVectorSubtract(b.Center, a.Center);
            float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(delta));
            float rSum = a.Radius + b.Radius;

            if (distSq >= rSum * rSum)
                return false;

            float dist = sqrtf(distSq);
            DirectX::XMVECTOR normal = dist > 1e-5f ? DirectX::XMVectorScale(delta, 1.0f / dist) : DirectX::XMVectorSet(0, 1, 0, 0);

            ContactPoint contactPoint;
            contactPoint.Normal = normal;
            contactPoint.PenetrationDepth = rSum - dist;
            contactPoint.Point = DirectX::XMVectorAdd(a.Center, DirectX::XMVectorScale(normal, a.Radius));

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

        bool OBBVsPlane(OBB& obb, const Plane& plane, ContactManifold* contact = nullptr)
        {
            float r = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                float proj = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(obb.Axis[i], plane.Normal)));
                r += DirectX::XMVectorGetX(obb.Extents) * proj;
                obb.Extents = DirectX::XMVectorSwizzle<1, 2, 0, 3>(obb.Extents);
            }

            float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(obb.Center, plane.Center), plane.Normal));

            if (dist > r)
                return false;

            ContactPoint contactPoint;
            contactPoint.Normal = plane.Normal;
            contactPoint.PenetrationDepth = r - dist;
            contactPoint.Point = DirectX::XMVectorSubtract(obb.Center, DirectX::XMVectorScale(plane.Normal, dist));

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

		bool SphereVsPlane(const Sphere& sphere, const Plane& plane, ContactManifold* contact = nullptr)
		{
			float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(sphere.Center, plane.Center), plane.Normal));

			if (dist > sphere.Radius)
				return false;

            ContactPoint contactPoint;
            contactPoint.Normal = plane.Normal;
            contactPoint.PenetrationDepth = sphere.Radius - dist;
            contactPoint.Point = DirectX::XMVectorSubtract(sphere.Center, DirectX::XMVectorScale(plane.Normal, dist));

            if (contact)
                contact->AddContact(contactPoint);

			return true;
		}

        int GetReferenceFaceIndex(const OBB& box, const DirectX::XMVECTOR& planeNormal) 
        {
            DirectX::XMVECTOR right = box.Axis[0];
            DirectX::XMVECTOR up = box.Axis[1];
            DirectX::XMVECTOR forward = box.Axis[2];

            DirectX::XMVECTOR axes[3] = { right, up, forward };
            float maxDot = -FLT_MAX;
            int bestAxis = 0;

            for (int i = 0; i < 3; ++i) 
            {
                float dot = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(axes[i], planeNormal)));
                if (dot > maxDot) 
                {
                    maxDot = dot;
                    bestAxis = i;
                }
            }
            return bestAxis;
        }

        void GetBoxFaceVertices(const OBB& box, int faceIndex, std::vector<DirectX::XMVECTOR>& outVerts) 
        {
            DirectX::XMVECTOR center = box.Center;
            DirectX::XMVECTOR ex = DirectX::XMVectorReplicate(DirectX::XMVectorGetX(box.Extents));
            DirectX::XMVECTOR ey = DirectX::XMVectorReplicate(DirectX::XMVectorGetY(box.Extents));
            DirectX::XMVECTOR ez = DirectX::XMVectorReplicate(DirectX::XMVectorGetZ(box.Extents));

            DirectX::XMVECTOR right = box.Axis[0];
            DirectX::XMVECTOR up = box.Axis[1];
            DirectX::XMVECTOR fwd = box.Axis[2];

            DirectX::XMVECTOR normal, axisA, axisB;
            if (faceIndex == 0) 
            {
                normal = right;
                axisA = up;
                axisB = fwd;
                ex = DirectX::XMVectorNegate(ex);
            }
            else if (faceIndex == 1) 
            {
                normal = up;
                axisA = right;
                axisB = fwd;
                ey = DirectX::XMVectorNegate(ey);
            }
            else 
            {
                normal = fwd;
                axisA = right;
                axisB = up;
                ez = DirectX::XMVectorNegate(ez);
            }

            DirectX::XMVECTOR faceCenter = DirectX::XMVectorAdd(center, DirectX::XMVectorScale(normal, box.Extents.m128_f32[faceIndex]));

            outVerts.clear();
            outVerts.push_back(DirectX::XMVectorAdd(faceCenter, DirectX::XMVectorAdd(DirectX::XMVectorMultiply(axisA, box.Extents), DirectX::XMVectorMultiply(axisB, box.Extents))));
            outVerts.push_back(DirectX::XMVectorSubtract(faceCenter, DirectX::XMVectorAdd(DirectX::XMVectorMultiply(axisA, box.Extents), DirectX::XMVectorMultiply(axisB, box.Extents))));
            outVerts.push_back(DirectX::XMVectorSubtract(faceCenter, DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(axisA, box.Extents), DirectX::XMVectorMultiply(axisB, box.Extents))));
            outVerts.push_back(DirectX::XMVectorAdd(faceCenter, DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(axisA, box.Extents), DirectX::XMVectorMultiply(axisB, box.Extents))));
        }

        void ClipFaceAgainstPlane(const std::vector<DirectX::XMVECTOR>& faceVerts, const Plane& plane, ContactManifold& outManifold) 
        {
            for (const auto& v : faceVerts) 
            {
                float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, DirectX::XMVectorSubtract(v, plane.Center)));
                if (dist <= 0.0f) 
                {
                    ContactPoint contact;
                    contact.Point = v;
                    contact.Normal = plane.Normal;
                    contact.PenetrationDepth = -dist;
                    outManifold.AddContact(contact);
                }
            }
        }
    };
}
