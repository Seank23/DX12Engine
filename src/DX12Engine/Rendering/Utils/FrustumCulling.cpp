#include "FrustumCulling.h"
#include <utility>

namespace DX12Engine
{
	DirectX::BoundingFrustum FrustumCulling::BuildFrustum(DirectX::FXMMATRIX projectionMatrix, DirectX::FXMMATRIX viewMatrix)
	{
		const DirectX::XMMATRIX inverseViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		const DirectX::BoundingFrustum viewSpaceFrustum = BuildViewSpaceFrustum(projectionMatrix);
		return TransformToWorldSpace(viewSpaceFrustum, inverseViewMatrix);
	}

	DirectX::BoundingFrustum FrustumCulling::BuildViewSpaceFrustum(DirectX::FXMMATRIX projectionMatrix)
	{
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, projectionMatrix);
		// CreateFromMatrix assumes standard-Z; our reverse-Z projection makes it
		// read Near/Far swapped (Near > Far), so swap them back.
		std::swap(frustum.Near, frustum.Far);
		return frustum;
	}

	DirectX::BoundingFrustum FrustumCulling::TransformToWorldSpace(const DirectX::BoundingFrustum& viewSpaceFrustum, DirectX::FXMMATRIX inverseViewMatrix)
	{
		DirectX::BoundingFrustum worldFrustum;
		viewSpaceFrustum.Transform(worldFrustum, inverseViewMatrix);
		return worldFrustum;
	}

	bool FrustumCulling::Intersects(const DirectX::BoundingFrustum& frustum, const DirectX::BoundingBox& box)
	{
		return frustum.Intersects(box);
	}

	bool FrustumCulling::Intersects(const DirectX::BoundingFrustum& frustum, const DirectX::BoundingOrientedBox& box)
	{
		return frustum.Intersects(box);
	}
}