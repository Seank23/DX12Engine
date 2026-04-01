#pragma once
#include "../Resources/Materials/Material.h"
#include "MaterialTemplate.h"
#include <memory>
#include <string>
#include <utility>

namespace DX12Engine
{
	class MaterialAsset
	{
	public:
		MaterialAsset() = default;
		explicit MaterialAsset(std::string name, std::shared_ptr<Material> material = nullptr)
			: m_Name(std::move(name)), m_Material(std::move(material))
		{
			m_Template = std::make_shared<MaterialTemplate>();
		}

		void SetName(std::string name) { m_Name = std::move(name); }
		const std::string& GetName() const { return m_Name; }

		void SetMaterial(std::shared_ptr<Material> material) { m_Material = std::move(material); }
		std::shared_ptr<Material> GetMaterialShared() const { return m_Material; }
		Material* GetMaterial() const { return m_Material.get(); }

		// Template: defines which shaders and PSO policy this asset uses.
		// Null means the pass falls back to its own default PSO.
		void SetTemplate(std::shared_ptr<MaterialTemplate> tmpl) { m_Template = std::move(tmpl); }
		MaterialTemplate* GetTemplate() const { return m_Template.get(); }

		void SetAlphaMode(AlphaMode alphaMode) { m_AlphaMode = alphaMode; }
		AlphaMode GetAlphaMode() const { return m_AlphaMode; }

		void SetAlphaCutoff(float alphaCutoff) { m_AlphaCutoff = alphaCutoff; }
		float GetAlphaCutoff() const { return m_AlphaCutoff; }

		void SetDoubleSided(bool doubleSided) { m_DoubleSided = doubleSided; }
		bool IsDoubleSided() const { return m_DoubleSided; }

	private:
		std::string m_Name;
		std::shared_ptr<Material> m_Material;
		std::shared_ptr<MaterialTemplate> m_Template;
		AlphaMode m_AlphaMode = AlphaMode::Opaque;
		float m_AlphaCutoff = 0.5f;
		bool m_DoubleSided = false;
	};
}

