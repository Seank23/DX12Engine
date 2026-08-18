#include "ResourceManager.h"
#include "Materials/PBRMaterial.h"
#include "../Rendering/Heaps/DescriptorHeapManager.h"
#include "../Utils/EngineUtils.h"
#include "../Utils/Constants.h"
#include "UploadResourceWrapper.h"
#include <filesystem>
#include <iostream>
#include <limits>

namespace DX12Engine
{
	static ResourceManager* s_Instance = nullptr;

	ResourceManager::ResourceManager()
	{
		for (auto& file : std::filesystem::directory_iterator(GetShaderFolder()))
		{
			if (file.path().extension() == ".hlsl")
			{
				std::string filename = file.path().filename().string();
				filename = filename.substr(0, filename.size() - 5); // Remove .hlsl extension
				if (filename.find("_VS") != std::string::npos)
					m_Shaders.insert({ filename, std::make_unique<Shader>(GetShaderPath(filename + ".hlsl"), ShaderType::Vertex) });
				else if (filename.find("_PS") != std::string::npos)
					m_Shaders.insert({ filename, std::make_unique<Shader>(GetShaderPath(filename + ".hlsl"), ShaderType::Pixel) });
				else if (filename.find("_CS") != std::string::npos)
					m_Shaders.insert({ filename, std::make_unique<Shader>(GetShaderPath(filename + ".hlsl"), ShaderType::Compute) });
			}
		}
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

	DescriptorHeapManager* ResourceManager::TryGetHeapManager()
	{
		if (!s_Instance)
			return nullptr;

		return s_Instance->m_HeapManager;
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
		m_FrameConstantAllocator = std::make_unique<FrameConstantAllocator>(m_Device.Get(), CONSTANT_RING_BYTES_PER_FRAME);
		m_DefaultMaterial = std::make_unique<PBRMaterial>();
		m_DefaultMaterial->SetAlbedo({ 1.0f, 0.0f, 1.0f });
		m_DefaultMaterial->SetMetallic(0.0f);
		m_DefaultMaterial->SetRoughness(0.8f);
	}

	std::unique_ptr<VertexBuffer> ResourceManager::CreateVertexBuffer(const std::vector<Vertex>& vertices)
	{
		if (vertices.empty())
		{
			std::cerr << "[ResourceManager] Refusing to create vertex buffer with zero vertices.\n";
			return nullptr;
		}

		if (vertices.size() > (std::numeric_limits<UINT>::max)() / sizeof(Vertex))
		{
			std::cerr << "[ResourceManager] Vertex buffer is too large for 32-bit byte size.\n";
			return nullptr;
		}

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
		vertexData.pData = vertices.data();
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
		if (indices.empty())
		{
			std::cerr << "[ResourceManager] Refusing to create index buffer with zero indices.\n";
			return nullptr;
		}

		if (indices.size() > (std::numeric_limits<UINT>::max)() / sizeof(UINT))
		{
			std::cerr << "[ResourceManager] Index buffer is too large for 32-bit byte size.\n";
			return nullptr;
		}

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
		indexData.pData = indices.data();
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
		constantBufferDesc.Width = alignedSize * FRAMES_IN_FLIGHT;
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
			IID_PPV_ARGS(&constantBufferResource)));

