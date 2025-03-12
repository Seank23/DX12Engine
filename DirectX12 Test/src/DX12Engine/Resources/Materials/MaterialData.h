#pragma once
#include <DirectXMath.h>

namespace DX12Engine
{
	struct MaterialData
	{
		DirectX::XMFLOAT4 BaseColor;
		int HasTexture = 0;
	};

	struct BasicMaterialData : MaterialData
	{
		
	};

	struct alignas(16) PBRMaterialData : MaterialData
	{
		DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
		float Metallic = 0.0f;
		float Roughness = 0.1f;
		float AO = 0.5f;
		DirectX::XMFLOAT3 Emissive = { 0.0f, 0.0f, 0.0f };
		float Padding;
	};
}
