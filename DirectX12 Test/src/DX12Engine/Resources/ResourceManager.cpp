#include "ResourceManager.h"
#include "../Heaps/DescriptorHeapManager.h"
#include "../Utils/EngineUtils.h"
#include "../Utils/Constants.h"

namespace DX12Engine
{
	static ResourceManager* s_Instance = nullptr;

	ResourceManager::ResourceManager()
	{
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

	void ResourceManager::Init(Microsoft::WRL::ComPtr<ID3D12Device> device)
	{
		m_Device = device;
	}

	std::unique_ptr<VertexBuffer> ResourceManager::CreateVertexBuffer(std::vector<Vertex>& vertices)
	{
		const UINT vertexBufferSize = sizeof(Vertex) * vertices.size();

		ID3D12Resource* vertexBufferResource = nullptr;

		// Create the vertex buffer resource in the GPU's default heap
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBufferResource)));

		// Copy the triangle vertices to the vertex buffer
		UINT8* vertexDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
		vertexBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
		memcpy(vertexDataBegin, &vertices[0], vertexBufferSize);
		vertexBufferResource->Unmap(0, nullptr);

		return std::make_unique<VertexBuffer>(vertexBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, sizeof(Vertex), vertexBufferSize);
	}

	std::unique_ptr<IndexBuffer> ResourceManager::CreateIndexBuffer(std::vector<UINT>& indices)
	{
		const UINT indexBufferSize = sizeof(UINT) * indices.size();

		ID3D12Resource* indexBufferResource = nullptr;

		// Create the index buffer resource in the GPU's default heap
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&indexBufferResource)));

		// Copy the indices to the index buffer
		void* indexDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
		indexBufferResource->Map(0, &readRange, &indexDataBegin);
		memcpy(indexDataBegin, &indices[0], indexBufferSize);
		indexBufferResource->Unmap(0, nullptr);
		return std::make_unique<IndexBuffer>(indexBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, DXGI_FORMAT_R32_UINT, indexBufferSize);
	}

	std::unique_ptr<ConstantBuffer> ResourceManager::CreateConstantBuffer(UINT bufferSize)
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

		DescriptorHeapHandle constantBufferHeapHandle = DescriptorHeapManager::GetInstance().GetNewSRVDescriptorHeapHandle();
		m_Device->CreateConstantBufferView(&constantBufferViewDesc, constantBufferHeapHandle.GetCPUHandle());
		std::unique_ptr<ConstantBuffer> constantBuffer = std::make_unique<ConstantBuffer>(constantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, alignedSize, constantBufferHeapHandle);
		constantBuffer->SetIsReady(true);
		return constantBuffer;
	}

	std::unique_ptr<Texture> ResourceManager::CreateTexture(const DirectX::ScratchImage* imageData)
	{
		const DirectX::TexMetadata& textureMetadata = imageData->GetMetadata();
		bool is3DTexture = textureMetadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;
		D3D12_RESOURCE_DESC textureDesc{};
		textureDesc.Format = textureMetadata.format;
		textureDesc.Width = textureMetadata.width;
		textureDesc.Height = textureMetadata.height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = is3DTexture ? textureMetadata.depth : textureMetadata.arraySize;
		textureDesc.MipLevels = textureMetadata.mipLevels;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = is3DTexture ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Alignment = 0;

		D3D12_HEAP_PROPERTIES defaultProperties;
		defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		defaultProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		defaultProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		defaultProperties.CreationNodeMask = 0;
		defaultProperties.VisibleNodeMask = 0;

		ID3D12Resource* newTextureResource = NULL;
		EngineUtils::ThrowIfFailed(m_Device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&newTextureResource)));

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
		if (textureMetadata.IsCubemap())
		{
			EngineUtils::Assert(textureMetadata.arraySize);

			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.TextureCube.MostDetailedMip = 0;
			shaderResourceViewDesc.TextureCube.MipLevels = (UINT)textureMetadata.mipLevels;
			shaderResourceViewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		}

		DescriptorHeapHandle srvHandle = DescriptorHeapManager::GetInstance().GetNewSRVDescriptorHeapHandle();
		m_Device->CreateShaderResourceView(newTextureResource, textureMetadata.IsCubemap() ? &shaderResourceViewDesc : NULL, srvHandle.GetCPUHandle());

		UINT64 textureMemorySize = 0;
		UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];
		UINT64 rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[MAX_TEXTURE_SUBRESOURCE_COUNT];
		const UINT64 numSubResources = textureMetadata.mipLevels * textureMetadata.arraySize;

		m_Device->GetCopyableFootprints(&textureDesc, 0, (UINT)numSubResources, 0, layouts, numRows, rowSizesInBytes, &textureMemorySize);
		UINT alignedSize = EngineUtils::AlignUINT(textureMemorySize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		ID3D12Resource* textureUploadResource = NULL;
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
		m_Device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadResource));

		UINT8* textureDataBegin = nullptr;
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
		textureUploadResource->Map(0, &readRange, reinterpret_cast<void**>(&textureDataBegin));

		for (UINT64 arrayIndex = 0; arrayIndex < textureMetadata.arraySize; arrayIndex++)
		{
			for (UINT64 mipIndex = 0; mipIndex < textureMetadata.mipLevels; mipIndex++)
			{
				const UINT64 subResourceIndex = mipIndex + (arrayIndex * textureMetadata.mipLevels);

				const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIndex];
				const UINT64 subResourceHeight = numRows[subResourceIndex];
				const UINT64 subResourcePitch = EngineUtils::AlignUINT(subResourceLayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
				const UINT64 subResourceDepth = subResourceLayout.Footprint.Depth;
				UINT8* destinationSubResourceMemory = textureDataBegin + subResourceLayout.Offset;

				for (UINT64 sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
				{
					const DirectX::Image* subImage = imageData->GetImage(mipIndex, arrayIndex, sliceIndex);
					const UINT8* sourceSubResourceMemory = subImage->pixels;

					for (UINT64 height = 0; height < subResourceHeight; height++)
					{
						memcpy(destinationSubResourceMemory, sourceSubResourceMemory, min(subResourcePitch, subImage->rowPitch));
						destinationSubResourceMemory += subResourcePitch;
						sourceSubResourceMemory += subImage->rowPitch;
					}
				}
			}
		}
		textureUploadResource->Unmap(0, nullptr);
		return std::make_unique<Texture>(newTextureResource, textureUploadResource, D3D12_RESOURCE_STATE_COPY_DEST, textureMetadata, layouts);
	}
}
