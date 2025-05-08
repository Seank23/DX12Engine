#include "BasicMaterial.h"
#include "../../Resources/ResourceManager.h"

namespace DX12Engine
{
	BasicMaterial::BasicMaterial(DirectX::XMFLOAT4 color)
		: Material()
	{
		SetColor(color);
	}

	BasicMaterial::~BasicMaterial()
	{
	}

	void BasicMaterial::SetColor(DirectX::XMFLOAT4 color)
	{
		m_MaterialData.BaseColor = color;
		UpdateConstantBufferData(m_MaterialData);
	}

	void BasicMaterial::SetTexture(std::shared_ptr<Texture> texture)
	{
		m_Texture = texture;
		m_MaterialData.HasTexture = 1;
		UpdateConstantBufferData(m_MaterialData);
	}

	Texture* BasicMaterial::GetTexture(TextureType type)
	{
		switch (type)
		{
		case TextureType::Albedo:
			return m_Texture.get();
		default:
			return nullptr;
		}
	}

	bool BasicMaterial::HasTexture(TextureType type)
	{
		switch (type)
		{
		case TextureType::Albedo:
			return m_Texture != nullptr;
		default:
			return false;
		}
	}

	void BasicMaterial::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex)
	{
		Material::Bind(commandList, startIndex);
		if (HasTexture(TextureType::Albedo))
			commandList->SetGraphicsRootDescriptorTable(*startIndex, m_Texture->GetGPUHandle());
	}

	void BasicMaterial::SetAllTextures(std::unordered_map<TextureType, std::shared_ptr<Texture>> textures)
	{
		for (auto& texture : textures)
		{
			switch (texture.first)
			{
			case TextureType::Albedo:
				SetTexture(texture.second);
				break;
			}
		}
	}
}	
