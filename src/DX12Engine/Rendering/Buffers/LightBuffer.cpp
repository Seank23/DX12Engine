#include "LightBuffer.h"
#include "../../Resources/ResourceManager.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	LightBuffer::LightBuffer()
	{
		m_LightsByTypeMap[LightType::Directional] = {};
		m_LightsByTypeMap[LightType::Spot] = {};
		m_LightsByTypeMap[LightType::Point] = {};
	}

	LightBuffer::~LightBuffer()
	{
	}

	void LightBuffer::Update()
	{
		m_LightData.clear();
		m_LightData.reserve(m_Lights.size());
		m_DirectionalLightCount = 0;

		for (const auto& light : m_Lights)
		{
			if (light->GetType() == LightType::Directional)
			{
				m_LightData.push_back(light->GetLightData());
				m_DirectionalLightCount++;
			}
		}

		for (const auto& light : m_Lights)
		{
			if (light->GetType() != LightType::Directional)
				m_LightData.push_back(light->GetLightData());
		}
	}

	void LightBuffer::AddLight(std::shared_ptr<Light> light)
	{
		LightType type = light->GetType();
		uint32_t* shadowSlots = nullptr;
		if (type == LightType::Spot && m_SpotShadowSlots < MAX_SHADOW_CASTING_SPOT_LIGHTS)
			shadowSlots = &m_SpotShadowSlots;
		else if (type == LightType::Point && m_PointShadowSlots < MAX_SHADOW_CASTING_POINT_LIGHTS)
			shadowSlots = &m_PointShadowSlots;
		
		bool castsShadows = shadowSlots != nullptr && light->GetCastsShadows();
		light->SetShadowMapIndex(castsShadows ? (int)(*shadowSlots)++ : -1);
		m_Lights.push_back(light);
		m_LightsByTypeMap[type].push_back(static_cast<int>(m_Lights.size() - 1));
		m_LightData.push_back(light->GetLightData());
	}

	std::vector<Light*> LightBuffer::GetAllLights()
	{
		return EngineUtils::VectorSharedPtrToPtrs(m_Lights);
	}

	std::vector<Light*> LightBuffer::GetShadowCastingLightsByType(std::vector<LightType> types)
	{
		std::vector<Light*> shadowCasters;
		for (Light* light : GetLightsByType(types))
		{
			if (light->GetLightData().ShadowMapIndex >= 0)
				shadowCasters.push_back(light);
		}
		return shadowCasters;
	}

	std::vector<Light*> LightBuffer::GetLightsByType(std::vector<LightType> types)
	{
		std::vector<Light*> lightsByType;
		for (LightType type : types)
		{
			std::vector<int> indices = m_LightsByTypeMap[type];
			for (int index : indices)
				lightsByType.push_back(m_Lights[index].get());
		}
		return lightsByType;
	}
}
