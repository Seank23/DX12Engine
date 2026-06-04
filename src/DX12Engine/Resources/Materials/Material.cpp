#include "Material.h"
#include "../ResourceManager.h"

namespace DX12Engine
{
	Material::Material()
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(MaterialData));
	}

	Material::Material(UINT bufferSize)
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(bufferSize);
	}

	Material::~Material()
	{
	}

	void Material::Bind(ID3D12GraphicsCommandList* commandList, int cbSlot, int textureSlot)
	{
		commandList->SetGraphicsRootConstantBufferView(cbSlot, GetCBVAddress());
	}

	void Material::UpdateConstantBufferData(const void* data, UINT size)
	{
		m_ConstantBuffer->Update(const_cast<void*>(data), size);
	}
}
