#include "PBRMaterial.h"
#include "../../Resources/ResourceManager.h"

namespace DX12Engine
{
	PBRMaterial::PBRMaterial()
		: Material()
	{
	}

	PBRMaterial::~PBRMaterial()
	{
	}

	Texture* PBRMaterial::GetTexture(TextureType type)
	{
		switch (type)
		{
		case TextureType::Albedo:
			return m_AlbedoMap.get();
		case TextureType::Normal:
			return m_NormalMap.get();
		case TextureType::Metallic:
			return m_MetallicMap.get();
		case TextureType::Roughness:
			return m_RoughnessMap.get();
		case TextureType::AOMap:
			return m_AOMap.get();
		default:
			return nullptr;
		}
	}

	bool PBRMaterial::HasTexture(TextureType type)
	{
		switch (type)
		{
		case TextureType::Albedo:
			return m_AlbedoMap != nullptr;
		case TextureType::Normal:
			return m_NormalMap != nullptr;
		case TextureType::Metallic:
			return m_MetallicMap != nullptr;
		case TextureType::Roughness:
			return m_RoughnessMap != nullptr;
		case TextureType::AOMap:
			return m_AOMap != nullptr;
		default:
			return false;
		}
	}
	void PBRMaterial::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex)
	{
		Material::Bind(commandList, startIndex);
		if (HasTexture(TextureType::Albedo))
			commandList->SetGraphicsRootDescriptorTable((*startIndex)++, m_AlbedoMap->GetGPUHandle());
	}

	void PBRMaterial::SetAlbedo(DirectX::XMFLOAT3 albedo)
	{
		m_MaterialData.Albedo = albedo;
		UpdateConstantBufferData(m_MaterialData);
	}

	void PBRMaterial::SetMetallic(float metallic)
	{
		m_MaterialData.Metallic = metallic;
		UpdateConstantBufferData(m_MaterialData);
	}

	void PBRMaterial::SetRoughness(float roughness)
	{
		m_MaterialData.Roughness = roughness;
		UpdateConstantBufferData(m_MaterialData);
	}

	void PBRMaterial::SetAO(float ao)
	{
		m_MaterialData.AO = ao;
		UpdateConstantBufferData(m_MaterialData);
	}

	void PBRMaterial::SetEmissive(DirectX::XMFLOAT3 emissive)
	{
		m_MaterialData.Emissive = emissive;
		UpdateConstantBufferData(m_MaterialData);
	}
}
