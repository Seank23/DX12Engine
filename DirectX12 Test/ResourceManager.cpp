#include "ResourceManager.h"

namespace DX12Engine
{
	static ResourceManager* s_Instance = nullptr;

	ResourceManager::ResourceManager()
	{
	}

	ResourceManager::~ResourceManager()
	{
	}

	std::unique_ptr<VertexBuffer> ResourceManager::CreateVertexBuffer(std::vector<Vertex>& vertices)
	{
		const UINT vertexBufferSize = sizeof(Vertex) * vertices.size();

		ID3D12Resource* vertexBufferResource = nullptr;

		// Create the vertex buffer resource in the GPU's default heap
		auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBufferResource));

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
		m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&indexBufferResource));

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

		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBufferResource)
		);
		return std::make_unique<ConstantBuffer>(constantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ, bufferSize);
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
}
