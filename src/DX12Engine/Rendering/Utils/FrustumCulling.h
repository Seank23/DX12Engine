#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

namespace DX12Engine
{
	class FrustumCulling
	{
	public:
		static DirectX::BoundingFrustum BuildFrustum(DirectX::FXMMATRIX projectionMatrix, DirectX::FXMMATRIX viewMatrix);
		static DirectX::BoundingFrustum BuildViewSpaceFrustum(DirectX::FXMMATRIX projectionMatrix);
		static DirectX::BoundingFrustum TransformToWorldSpace(const DirectX::BoundingFrustum& viewSpaceFrustum, DirectX::FXMMATRIX inverseViewMatrix);

		static bool Intersects(const DirectX::BoundingFrustum& frustum, const DirectX::BoundingBox& box);
		static bool Intersects(const DirectX::BoundingFrustum& frustum, const DirectX::BoundingOrientedBox& box);
	};
}