#pragma once
#include <DirectXMath.h>
#include "./ConstantBuffer.h"
#include "../../Resources/Light.h"
#include <unordered_map>

namespace DX12Engine
{
    struct LightBufferData
    {
        int LightCount = 0;
        DirectX::XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
        LightData Lights[4];
    };

	class LightBuffer
	{
    public:
		LightBuffer();
		~LightBuffer();

		void Update();
		void AddLight(std::shared_ptr<Light> light);
        D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

        Light* GetLight(int index) { return m_Lights[index].get(); }
        int GetLightCount() { return m_Lights.size(); }

        std::vector<Light*> GetAllLights();
        std::vector<Light*> GetLightsByType(std::vector<LightType> types);

    private:
        std::unordered_map<LightType, std::vector<int>> m_LightsByTypeMap;
        LightBufferData m_LightsBufferData;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
        std::vector<std::shared_ptr<Light>> m_Lights;
	};
}
