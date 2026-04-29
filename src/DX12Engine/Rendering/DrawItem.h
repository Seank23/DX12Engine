#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <cstdint>

namespace DX12Engine
{
	class MeshPrimitive;
	class Material;
	class MaterialTemplate;
	enum class AlphaMode : uint8_t;

	struct DrawItem
	{
		MeshPrimitive*    Primitive;
		Material*         Material;
		MaterialTemplate* Template;
		D3D12_GPU_VIRTUAL_ADDRESS CBVAddress;

		UINT IndexCount;
		UINT FirstIndex;
		INT  BaseVertex;
		UINT ActiveLODLevel;

		DirectX::XMMATRIX ModelMatrix;

		uint64_t PipelineKey;
		uint64_t MaterialKey;
		uint64_t MeshKey;

		AlphaMode BlendMode;
	};
}
