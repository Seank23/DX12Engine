#pragma once
#define NOMINMAX
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <cstdint>
#include <functional>

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
        DirectX::XMVECTOR Tangent0;
        DirectX::XMVECTOR Tangent1;
        float HalfExtent0 = 0.0f;
        float HalfExtent1 = 0.0f;
    };

	struct ContactPoint
	{
		DirectX::XMVECTOR Point;
		DirectX::XMVECTOR Normal;
		float PenetrationDepth;
	};

	struct CachedContact
	{
		DirectX::XMVECTOR Point;
		float AccJn = 0.0f;
		float AccJt0 = 0.0f;
		float AccJt1 = 0.0f;
	};

	struct BodyPairKey
	{
		const PhysicsComponent* A;
		const PhysicsComponent* B;

		bool operator==(const BodyPairKey& other) const
		{
			return A == other.A && B == other.B;
		}
	};

	struct BodyPairHash
	{
		size_t operator()(const BodyPairKey& key) const
		{
			auto h1 = std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(key.A));
			auto h2 = std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(key.B));
			return h1 ^ (h2 * 2654435761u);
		}
	};

	struct CachedManifold
	{
		std::vector<CachedContact> Contacts;
		DirectX::XMVECTOR Normal = DirectX::XMVectorZero();
		DirectX::XMVECTOR Tangent0 = DirectX::XMVectorZero();
		DirectX::XMVECTOR Tangent1 = DirectX::XMVectorZero();
		int Age = 0;
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
			// Accumulate raw normal sum; normalise once after all contacts are added
			Normal = DirectX::XMVectorAdd(Normal, contact.Normal);
			PenetrationDepth += contact.PenetrationDepth;
		}

		void Finalise()
		{
			if (Contacts.empty()) return;
			float invN = 1.0f / static_cast<float>(Contacts.size());
			Normal = DirectX::XMVector3Normalize(Normal);
			PenetrationDepth *= invN;
		}
	};

    struct CollisionMesh
    {
		CollisionMeshType Type = CollisionMeshType::None;
		OBB OBBData;
		Sphere SphereData;
		Plane PlaneData;

		bool Intersects(const CollisionMesh& other, ContactManifold* outContact) const
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

        bool BoxVsPlaneContacts(const OBB& box, const Plane& plane, ContactManifold* outManifold) const 
        {
            if (!OBBVsPlane(box, plane))
                return false;

            float ex = DirectX::XMVectorGetX(box.Extents);
            float ey = DirectX::XMVectorGetY(box.Extents);
            float ez = DirectX::XMVectorGetZ(box.Extents);

            const float sx[8] = { -1, 1, 1,-1,-1, 1, 1,-1 };
            const float sy[8] = { -1,-1, 1, 1,-1,-1, 1, 1 };
            const float sz[8] = { -1,-1,-1,-1, 1, 1, 1, 1 };

            std::vector<ContactPoint> pendingContacts;
            std::vector<DirectX::XMVECTOR> pendingVertices;

            for (int i = 0; i < 8; ++i)
            {
                DirectX::XMVECTOR v = DirectX::XMVectorAdd(box.Center,
                    DirectX::XMVectorAdd(
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(box.Axis[0], sx[i] * ex),
                            DirectX::XMVectorScale(box.Axis[1], sy[i] * ey)
                        ),
                        DirectX::XMVectorScale(box.Axis[2], sz[i] * ez)
                    )
                );

                float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(plane.Normal, DirectX::XMVectorSubtract(v, plane.Center)));
                if (dist <= 0.0f)
                {
                    DirectX::XMVECTOR projectedPt = DirectX::XMVectorSubtract(v, DirectX::XMVectorScale(plane.Normal, dist));

                    DirectX::XMVECTOR offset = DirectX::XMVectorSubtract(projectedPt, plane.Center);
                    float proj0 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(offset, plane.Tangent0));
                    float proj1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(offset, plane.Tangent1));
                    bool wasClamped = false;
                    if (plane.HalfExtent0 > 0.0f)
                    {
                        float clamped0 = (std::max)(-plane.HalfExtent0, (std::min)(proj0, plane.HalfExtent0));
                        if (clamped0 != proj0) wasClamped = true;
                    }
                    if (plane.HalfExtent1 > 0.0f)
                    {
                        float clamped1 = (std::max)(-plane.HalfExtent1, (std::min)(proj1, plane.HalfExtent1));
                        if (clamped1 != proj1) wasClamped = true;
                    }

                    if (wasClamped) continue;

                    ContactPoint cp;
                    cp.Point = projectedPt;
                    cp.Normal = plane.Normal;
                    cp.PenetrationDepth = -dist;
                    pendingContacts.push_back(cp);
                    pendingVertices.push_back(v);
                }
            }

            if (pendingContacts.empty())
                return false;

            // For corner/edge contacts (1-2 vertices) use the actual
            // penetrating vertex as the contact point.  This gives the
            // solver the correct lever arm from the box centre so that
            // normal + friction impulses produce the right toppling torque.
            // For face landings (3-4 vertices) use the plane-projected
            // points � their symmetric layout prevents angular artefacts.
            const bool usePenetratingVertex = (pendingContacts.size() <= 2);
            for (size_t i = 0; i < pendingContacts.size(); ++i)
            {
                ContactPoint cp = pendingContacts[i];
                if (usePenetratingVertex)
                    cp.Point = pendingVertices[i];
                outManifold->AddContact(cp);
            }

            outManifold->Finalise();
            return true;
        }

		bool OBBvsOBB(const OBB& a, const OBB& b, ContactManifold* contact = nullptr) const
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
			{
				contact->AddContact(contactPoint);
				contact->Finalise();
			}

            return true;
		}

        bool SphereVsOBB(const Sphere& sphere, const OBB& obb, ContactManifold* contact = nullptr) const
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
			{
                contact->AddContact(contactPoint);
				contact->Finalise();
			}

            return true;
        }

        bool SphereVsSphere(const Sphere& a, const Sphere& b, ContactManifold* contact = nullptr) const
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
			{
                contact->AddContact(contactPoint);
				contact->Finalise();
			}

            return true;
        }

        bool OBBVsPlane(const OBB& obb, const Plane& plane) const
        {
            float r = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                float proj = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(obb.Axis[i], plane.Normal)));
                r += DirectX::XMVectorGetByIndex(obb.Extents, i) * proj;
            }

            float dist = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(obb.Center, plane.Center), plane.Normal));

            if (dist > r || dist < -r)
                return false;

            DirectX::XMVECTOR d = DirectX::XMVectorSubtract(obb.Center, plane.Center);
            if (plane.HalfExtent0 > 0.0f)
            {
                float boxR0 = 0.0f;
                for (int i = 0; i < 3; ++i)
                    boxR0 += fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(obb.Axis[i], plane.Tangent0))) * DirectX::XMVectorGetByIndex(obb.Extents, i);
                float d0 = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, plane.Tangent0)));
                if (d0 > plane.HalfExtent0 + boxR0)
                    return false;
            }
            if (plane.HalfExtent1 > 0.0f)
            {
                float boxR1 = 0.0f;
                for (int i = 0; i < 3; ++i)
                    boxR1 += fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(obb.Axis[i], plane.Tangent1))) * DirectX::XMVectorGetByIndex(obb.Extents, i);
                float d1 = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, plane.Tangent1)));
                if (d1 > plane.HalfExtent1 + boxR1)
                    return false;
            }

            return true;
        }

		bool SphereVsPlane(const Sphere& sphere, const Plane& plane, ContactManifold* contact = nullptr) const
		{
			// Find the closest point on the (possibly finite) plane to the sphere center.
			DirectX::XMVECTOR offset = DirectX::XMVectorSubtract(sphere.Center, plane.Center);
			float distN = DirectX::XMVectorGetX(DirectX::XMVector3Dot(offset, plane.Normal));

			if (distN > sphere.Radius || distN < -sphere.Radius)
				return false;

			// Project sphere center onto the plane surface, then clamp to bounds.
			float proj0 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(offset, plane.Tangent0));
			float proj1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(offset, plane.Tangent1));
			float clamped0 = proj0;
			float clamped1 = proj1;
			if (plane.HalfExtent0 > 0.0f)
				clamped0 = (std::max)(-plane.HalfExtent0, (std::min)(proj0, plane.HalfExtent0));
			if (plane.HalfExtent1 > 0.0f)
				clamped1 = (std::max)(-plane.HalfExtent1, (std::min)(proj1, plane.HalfExtent1));

			DirectX::XMVECTOR closest = DirectX::XMVectorAdd(plane.Center,
				DirectX::XMVectorAdd(
					DirectX::XMVectorScale(plane.Tangent0, clamped0),
					DirectX::XMVectorScale(plane.Tangent1, clamped1)
				)
			);

			DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(sphere.Center, closest);
			float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff));

			if (distSq > sphere.Radius * sphere.Radius)
				return false;

			float dist = sqrtf(distSq);
            ContactPoint contactPoint;
            contactPoint.Normal = dist > 1e-5f ? DirectX::XMVectorScale(diff, 1.0f / dist) : plane.Normal;
            contactPoint.PenetrationDepth = sphere.Radius - dist;
            contactPoint.Point = closest;

            if (contact)
			{
                contact->AddContact(contactPoint);
				contact->Finalise();
			}

			return true;
		}

        int GetReferenceFaceIndex(const OBB& box, const DirectX::XMVECTOR& planeNormal) const 
        {
            // Find the box face axis that is most anti-aligned with the plane normal.
            // That face is the one closest to (pointing toward) the plane surface.
            float maxDot = -FLT_MAX;
            int bestAxis = 0;
            int bestSign = -1;

            for (int i = 0; i < 3; ++i) 
            {
                float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(box.Axis[i], planeNormal));
                // Positive dot: axis points away from plane � face is on the far side
                // Negative dot: axis points toward plane � face is closest to plane
                float absDot = fabsf(dot);
                if (absDot > maxDot) 
                {
                    maxDot = absDot;
                    bestAxis = i;
                    // If dot is negative the face in the +axis direction faces the plane,
                    // if dot is positive the face in the -axis direction faces the plane.
                    bestSign = (dot >= 0.0f) ? -1 : 1;
                }
            }
            // Encode sign into upper bits: index + (sign==1 ? 3 : 0)
            return bestAxis + (bestSign == 1 ? 3 : 0);
        }

        void GetBoxFaceVertices(const OBB& box, int faceIndex, std::vector<DirectX::XMVECTOR>& outVerts) const 
        {
            // Decode axis index and which side the face is on
            bool positiveDir = faceIndex >= 3;
            int axisIdx = faceIndex % 3;

            float extents[3] = {
                DirectX::XMVectorGetX(box.Extents),
                DirectX::XMVectorGetY(box.Extents),
                DirectX::XMVectorGetZ(box.Extents)
            };

            // The two tangent axes are the other two axes
            int axisA = (axisIdx + 1) % 3;
            int axisB = (axisIdx + 2) % 3;

            // Walk to the face center: positive direction if positiveDir, else negative
            float faceSign = positiveDir ? 1.0f : -1.0f;
            DirectX::XMVECTOR faceCenter = DirectX::XMVectorAdd(
                box.Center,
                DirectX::XMVectorScale(box.Axis[axisIdx], faceSign * extents[axisIdx])
            );

            DirectX::XMVECTOR dA = DirectX::XMVectorScale(box.Axis[axisA], extents[axisA]);
            DirectX::XMVECTOR dB = DirectX::XMVectorScale(box.Axis[axisB], extents[axisB]);

            outVerts.clear();
            outVerts.push_back(DirectX::XMVectorAdd(     faceCenter, DirectX::XMVectorAdd(dA, dB)));
            outVerts.push_back(DirectX::XMVectorAdd(     faceCenter, DirectX::XMVectorSubtract(dA, dB)));
            outVerts.push_back(DirectX::XMVectorSubtract(faceCenter, DirectX::XMVectorAdd(dA, dB)));
            outVerts.push_back(DirectX::XMVectorSubtract(faceCenter, DirectX::XMVectorSubtract(dA, dB)));
        }

        void ClipFaceAgainstPlane(const std::vector<DirectX::XMVECTOR>& faceVerts, const Plane& plane, ContactManifold& outManifold) const 
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
