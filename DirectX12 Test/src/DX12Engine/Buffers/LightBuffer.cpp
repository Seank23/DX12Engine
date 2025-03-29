#include "LightBuffer.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	LightBuffer::LightBuffer()
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(LightBufferData));
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

	void LightBuffer::AddLight(Light* light)
	{
		m_Lights.push_back(light);
		m_LightsBufferData.Lights[m_LightsBufferData.LightCount++] = light->GetLightData();
		Update();
	}
}
