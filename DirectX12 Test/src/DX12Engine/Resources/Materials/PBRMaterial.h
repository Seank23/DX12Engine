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

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex, bool bindPipelineState = true) override;

		void SetAlbedoMap(std::shared_ptr<Texture> albedoMap) { m_AlbedoMap = albedoMap; }
		void SetNormalMap(std::shared_ptr<Texture> normalMap) { m_NormalMap = normalMap; }
		void SetMetallicMap(std::shared_ptr<Texture> metallicMap) { m_MetallicMap = metallicMap; }
		void SetRoughnessMap(std::shared_ptr<Texture> roughnessMap) { m_RoughnessMap = roughnessMap; }
		void SetAOMap(std::shared_ptr<Texture> aoMap) { m_AOMap = aoMap; }

		void SetAlbedo(DirectX::XMFLOAT3 albedo);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetAO(float ao);
		void SetEmissive(DirectX::XMFLOAT3 emissive);

	private:
		PBRMaterialData m_MaterialData;
		std::shared_ptr<Texture> m_AlbedoMap;
		std::shared_ptr<Texture> m_NormalMap;
		std::shared_ptr<Texture> m_MetallicMap;
		std::shared_ptr<Texture> m_RoughnessMap;
		std::shared_ptr<Texture> m_AOMap;
	};
}

