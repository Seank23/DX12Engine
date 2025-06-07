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
		std::vector<std::shared_ptr<ContactPoint>> Contacts;
        DirectX::XMVECTOR Normal = DirectX::XMVectorZero();
		float PenetrationDepth = 0.0f;
		PhysicsComponent* A = nullptr;
		PhysicsComponent* B = nullptr;

		void AddContact(std::shared_ptr<ContactPoint> contact)
		{
			Contacts.push_back(contact);
			Normal = DirectX::XMVector3Normalize(DirectX::XMVectorAdd(Normal, contact->Normal));
			PenetrationDepth += contact->PenetrationDepth;
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
                return OBBVsPlane(other.OBBData, PlaneData, outContact);
            else if (Type == CollisionMeshType::Box && other.Type == CollisionMeshType::Plane)
                return OBBVsPlane(OBBData, other.PlaneData, outContact);
			return false;
		}

		bool OBBvsOBB(OBB& a, OBB& b, ContactManifold* contact = nullptr)
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

            std::shared_ptr<ContactPoint> contactPoint = std::make_shared<ContactPoint>();
			contactPoint->Normal = smallestAxis;
			contactPoint->PenetrationDepth = minOverlap;
			contactPoint->Point = DirectX::XMVectorAdd(a.Center, DirectX::XMVectorScale(smallestAxis, 0.5f * minOverlap));

            if (contact) 
				contact->AddContact(contactPoint);

            return true;
		}

        bool SphereVsOBB(Sphere& sphere, OBB& obb, ContactManifold* contact = nullptr)
        {
            DirectX::XMVECTOR d = DirectX::XMVectorSubtract(sphere.Center, obb.Center);
            DirectX::XMVECTOR closest = obb.Center;

            for (int i = 0; i < 3; ++i) {
                float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, obb.Axis[i]));
                float clamped = (std::max)(-DirectX::XMVectorGetByIndex(obb.Extents, i), (std::min)(dist, DirectX::XMVectorGetByIndex(obb.Extents, i)));
                closest = DirectX::XMVectorAdd(closest, DirectX::XMVectorScale(obb.Axis[i], clamped));
            }

            DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(sphere.Center, closest);
            float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff));

            if (distSq > sphere.Radius * sphere.Radius)
                return false;

            float dist = sqrtf(distSq);
            std::shared_ptr<ContactPoint> contactPoint = std::make_shared<ContactPoint>();
            contactPoint->Normal = dist > 1e-5f ? DirectX::XMVectorScale(diff, 1.0f / dist) : DirectX::XMVectorSet(0, 1, 0, 0);
            contactPoint->PenetrationDepth = sphere.Radius - dist;
            contactPoint->Point = closest;

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

        bool SphereVsSphere(Sphere& a, Sphere& b, ContactManifold* contact = nullptr)
        {
            DirectX::XMVECTOR delta = DirectX::XMVectorSubtract(b.Center, a.Center);
            float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(delta));
            float rSum = a.Radius + b.Radius;

            if (distSq >= rSum * rSum)
                return false;

            float dist = sqrtf(distSq);
            DirectX::XMVECTOR normal = dist > 1e-5f ? DirectX::XMVectorScale(delta, 1.0f / dist) : DirectX::XMVectorSet(0, 1, 0, 0);

            std::shared_ptr<ContactPoint> contactPoint = std::make_shared<ContactPoint>();
            contactPoint->Normal = normal;
            contactPoint->PenetrationDepth = rSum - dist;
            contactPoint->Point = DirectX::XMVectorAdd(a.Center, DirectX::XMVectorScale(normal, a.Radius));

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

        bool OBBVsPlane(OBB& obb, Plane& plane, ContactManifold* contact = nullptr)
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

            std::shared_ptr<ContactPoint> contactPoint = std::make_shared<ContactPoint>();
            contactPoint->Normal = plane.Normal;
            contactPoint->PenetrationDepth = r - dist;
            contactPoint->Point = DirectX::XMVectorSubtract(obb.Center, DirectX::XMVectorScale(plane.Normal, dist));

            if (contact)
                contact->AddContact(contactPoint);

            return true;
        }

		bool SphereVsPlane(Sphere& sphere, Plane& plane, ContactManifold* contact = nullptr)
		{
			float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(sphere.Center, plane.Center), plane.Normal));

			if (dist > sphere.Radius)
				return false;

            std::shared_ptr<ContactPoint> contactPoint = std::make_shared<ContactPoint>();
            contactPoint->Normal = plane.Normal;
            contactPoint->PenetrationDepth = sphere.Radius - dist;
            contactPoint->Point = DirectX::XMVectorSubtract(sphere.Center, DirectX::XMVectorScale(plane.Normal, dist));

            if (contact)
                contact->AddContact(contactPoint);

			return true;
		}
    };
}
