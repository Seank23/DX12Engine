#pragma once
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>

namespace DX12Engine
{
    struct Vertex 
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 TexCoord;
        DirectX::XMFLOAT3 Tangent;
    };

    struct Mesh 
    {
        std::vector<Vertex> Vertices;
        std::vector<UINT> Indices;

		void Reset()
		{
			Vertices.clear();
			Indices.clear();
		}
    };
}
