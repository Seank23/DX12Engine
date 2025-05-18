#include "LightBuffer.h"
#include "../../Resources/ResourceManager.h"
#include "../../Utils/EngineUtils.h"

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
		for (int i = 0; i < m_LightsBufferData.LightCount; i++)
			m_LightsBufferData.Lights[i] = m_Lights[i]->GetLightData();

		m_ConstantBuffer->Update(&m_LightsBufferData, sizeof(LightBufferData));
	}

	void LightBuffer::AddLight(std::shared_ptr<Light> light)
	{
		m_Lights.push_back(light);
		m_LightsByTypeMap[light->GetType()].push_back(m_LightsBufferData.LightCount);
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
			std::vector indices = m_LightsByTypeMap[type];
			for (int index : indices)
				lightsByType.push_back(m_Lights[index].get());
		}
		return lightsByType;
	}
}
