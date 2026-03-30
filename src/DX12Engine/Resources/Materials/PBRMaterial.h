#pragma once
#include "./Material.h"
#include "../Texture.h"

namespace DX12Engine
{
	class PBRMaterial : public Material
	{
	public:
		PBRMaterial();
		~PBRMaterial();

		virtual Texture* GetTexture(TextureType type) override;
		virtual bool HasTexture(TextureType type) override;

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex) override;

		virtual void SetAllTextures(std::unordered_map<TextureType, std::shared_ptr<Texture>> textures) override;

		void SetAlbedoMap(std::shared_ptr<Texture> texture);
		void SetNormalMap(std::shared_ptr<Texture> texture);
		void SetMetallicMap(std::shared_ptr<Texture> texture);
		void SetRoughnessMap(std::shared_ptr<Texture> texture);
		void SetAOMap(std::shared_ptr<Texture> texture);

		void SetAlbedo(DirectX::XMFLOAT3 albedo);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetAO(float ao);
		void SetEmissive(DirectX::XMFLOAT3 emissive);
		void SetBaseColorAlpha(float alpha);
		void SetNormalScale(float scale);
		void SetOcclusionStrength(float strength);
		void SetAlphaMode(int mode);
		void SetAlphaCutoff(float cutoff);

	private:
		PBRMaterialData m_MaterialData;
		std::shared_ptr<Texture> m_AlbedoMap;
		std::shared_ptr<Texture> m_NormalMap;
		std::shared_ptr<Texture> m_MetallicMap;
		std::shared_ptr<Texture> m_RoughnessMap;
		std::shared_ptr<Texture> m_AOMap;
	};
}

