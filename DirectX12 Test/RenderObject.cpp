#include "RenderObject.h"

namespace DX12Engine
{
	RenderObject::RenderObject(Microsoft::WRL::ComPtr<ID3D12Device> device, Mesh mesh)
		: m_Mesh(mesh), m_ModelMatrix(DirectX::XMMatrixIdentity())
	{
		m_VertexBuffer.SetData(device, mesh.Vertices);
		m_IndexBuffer.SetData(device, mesh.Indices);
		
		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));
		device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_ConstantBufferRes)
		);
	}

	RenderObject::~RenderObject()
	{
	}

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix)
	{
		m_ConstantBufferData.WVPMatrix = wvpMatrix;
	}
}
