#pragma once
#include "./Material.h"
#include "../Texture.h"

namespace DX12Engine
{
	class BasicMaterial : public Material
	{
	public:
		BasicMaterial(DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f });
		~BasicMaterial();

		void SetColor(DirectX::XMFLOAT4 color);
		void SetTexture(std::shared_ptr<Texture> texture);

		virtual Texture* GetTexture(TextureType type) override;
		virtual bool HasTexture(TextureType type) override;

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex) override;

	private:
		BasicMaterialData m_MaterialData;
		std::shared_ptr<Texture> m_Texture;
	};
}

