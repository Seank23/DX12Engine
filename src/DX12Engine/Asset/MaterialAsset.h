#pragma once
#include "../Resources/Materials/Material.h"
#include <memory>
#include <string>
#include <utility>

namespace DX12Engine
{
	enum class AlphaMode
	{
		Opaque,
		Masked,
		Blend
	};

	class MaterialAsset
	{
	public:
		MaterialAsset() = default;
		explicit MaterialAsset(std::string name, std::shared_ptr<Material> material = nullptr)
			: m_Name(std::move(name)), m_Material(std::move(material))
		{
		}

		void SetName(std::string name) { m_Name = std::move(name); }
		const std::string& GetName() const { return m_Name; }

		void SetMaterial(std::shared_ptr<Material> material) { m_Material = std::move(material); }
		std::shared_ptr<Material> GetMaterialShared() const { return m_Material; }
		Material* GetMaterial() const { return m_Material.get(); }

		void SetAlphaMode(AlphaMode alphaMode) { m_AlphaMode = alphaMode; }
		AlphaMode GetAlphaMode() const { return m_AlphaMode; }

		void SetAlphaCutoff(float alphaCutoff) { m_AlphaCutoff = alphaCutoff; }
		float GetAlphaCutoff() const { return m_AlphaCutoff; }

		void SetDoubleSided(bool doubleSided) { m_DoubleSided = doubleSided; }
		bool IsDoubleSided() const { return m_DoubleSided; }

	private:
		std::string m_Name;
		std::shared_ptr<Material> m_Material;
		AlphaMode m_AlphaMode = AlphaMode::Opaque;
		float m_AlphaCutoff = 0.5f;
		bool m_DoubleSided = false;
	};
}
