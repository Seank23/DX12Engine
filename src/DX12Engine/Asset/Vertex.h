#pragma once
#include <DirectXMath.h>

namespace DX12Engine
{
	struct Vertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexCoord;
		DirectX::XMFLOAT4 Tangent; // xyz = tangent direction, w = handedness sign (+1 or -1)
	};
}
