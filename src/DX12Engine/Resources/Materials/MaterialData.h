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

	struct alignas(16) PBRMaterialData
	{
		DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
		float Metallic = 0.0f;
		float Roughness = 0.8f;
		float AO = 1.0f;
		DirectX::XMFLOAT3 Emissive = { 0.0f, 0.0f, 0.0f };
		int HasAlbedoMap    = 0;
		int HasNormalMap    = 0;
		int HasMetallicMap  = 0;
		int HasRoughnessMap = 0;
		int HasAOMap        = 0;
		float Padding[3]    = {};
	};
}
