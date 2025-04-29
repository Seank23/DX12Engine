#include "Material.h"
#include "../ResourceManager.h"

namespace DX12Engine
{
	Material::Material()
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(MaterialData));
	}

	Material::~Material()
	{
	}

	void Material::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex)
	{
		commandList->SetGraphicsRootConstantBufferView((*startIndex)++, GetCBVAddress());
	}

	void Material::UpdateConstantBufferData(MaterialData materialData)
	{
		m_ConstantBuffer->Update(&materialData, sizeof(materialData));
	}
}
