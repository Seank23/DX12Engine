#include "LightBuffer.h"
#include "../../Resources/ResourceManager.h"
#include "../../Utils/EngineUtils.h"
#include <iostream>

namespace DX12Engine
{
	LightBuffer::LightBuffer()
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(LightBufferData));
		m_LightsByTypeMap[LightType::Directional] = {};
		m_LightsByTypeMap[LightType::Spot] = {};
		m_LightsByTypeMap[LightType::Point] = {};
	}

	LightBuffer::~LightBuffer()
	{
	}

	void LightBuffer::Update()
	{
		for (int i = 0; i < m_LightsBufferData.LightCount && i < 4; i++)
			m_LightsBufferData.Lights[i] = m_Lights[i]->GetLightData();

		m_ConstantBuffer->Update(&m_LightsBufferData, sizeof(LightBufferData));
	}

	void LightBuffer::AddLight(std::shared_ptr<Light> light)
	{
		if (m_Lights.size() >= 4)
		{
			std::cout << "LightBuffer can only hold 4 lights. Cannot add more lights." << std::endl;
			return;
		}
		// Assign the light's slice within its own type's shadow atlas: spot lights index the
		// shadowMaps Texture2DArray, point lights index the shadowCubeMaps cube array. Passes
		// render each type in this same insertion order, so the slot doubles as the sample index.
		LightType type = light->GetType();
		light->SetShadowMapIndex(static_cast<int>(m_LightsByTypeMap[type].size()));
		m_Lights.push_back(light);
		m_LightsByTypeMap[type].push_back(m_LightsBufferData.LightCount);
		m_LightsBufferData.Lights[m_LightsBufferData.LightCount++] = light->GetLightData();
		Update();
	}

	std::vector<Light*> LightBuffer::GetAllLights()
	{
		return EngineUtils::VectorSharedPtrToPtrs(m_Lights);
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
