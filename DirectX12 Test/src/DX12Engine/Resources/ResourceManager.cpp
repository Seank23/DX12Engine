#include "ResourceManager.h"
#include "../Heaps/DescriptorHeapManager.h"
#include "../Utils/EngineUtils.h"
#include "../Utils/Constants.h"
#include "UploadResourceWrapper.h"

namespace DX12Engine
{
	static ResourceManager* s_Instance = nullptr;

	ResourceManager::ResourceManager()
	{
		m_Shaders.insert({ "BasicLighting_VS", std::make_unique<Shader>(GetShaderPath("BasicLighting_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "BasicLighting_PS", std::make_unique<Shader>(GetShaderPath("BasicLighting_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "PBRLighting_VS", std::make_unique<Shader>(GetShaderPath("PBRLighting_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "PBRLighting_PS", std::make_unique<Shader>(GetShaderPath("PBRLighting_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "ShadowMap_VS", std::make_unique<Shader>(GetShaderPath("ShadowMap_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "ShadowCubeMap_VS", std::make_unique<Shader>(GetShaderPath("ShadowCubeMap_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "ShadowCubeMap_PS", std::make_unique<Shader>(GetShaderPath("ShadowCubeMap_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "Geometry_VS", std::make_unique<Shader>(GetShaderPath("Geometry_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "Geometry_PS", std::make_unique<Shader>(GetShaderPath("Geometry_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "RenderTriangle_VS", std::make_unique<Shader>(GetShaderPath("RenderTriangle_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "RenderTriangleInvertedY_VS", std::make_unique<Shader>(GetShaderPath("RenderTriangleInvertedY_VS.hlsl"), "vertex") });
		m_Shaders.insert({ "PBRLightingDeferred_PS", std::make_unique<Shader>(GetShaderPath("PBRLightingDeferred_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "FinalRender_PS", std::make_unique<Shader>(GetShaderPath("FinalRender_PS.hlsl"), "pixel") });
		m_Shaders.insert({ "SSRPass_PS", std::make_unique<Shader>(GetShaderPath("SSRPass_PS.hlsl"), "pixel") });
	}

	ResourceManager::~ResourceManager()
	{
	}

	ResourceManager& ResourceManager::GetInstance()
	{
		if (!s_Instance)
			s_Instance = new ResourceManager();
		return *s_Instance;
	}

	void ResourceManager::Shutdown()
	{
		delete s_Instance;
		s_Instance = nullptr;
	}

	void ResourceManager::Init(RenderContext& context)
	{
		m_Device = context.GetDevice();
		m_HeapManager = &(context.GetHeapManager());
		m_GPUUploader = &(context.GetUploader());
		m_PipelineStateCache = std::make_unique<PipelineStateCache>(m_Device.Get());
		m_RootSignatureCache = std::make_unique<RootSignatureCache>(m_Device.Get());
	}

	std::unique_ptr<VertexBuffer> ResourceManager::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		const UINT vertexBufferSize = sizeof(Vertex) * vertices.size();

		ID3D12Resource* vertexBufferResource = nullptr;
		auto mainHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto mainResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&mainHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&mainResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&vertexBufferResource)));

		ID3D12Resource* uploadResource = nullptr;
		auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceDesc,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr,
			IID_PPV_ARGS(&uploadResource)));

		auto vertexBuffer = std::make_unique<VertexBuffer>(vertexBufferResource, D3D12_RESOURCE_STATE_COPY_DEST, sizeof(Vertex), vertexBufferSize);

		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = &vertices[0];
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;

		UploadResourceWrapper uploadResourceWrapper;
		uploadResourceWrapper.GPUResource = vertexBuffer.get();
		uploadResourceWrapper.UploadResource = uploadResource;
		uploadResourceWrapper.UploadState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		uploadResourceWrapper.Data = vertexData;

		m_GPUUploader->UploadResource(uploadResourceWrapper);

		return vertexBuffer;
	}

	std::unique_ptr<IndexBuffer> ResourceManager::CreateIndexBuffer(const std::vector<UINT>& indices)
	{
		const UINT indexBufferSize = sizeof(UINT) * indices.size();

		ID3D12Resource* indexBufferResource = nullptr;
		auto mainHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto mainResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&mainHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&mainResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&indexBufferResource)));

		ID3D12Resource* uploadResource = nullptr;
		auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceDesc,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr,
			IID_PPV_ARGS(&uploadResource)));

		auto indexBuffer = std::make_unique<IndexBuffer>(indexBufferResource, D3D12_RESOURCE_STATE_COPY_DEST, DXGI_FORMAT_R32_UINT, indexBufferSize);

		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = &indices[0];
		indexData.RowPitch = indexBufferSize;
		indexData.SlicePitch = indexData.RowPitch;

		UploadResourceWrapper uploadResourceWrapper;
		uploadResourceWrapper.GPUResource = indexBuffer.get();
		uploadResourceWrapper.UploadResource = uploadResource;
		uploadResourceWrapper.UploadState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		uploadResourceWrapper.Data = indexData;

		m_GPUUploader->UploadResource(uploadResourceWrapper);

		return indexBuffer;
	}

	std::unique_ptr<ConstantBuffer> ResourceManager::CreateConstantBuffer(const UINT bufferSize)
	{
		ID3D12Resource* constantBufferResource = nullptr;
		UINT alignedSize = EngineUtils::AlignUINT(bufferSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		D3D12_RESOURCE_DESC constantBufferDesc;
		constantBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		constantBufferDesc.Alignment = 0;
		constantBufferDesc.Width = alignedSize;
		constantBufferDesc.Height = 1;
		constantBufferDesc.DepthOrArraySize = 1;
		constantBufferDesc.MipLevels = 1;
		constantBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		constantBufferDesc.SampleDesc.Count = 1;
		constantBufferDesc.SampleDesc.Quality = 0;
		constantBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		constantBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&constantBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBufferResource)
		));

		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = constantBufferResource->GetGPUVirtualAddress();
		constantBufferViewDesc.SizeInBytes = alignedSize;

		DescriptorHeapHandle constantBufferHeapHandle = m_HeapManager->GetNewSRVDescriptorHeapHandle();
		m_Device->CreateConstantBufferView(&constantBufferViewDesc, constantBufferHeapHandle.GetCPUHandle());
		std::unique_ptr<ConstantBuffer> constantBuffer = std::make_unique<ConstantBuffer>(constantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, alignedSize, constantBufferHeapHandle);
		constantBuffer->SetIsReady(true);
		return constantBuffer;
	}

	std::unique_ptr<Texture> ResourceManager::CreateTexture(const DirectX::ScratchImage* imageData)
	{
		const DirectX::Image* image = imageData->GetImage(0, 0, 0);

		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Format = image->format;
		textureDesc.Width = image->width;
		textureDesc.Height = image->height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Alignment = 0;

		D3D12_HEAP_PROPERTIES defaultProperties;
		defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		defaultProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		defaultProperties.CreationNodeMask = 0;
		defaultProperties.VisibleNodeMask = 0;

		ID3D12Resource* textureResource = nullptr;
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&defaultProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&textureResource)
		));

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource, 0, 1);
		ID3D12Resource* textureUploadResource = nullptr;
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadResource)
		));

		std::vector<D3D12_SUBRESOURCE_DATA> textureData;
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = image->pixels;
		data.RowPitch = image->rowPitch;
		data.SlicePitch = image->slicePitch;
		textureData.emplace_back(data);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureResource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetNewSRVDescriptorHeapHandle();
		m_Device->CreateShaderResourceView(textureResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<Texture>(textureResource, textureUploadResource, D3D12_RESOURCE_STATE_COPY_DEST, textureData, srvHandle, false);
	}

	std::unique_ptr<Texture> ResourceManager::CreateCubeMap(const DirectX::ScratchImage* imageData)
	{
		const DirectX::TexMetadata& metadata = imageData->GetMetadata();

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = static_cast<UINT>(metadata.width);
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);  // 6 for a cubemap
		textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		textureDesc.Format = metadata.format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES defaultProperties;
		defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		defaultProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		defaultProperties.CreationNodeMask = 0;
		defaultProperties.VisibleNodeMask = 0;

		ID3D12Resource* textureResource = nullptr;
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&defaultProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&textureResource)
		));

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource, 0, metadata.mipLevels * metadata.arraySize);
		ID3D12Resource* textureUploadResource = nullptr;
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadResource)
		));

		std::vector<D3D12_SUBRESOURCE_DATA> cubemapData;
		DirectX::PrepareUpload(m_Device.Get(), imageData->GetImages(), imageData->GetImageCount(), metadata, cubemapData);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = metadata.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);

		DescriptorHeapHandle srvHandle = m_HeapManager->GetNewSRVDescriptorHeapHandle();
		m_Device->CreateShaderResourceView(textureResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<Texture>(textureResource, textureUploadResource, D3D12_RESOURCE_STATE_COPY_DEST, cubemapData, srvHandle, true);
	}

	std::unique_ptr<RenderTexture> ResourceManager::CreateDepthMap(DirectX::XMINT3 dimensions, DXGI_FORMAT dsvFormat, DXGI_FORMAT srvFormat, bool isCubeMap)
	{
		int arraySize = dimensions.z;
		bool isSingleMap = arraySize == 1 && !isCubeMap;
		if (isCubeMap)
			arraySize = 6;

		D3D12_RESOURCE_DESC depthMapDesc = {};
		depthMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthMapDesc.Width = dimensions.x;
		depthMapDesc.Height = dimensions.y;
		depthMapDesc.DepthOrArraySize = arraySize;
		depthMapDesc.MipLevels = 1;
		depthMapDesc.Format = dsvFormat;
		depthMapDesc.SampleDesc.Count = 1;
		depthMapDesc.SampleDesc.Quality = 0;
		depthMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = dsvFormat;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ID3D12Resource* depthMapResource = nullptr;
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthMapDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&depthMapResource));

		std::vector<DescriptorHeapHandle> dsvDescriptors;
		if (isSingleMap)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = dsvFormat;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

			DescriptorHeapHandle dsvHandle = m_HeapManager->GetNewDSVDescriptorHeapHandle();
			m_Device->CreateDepthStencilView(depthMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
			dsvDescriptors.push_back(dsvHandle);
		}
		else
		{
			for (int i = 0; i < arraySize; i++)
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format = dsvFormat;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.FirstArraySlice = i;
				dsvDesc.Texture2DArray.ArraySize = 1;
				dsvDesc.Texture2DArray.MipSlice = 0;

				DescriptorHeapHandle dsvHandle = m_HeapManager->GetNewDSVDescriptorHeapHandle();
				m_Device->CreateDepthStencilView(depthMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
				dsvDescriptors.push_back(dsvHandle);
			}
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = srvFormat;
		srvDesc.ViewDimension = isCubeMap ? D3D12_SRV_DIMENSION_TEXTURECUBE : isSingleMap ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2D.MipLevels = 1;
		if (!isSingleMap) srvDesc.Texture2DArray.ArraySize = arraySize;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetRenderHeapHandleBlock(1);
		m_Device->CreateShaderResourceView(depthMapResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<RenderTexture>(depthMapResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, srvHandle, dsvDescriptors, isCubeMap);
	}

	std::unique_ptr<RenderTexture> ResourceManager::CreateRenderTargetTexture(DirectX::XMINT2 dimensions, DXGI_FORMAT format, UINT mipLevels)
	{
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = dimensions.x;
		textureDesc.Height = dimensions.y;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = mipLevels;
		textureDesc.Format = format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = format;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;

		ID3D12Resource* renderTargetResource = nullptr;
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&renderTargetResource));

		std::vector<DescriptorHeapHandle> rtvDescriptors;
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		DescriptorHeapHandle rtvHandle = m_HeapManager->GetNewRTVDescriptorHeapHandle();
		m_Device->CreateRenderTargetView(renderTargetResource, &rtvDesc, rtvHandle.GetCPUHandle());
		rtvDescriptors.push_back(rtvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = mipLevels;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetRenderHeapHandleBlock(1);
		m_Device->CreateShaderResourceView(renderTargetResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<RenderTexture>(renderTargetResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, srvHandle, rtvDescriptors, false);
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> ResourceManager::CreatePipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
	{
		return m_PipelineStateCache->GetOrCreatePSO(desc);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> ResourceManager::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		return m_RootSignatureCache->GetOrCreateRootSignature(desc);
	}
}
