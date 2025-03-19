#pragma once
#include "Material.h"
#include "../Texture.h"

namespace DX12Engine
{
	class SkyboxMaterial : public Material
	{
	public:
		SkyboxMaterial();
		~SkyboxMaterial();

		void SetTexture(std::shared_ptr<Texture> texture) { m_Texture = texture; }

		virtual Texture* GetTexture(TextureType type = TextureType::Albedo) override { return m_Texture.get(); }
		virtual bool HasTexture(TextureType type = TextureType::Albedo) override { return m_Texture != nullptr; }

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex) override;

	private:
		BasicMaterialData m_MaterialData;
		std::shared_ptr<Texture> m_Texture;
	};
}