		std::unique_ptr<ConstantBuffer> constantBuffer = std::make_unique<ConstantBuffer>(constantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, alignedSize);
		constantBuffer->SetIsReady(true);
		return constantBuffer;
	}

	std::unique_ptr<Texture> ResourceManager::CreateTexture(DirectX::ScratchImage* imageData)
	{
		const DirectX::TexMetadata& metadata = imageData->GetMetadata();

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = static_cast<UINT>(metadata.width);
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		textureDesc.DepthOrArraySize = 1;
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
			IID_PPV_ARGS(&textureResource)));

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		std::vector<D3D12_SUBRESOURCE_DATA> textureData;
		DirectX::PrepareUpload(m_Device.Get(), imageData->GetImages(), imageData->GetImageCount(), metadata, textureData);

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource, 0, static_cast<UINT>(textureData.size()));
		ID3D12Resource* textureUploadResource = nullptr;
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadResource)));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = metadata.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

		DescriptorHeapHandle srvHandle = m_HeapManager->AllocatePersistentSRV();

		return std::make_unique<Texture>(imageData, textureResource, textureUploadResource, D3D12_RESOURCE_STATE_COPY_DEST, textureData, srvHandle, srvDesc, false);
	}

	std::unique_ptr<Texture> ResourceManager::CreateCubeMap(DirectX::ScratchImage* imageData)
	{
		const DirectX::TexMetadata& metadata = imageData->GetMetadata();

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = static_cast<UINT>(metadata.width);
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize); // 6 for a cubemap
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
			IID_PPV_ARGS(&textureResource)));

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
			IID_PPV_ARGS(&textureUploadResource)));

		std::vector<D3D12_SUBRESOURCE_DATA> cubemapData;
		DirectX::PrepareUpload(m_Device.Get(), imageData->GetImages(), imageData->GetImageCount(), metadata, cubemapData);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = metadata.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);

		DescriptorHeapHandle srvHandle = m_HeapManager->AllocatePersistentSRV();

		return std::make_unique<Texture>(imageData, textureResource, textureUploadResource, D3D12_RESOURCE_STATE_COPY_DEST, cubemapData, srvHandle, srvDesc, true);
	}

	std::unique_ptr<Texture> ResourceManager::CreateDefaultCubeMap()
	{
		constexpr UINT faceSize = 1;
		constexpr UINT arraySize = 6;
		constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = faceSize;
		textureDesc.Height = faceSize;
		textureDesc.DepthOrArraySize = arraySize;
		textureDesc.MipLevels = 1;
		textureDesc.Format = format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ID3D12Resource* textureResource = nullptr;
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&textureResource)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource, 0, arraySize);
		auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ID3D12Resource* uploadResource = nullptr;
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadResource)));

		static constexpr UINT32 grayPixel = 0xFF404040;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources(arraySize);
		for (UINT i = 0; i < arraySize; i++)
		{
			subresources[i].pData = &grayPixel;
			subresources[i].RowPitch = sizeof(UINT32);
			subresources[i].SlicePitch = sizeof(UINT32);
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = 1;

		DescriptorHeapHandle srvHandle = m_HeapManager->AllocatePersistentSRV();
		m_Device->CreateShaderResourceView(textureResource, &srvDesc, srvHandle.GetCPUHandle());

		auto texture = std::make_unique<Texture>(nullptr, textureResource, uploadResource, D3D12_RESOURCE_STATE_COPY_DEST, subresources, srvHandle, srvDesc, true);
		m_GPUUploader->UploadTextureBatch({ texture.get() });

		return texture;
	}

	std::unique_ptr<RenderTexture> ResourceManager::CreateDepthMap(RenderTextureConfig config)
	{
		int arraySize = config.Dimensions.z;
		bool isSingleMap = arraySize == 1 && !config.IsCubeMap;
		int cubeCount = 1;
		if (config.IsCubeMap)
		{
			// One cube (6 faces) per shadow-casting point light; Dimensions.z carries the light count.
			cubeCount = arraySize > 0 ? arraySize : 1;
			arraySize = 6 * cubeCount;
		}

		D3D12_RESOURCE_DESC depthMapDesc = {};
		depthMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthMapDesc.Width = config.Dimensions.x;
		depthMapDesc.Height = config.Dimensions.y;
		depthMapDesc.DepthOrArraySize = arraySize;
		depthMapDesc.MipLevels = 1;
		depthMapDesc.Format = config.DSVFormat;
		depthMapDesc.SampleDesc.Count = 1;
		depthMapDesc.SampleDesc.Quality = 0;
		depthMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = config.DSVFormat;
		depthOptimizedClearValue.DepthStencil.Depth = config.ClearDepth;
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
			dsvDesc.Format = config.DSVFormat;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

			DescriptorHeapHandle dsvHandle = m_HeapManager->AllocatePersistentDSV();
			m_Device->CreateDepthStencilView(depthMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
			dsvDescriptors.push_back(dsvHandle);
		}
		else
		{
			for (int i = 0; i < arraySize; i++)
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format = config.DSVFormat;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.FirstArraySlice = i;
				dsvDesc.Texture2DArray.ArraySize = 1;
				dsvDesc.Texture2DArray.MipSlice = 0;

				DescriptorHeapHandle dsvHandle = m_HeapManager->AllocatePersistentDSV();
				m_Device->CreateDepthStencilView(depthMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
				dsvDescriptors.push_back(dsvHandle);
			}
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = config.Format;
		if (config.IsCubeMap)
		{
			// Bind as a cube array so the lighting shader can index a cube per point light.
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			srvDesc.TextureCubeArray.MostDetailedMip = 0;
			srvDesc.TextureCubeArray.MipLevels = 1;
			srvDesc.TextureCubeArray.First2DArrayFace = 0;
			srvDesc.TextureCubeArray.NumCubes = cubeCount;
		}
		else if (isSingleMap)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.ArraySize = arraySize;
		}

		DescriptorHeapHandle srvHandle = m_HeapManager->AllocatePersistentSRV();
		m_Device->CreateShaderResourceView(depthMapResource, &srvDesc, srvHandle.GetCPUHandle());

		auto renderTexture = std::make_unique<RenderTexture>(depthMapResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, dsvDescriptors, srvDesc, config, true);
		renderTexture->SetTransientDescriptor(srvHandle);
		renderTexture->SetPersistentDescriptor(srvHandle);
		return renderTexture;
	}

	std::unique_ptr<RenderTexture> ResourceManager::CreateRenderTargetTexture(RenderTextureConfig config)
	{
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = config.Dimensions.x;
		textureDesc.Height = config.Dimensions.y;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = config.MipLevels;
		textureDesc.Format = config.Format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = config.Format;
		clearValue.Color[0] = config.ClearColor.x;
		clearValue.Color[1] = config.ClearColor.y;
		clearValue.Color[2] = config.ClearColor.z;
		clearValue.Color[3] = config.ClearColor.w;
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
		rtvDesc.Format = config.Format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		DescriptorHeapHandle rtvHandle = m_HeapManager->AllocatePersistentRTV();
		m_Device->CreateRenderTargetView(renderTargetResource, &rtvDesc, rtvHandle.GetCPUHandle());
		rtvDescriptors.push_back(rtvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = config.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = config.MipLevels;

		DescriptorHeapHandle srvHandle = m_HeapManager->AllocatePersistentSRV();
		m_Device->CreateShaderResourceView(renderTargetResource, &srvDesc, srvHandle.GetCPUHandle());

		auto renderTexture = std::make_unique<RenderTexture>(renderTargetResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, rtvDescriptors, srvDesc, config);
		renderTexture->SetTransientDescriptor(srvHandle);
		renderTexture->SetPersistentDescriptor(srvHandle);
		return renderTexture;
	}

	DescriptorHeapHandle ResourceManager::UpdateSRVDescriptors(std::vector<GPUResource*> resources)
	{
		if (resources.empty())
			return DescriptorHeapHandle{};

		// Allocate a transient block in the shader-visible render-pass heap for this frame's
		// descriptor table, then copy from each resource's stable persistent CPU handle.
		DescriptorHeapHandle transientBlock = m_HeapManager->AllocateTransientSRVBlock(static_cast<UINT>(resources.size()));
		D3D12_CPU_DESCRIPTOR_HANDLE dstCPU = transientBlock.GetCPUHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE dstGPU = transientBlock.GetGPUHandle();
		UINT descriptorSize = m_HeapManager->GetRenderPassHeap().GetDescriptorSize();

		for (GPUResource* resource : resources)
		{
			DescriptorHeapHandle* persistent = resource->GetPersistentDescriptor();
			if (persistent && persistent->IsValid())
			{
				// Copy from persistent CPU slot into the contiguous transient table slot.
				m_Device->CopyDescriptorsSimple(1, dstCPU, persistent->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			else
			{
				// Fallback: write SRV directly if no persistent handle exists yet.
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = resource->GetSRVDesc();
				m_Device->CreateShaderResourceView(resource->GetResource(), &srvDesc, dstCPU);
			}

			// Expose the transient GPU handle so the pass can bind the table.
			DescriptorHeapHandle transientHandle;
			transientHandle.SetCPUHandle(dstCPU);
			transientHandle.SetGPUHandle(dstGPU);
			resource->SetTransientDescriptor(transientHandle);

			dstCPU.ptr += descriptorSize;
			dstGPU.ptr += descriptorSize;
		}

		return transientBlock;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> ResourceManager::CreatePipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
	{
		return m_PipelineStateCache->GetOrCreatePSO(desc);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> ResourceManager::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		return m_RootSignatureCache->GetOrCreateRootSignature(desc);
	}

	bool ResourceManager::ReloadChangedShaders()
	{
#ifndef _DEBUG
		return false;
#else
		for (auto& [name, shader] : m_Shaders)
		{
			if (shader->ReloadIfChanged())
			{
				m_ShaderGeneration++;
				m_PipelineStateCache->ClearCache();
				return true;
			}
		}
		return false;
#endif
	}

	void ResourceManager::BeginFrame(UINT slot)
	{
		ConstantBuffer::SetFrameSlot(slot);
		m_FrameConstantAllocator->BeginFrame(slot);
	}
}
