#pragma once
#include "d3dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#define NOMINMAX
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
#include "../Resources/RenderTexture.h"
#include "../Heaps/DescriptorHeapManager.h"
#include "../Rendering/RenderContext.h"
#include "../Rendering/GPUUploader.h"
#include "../Rendering/PipelineStateCache.h"
#include "../Rendering/RootSignatureCache.h"
#include "../IO/TextureLoader.h"

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
		std::unique_ptr<Texture> CreateCubeMap(const DirectX::ScratchImage* imageData);
		std::unique_ptr<RenderTexture> CreateDepthMap(DirectX::XMINT3 dimensions, DXGI_FORMAT dsvFormat, DXGI_FORMAT srvFormat, bool isCubeMap = false);
		std::unique_ptr<RenderTexture> CreateRenderTargetTexture(DirectX::XMINT2 dimensions, DXGI_FORMAT format, UINT mipLevels = 1);

		void UpdateSRVDescriptors(std::vector<GPUResource*> resources);

		Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
		Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);

		Shader* GetShader(const std::string& name) { return m_Shaders[name].get(); }

		static std::wstring GetMaterialPath(std::string path) { return L"res/Materials/" + std::wstring(path.begin(), path.end()); }
		static std::string GetModelPath(std::string path) { return "res/Models/" + path; }
		static std::string GetShaderPath(std::string path) { return "res/Shaders/" + path; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		DescriptorHeapManager* m_HeapManager;
		GPUUploader* m_GPUUploader;
		std::unique_ptr<PipelineStateCache> m_PipelineStateCache;
		std::unique_ptr<RootSignatureCache> m_RootSignatureCache;
		std::unordered_map<std::string, std::unique_ptr<Shader>> m_Shaders;
	};
}

