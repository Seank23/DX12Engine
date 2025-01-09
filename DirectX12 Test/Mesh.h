#pragma once
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>

namespace DX12Engine
{
    struct Vertex {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT4 Color;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 TexCoord;
    };

    struct Mesh {
        std::vector<Vertex> Vertices;
        std::vector<UINT> Indices;
    };
}
