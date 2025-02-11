#pragma once
#include "d3dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <memory>
#include <DirectXTex.h>

#include "../Buffers/VertexBuffer.h"
#include "../Buffers/IndexBuffer.h"
#include "../Buffers/ConstantBuffer.h"
#include "../Resources/Mesh.h"
#include "../Resources/Texture.h"
#include "../Heaps/DescriptorHeapManager.h"
#include "../Rendering/RenderContext.h"
#include "../Rendering/GPUUploader.h"

namespace DX12Engine
{
	class ResourceManager
	{
    public:
		static ResourceManager& GetInstance();
		void Init(RenderContext& context);
		static void Shutdown();
		
		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;

	private:
		ResourceManager();
		~ResourceManager();

	public:
		std::unique_ptr<VertexBuffer> CreateVertexBuffer(const std::vector<Vertex>& vertices);
		std::unique_ptr<IndexBuffer> CreateIndexBuffer(const std::vector<UINT>& indices);
		std::unique_ptr<ConstantBuffer> CreateConstantBuffer(const UINT bufferSize);
		std::unique_ptr<Texture> CreateTexture(const DirectX::ScratchImage* imageData);

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		DescriptorHeapManager* m_HeapManager;
		GPUUploader* m_GPUUploader;
	};
}

