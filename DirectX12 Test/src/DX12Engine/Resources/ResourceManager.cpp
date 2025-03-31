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
		m_Shaders.insert({ "BasicLighting_VS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\BasicLighting_VS.hlsl", "vertex") });
		m_Shaders.insert({ "BasicLighting_PS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\BasicLighting_PS.hlsl", "pixel") });
		m_Shaders.insert({ "PBRLighting_VS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\PBRLighting_VS.hlsl", "vertex") });
		m_Shaders.insert({ "PBRLighting_PS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\PBRLighting_PS.hlsl", "pixel") });
		m_Shaders.insert({ "Skybox_VS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\Skybox_VS.hlsl", "vertex") });
		m_Shaders.insert({ "Skybox_PS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\Skybox_PS.hlsl", "pixel") });
		m_Shaders.insert({ "ShadowMap_VS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\ShadowMap_VS.hlsl", "vertex") });
		m_Shaders.insert({ "ShadowCubeMap_VS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\ShadowCubeMap_VS.hlsl", "vertex") });
		m_Shaders.insert({ "ShadowCubeMap_PS", std::make_unique<Shader>("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\src\\DX12Engine\\Shaders\\ShadowCubeMap_PS.hlsl", "pixel") });
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

	Microsoft::WRL::ComPtr<ID3D12PipelineState> ResourceManager::CreatePipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
	{
		return m_PipelineStateCache->GetOrCreatePSO(desc);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> ResourceManager::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		return m_RootSignatureCache->GetOrCreateRootSignature(desc);
	}
}
