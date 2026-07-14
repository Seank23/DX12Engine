#pragma once
#include "../Rendering/Buffers/IndexBuffer.h"
#include "../Rendering/Buffers/VertexBuffer.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace DX12Engine
{
	struct MeshBounds
	{
		DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
	};

	class MeshPrimitive
	{
	public:
		struct LOD
		{
			std::unique_ptr<IndexBuffer> Buffer;
			UINT IndexCount = 0;
			float Ratio = 1.0f;
			float Error = 0.0f;
		};

		MeshPrimitive() = default;
		MeshPrimitive(std::unique_ptr<VertexBuffer> vertexBuffer, std::unique_ptr<IndexBuffer> indexBuffer, UINT indexCount = 0, UINT firstIndex = 0, INT baseVertex = 0, UINT materialIndex = 0)
			: m_VertexBuffer(std::move(vertexBuffer)),
			  m_FirstIndex(firstIndex),
			  m_BaseVertex(baseVertex),
			  m_MaterialIndex(materialIndex)
		{
			LOD baseLod;
			baseLod.Buffer = std::move(indexBuffer);
			baseLod.IndexCount = indexCount;
			baseLod.Ratio = 1.0f;
			baseLod.Error = 0.0f;
			if (baseLod.IndexCount == 0 && baseLod.Buffer)
				baseLod.IndexCount = InferIndexCount(baseLod.Buffer.get());
			m_LODs.push_back(std::move(baseLod));
		}

		MeshPrimitive(const MeshPrimitive&) = delete;
		MeshPrimitive& operator=(const MeshPrimitive&) = delete;
		MeshPrimitive(MeshPrimitive&&) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&&) noexcept = default;
		~MeshPrimitive() = default;

		void SetVertexBuffer(std::unique_ptr<VertexBuffer> vertexBuffer) { m_VertexBuffer = std::move(vertexBuffer); }
		void SetIndexBuffer(std::unique_ptr<IndexBuffer> indexBuffer)
		{
			if (m_LODs.empty())
				m_LODs.resize(1);

			m_LODs[0].Buffer = std::move(indexBuffer);
			m_LODs[0].IndexCount = m_LODs[0].Buffer ? InferIndexCount(m_LODs[0].Buffer.get()) : 0;

			if (m_ActiveLODLevel >= m_LODs.size())
				m_ActiveLODLevel = 0;
		}

		std::unique_ptr<VertexBuffer> TakeVertexBuffer() { return std::move(m_VertexBuffer); }
		std::unique_ptr<IndexBuffer> TakeIndexBuffer()
		{
			if (m_LODs.empty())
				return nullptr;

			return std::move(m_LODs[0].Buffer);
		}

		VertexBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
		IndexBuffer* GetIndexBuffer() const
		{
			if (m_LODs.empty())
				return nullptr;

			return m_LODs[0].Buffer.get();
		}

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const
		{
			return m_VertexBuffer ? m_VertexBuffer->GetVertexBufferView() : D3D12_VERTEX_BUFFER_VIEW{};
		}

		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
		{
			return GetIndexBuffer() ? GetIndexBuffer()->GetIndexBufferView() : D3D12_INDEX_BUFFER_VIEW{};
		}

		void SetDrawRange(UINT indexCount, UINT firstIndex = 0, INT baseVertex = 0)
		{
			if (m_LODs.empty())
				m_LODs.resize(1);

			m_LODs[0].IndexCount = indexCount;
			m_FirstIndex = firstIndex;
			m_BaseVertex = baseVertex;
		}

		UINT GetIndexCount() const
		{
			if (m_LODs.empty())
				return 0;

			return m_LODs[0].IndexCount;
		}
		UINT GetFirstIndex() const { return m_FirstIndex; }
		INT GetBaseVertex() const { return m_BaseVertex; }

		bool AddLODBuffer(std::unique_ptr<IndexBuffer> indexBuffer, UINT indexCount, float ratio, float error)
		{
			if (!indexBuffer)
				return false;

			LOD lod;
			lod.Buffer = std::move(indexBuffer);
			lod.IndexCount = indexCount;
			lod.Ratio = ratio;
			lod.Error = error;
			if (lod.IndexCount == 0)
				lod.IndexCount = InferIndexCount(lod.Buffer.get());

			if (lod.IndexCount == 0)
				return false;

			m_LODs.push_back(std::move(lod));
			return true;
		}

		void ClearAdditionalLODs()
		{
			if (m_LODs.size() > 1)
				m_LODs.erase(m_LODs.begin() + 1, m_LODs.end());

			if (m_ActiveLODLevel >= m_LODs.size())
				m_ActiveLODLevel = 0;
		}

		bool SetActiveLOD(UINT level)
		{
			if (m_LODs.empty())
			{
				m_ActiveLODLevel = 0;
				return false;
			}

			const UINT clampedLevel = (std::min)(level, static_cast<UINT>(m_LODs.size() - 1));
			if (!m_LODs[clampedLevel].Buffer || m_LODs[clampedLevel].IndexCount == 0)
			{
				m_ActiveLODLevel = 0;
				return false;
			}

			m_ActiveLODLevel = clampedLevel;
			return true;
		}

		UINT GetActiveLODLevel() const { return m_ActiveLODLevel; }
		UINT GetLODCount() const { return static_cast<UINT>(m_LODs.size()); }

		const LOD* GetLOD(UINT level) const
		{
			if (level >= m_LODs.size())
				return nullptr;

			return &m_LODs[level];
		}

		D3D12_INDEX_BUFFER_VIEW GetActiveIndexBufferView() const
		{
			if (m_ActiveLODLevel >= m_LODs.size())
				return D3D12_INDEX_BUFFER_VIEW{};

			const IndexBuffer* buffer = m_LODs[m_ActiveLODLevel].Buffer.get();
			return buffer ? buffer->GetIndexBufferView() : D3D12_INDEX_BUFFER_VIEW{};
		}

		UINT GetActiveIndexCount() const
		{
			if (m_ActiveLODLevel >= m_LODs.size())
				return 0;

			return m_LODs[m_ActiveLODLevel].IndexCount;
		}

		UINT GetActiveFirstIndex() const { return m_ActiveLODLevel == 0 ? m_FirstIndex : 0; }
		INT GetActiveBaseVertex() const { return m_ActiveLODLevel == 0 ? m_BaseVertex : 0; }

		void SetMaterialIndex(UINT materialIndex) { m_MaterialIndex = materialIndex; }
		UINT GetMaterialIndex() const { return m_MaterialIndex; }

		void SetBounds(const MeshBounds& bounds)
		{
			m_Bounds = bounds;
			DirectX::XMFLOAT3 center = {
				(bounds.Min.x + bounds.Max.x) * 0.5f,
				(bounds.Min.y + bounds.Max.y) * 0.5f,
				(bounds.Min.z + bounds.Max.z) * 0.5f
			};
			DirectX::XMFLOAT3 extents = {
				(bounds.Max.x - bounds.Min.x) * 0.5f,
				(bounds.Max.y - bounds.Min.y) * 0.5f,
				(bounds.Max.z - bounds.Min.z) * 0.5f
			};
			m_BoundingBox = DirectX::BoundingBox(center, extents);
		}
		const MeshBounds& GetBounds() const { return m_Bounds; }
		const DirectX::BoundingBox& GetBoundingBox() const { return m_BoundingBox; }

		void ComputeOrientedBoundingBox(const DirectX::XMMATRIX& modelMatrix)
		{
			if (m_Bounds.Min.x == m_Bounds.Max.x && m_Bounds.Min.y == m_Bounds.Max.y && m_Bounds.Min.z == m_Bounds.Max.z)
			{
				m_OrientedBoundingBox = DirectX::BoundingOrientedBox(DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT3(0, 0, 0), DirectX::XMFLOAT4(0, 0, 0, 1));
				return;
			}
			DirectX::BoundingOrientedBox localOrientedBox;
			DirectX::BoundingOrientedBox::CreateFromBoundingBox(localOrientedBox, m_BoundingBox);
			localOrientedBox.Transform(m_OrientedBoundingBox, modelMatrix);
		}
		const DirectX::BoundingOrientedBox& GetOrientedBoundingBox() const { return m_OrientedBoundingBox; }

		bool HasGeometry() const
		{
			const IndexBuffer* baseIndexBuffer = GetIndexBuffer();
			return m_VertexBuffer != nullptr && baseIndexBuffer != nullptr && GetIndexCount() > 0;
		}

	private:
		static UINT InferIndexCount(const IndexBuffer* indexBuffer)
		{
			if (!indexBuffer)
				return 0;

			D3D12_INDEX_BUFFER_VIEW view = indexBuffer->GetIndexBufferView();
			if (view.Format == DXGI_FORMAT_R16_UINT)
				return view.SizeInBytes / sizeof(std::uint16_t);

			return view.SizeInBytes / sizeof(std::uint32_t);
		}

		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::vector<LOD> m_LODs;
		UINT m_FirstIndex = 0;
		INT m_BaseVertex = 0;
		UINT m_MaterialIndex = 0;
		MeshBounds m_Bounds;
		DirectX::BoundingBox m_BoundingBox;
		DirectX::BoundingOrientedBox m_OrientedBoundingBox;
		UINT m_ActiveLODLevel = 0;
	};
}
