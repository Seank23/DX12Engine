#pragma once
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>

namespace DX12Engine
{
    struct AABoundingBox
    {
        std::vector<DirectX::XMVECTOR> Vertices;
        std::vector<UINT> Indices;
        DirectX::XMFLOAT3 MaxPoint;
        DirectX::XMFLOAT3 MinPoint;
		DirectX::XMFLOAT3 Dimensions;

		bool Intersects(const AABoundingBox& other) const
		{
            if (Vertices.empty() || other.Vertices.empty()) return false;
            return (MinPoint.x <= other.MaxPoint.x && MaxPoint.x >= other.MinPoint.x) &&
                (MinPoint.y <= other.MaxPoint.y && MaxPoint.y >= other.MinPoint.y) &&
                (MinPoint.z <= other.MaxPoint.z && MaxPoint.z >= other.MinPoint.z);
		}
    };
}
