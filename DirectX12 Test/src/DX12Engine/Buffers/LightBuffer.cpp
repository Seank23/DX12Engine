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
		m_ConstantBuffer->Update(&Lights, sizeof(LightBufferData));
	}

	void LightBuffer::AddLight(Light light)
	{
		Lights.Lights[Lights.LightCount++] = light;
		Update();
	}
}
