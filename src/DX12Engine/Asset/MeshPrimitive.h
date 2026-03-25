#pragma once
#include "../Rendering/Buffers/IndexBuffer.h"
#include "../Rendering/Buffers/VertexBuffer.h"
#include <DirectXMath.h>
#include <cstdint>
#include <memory>

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
        MeshPrimitive() = default;
        MeshPrimitive(std::unique_ptr<VertexBuffer> vertexBuffer, std::unique_ptr<IndexBuffer> indexBuffer,
            UINT indexCount = 0, UINT firstIndex = 0, INT baseVertex = 0, UINT materialIndex = 0)
            : m_VertexBuffer(std::move(vertexBuffer)),
            m_IndexBuffer(std::move(indexBuffer)),
            m_IndexCount(indexCount),
            m_FirstIndex(firstIndex),
            m_BaseVertex(baseVertex),
            m_MaterialIndex(materialIndex)
        {
            if (m_IndexCount == 0 && m_IndexBuffer)
                m_IndexCount = InferIndexCount(m_IndexBuffer.get());
        }

        MeshPrimitive(const MeshPrimitive&) = delete;
        MeshPrimitive& operator=(const MeshPrimitive&) = delete;
        MeshPrimitive(MeshPrimitive&&) noexcept = default;
        MeshPrimitive& operator=(MeshPrimitive&&) noexcept = default;
        ~MeshPrimitive() = default;

        void SetVertexBuffer(std::unique_ptr<VertexBuffer> vertexBuffer) { m_VertexBuffer = std::move(vertexBuffer); }
        void SetIndexBuffer(std::unique_ptr<IndexBuffer> indexBuffer)
        {
            m_IndexBuffer = std::move(indexBuffer);
            if (m_IndexCount == 0 && m_IndexBuffer)
                m_IndexCount = InferIndexCount(m_IndexBuffer.get());
        }

        std::unique_ptr<VertexBuffer> TakeVertexBuffer() { return std::move(m_VertexBuffer); }
        std::unique_ptr<IndexBuffer> TakeIndexBuffer() { return std::move(m_IndexBuffer); }

        VertexBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
        IndexBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }

        D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const
        {
            return m_VertexBuffer ? m_VertexBuffer->GetVertexBufferView() : D3D12_VERTEX_BUFFER_VIEW{};
        }

        D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
        {
            return m_IndexBuffer ? m_IndexBuffer->GetIndexBufferView() : D3D12_INDEX_BUFFER_VIEW{};
        }

        void SetDrawRange(UINT indexCount, UINT firstIndex = 0, INT baseVertex = 0)
        {
            m_IndexCount = indexCount;
            m_FirstIndex = firstIndex;
            m_BaseVertex = baseVertex;
        }

        UINT GetIndexCount() const { return m_IndexCount; }
        UINT GetFirstIndex() const { return m_FirstIndex; }
        INT GetBaseVertex() const { return m_BaseVertex; }

        void SetMaterialIndex(UINT materialIndex) { m_MaterialIndex = materialIndex; }
        UINT GetMaterialIndex() const { return m_MaterialIndex; }

        void SetBounds(const MeshBounds& bounds) { m_Bounds = bounds; }
        const MeshBounds& GetBounds() const { return m_Bounds; }

        bool HasGeometry() const { return m_VertexBuffer != nullptr && m_IndexBuffer != nullptr && m_IndexCount > 0; }

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
        std::unique_ptr<IndexBuffer> m_IndexBuffer;
        UINT m_IndexCount = 0;
        UINT m_FirstIndex = 0;
        INT m_BaseVertex = 0;
        UINT m_MaterialIndex = 0;
        MeshBounds m_Bounds;
    };
}
