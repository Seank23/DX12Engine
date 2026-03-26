#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <cstdint>

namespace DX12Engine
{
	class MeshPrimitive;
	class Material;

	struct DrawItem
	{
		MeshPrimitive* Primitive;
		Material* Material;
		D3D12_GPU_VIRTUAL_ADDRESS CBVAddress;

		UINT IndexCount;
		UINT FirstIndex;
		INT BaseVertex;

		// Object-space model matrix – needed by passes that compute their own
		// per-draw transform (e.g. shadow maps) without a per-object CBV.
		DirectX::XMMATRIX ModelMatrix;

		// Sort keys – used to produce a stable, bind-churn-minimising draw order.
		// PipelineKey: PSO variant (reserved for future multi-PSO support; 0 for now).
		// MaterialKey: identity of the material (pointer cast to uint64).
		// MeshKey:     identity of the vertex/index buffer set (Primitive pointer).
		uint64_t PipelineKey;
		uint64_t MaterialKey;
		uint64_t MeshKey;
	};
}