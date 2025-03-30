#pragma once
#include <DirectXMath.h>
#include "./ConstantBuffer.h"
#include "../Resources/Light.h"

namespace DX12Engine
{
    struct LightBufferData
    {
        int LightCount = 0;
        DirectX::XMFLOAT3 Padding;
        LightData Lights[4];
    };

	class LightBuffer
	{
    public:
		LightBuffer();
		~LightBuffer();

		void Update();
		void AddLight(Light* light);
        D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }
        Light* GetLight(int index) { return m_Lights[index]; }
        std::vector<Light*> GetAllLights() { return m_Lights; }
        int GetLightCount() { return m_Lights.size(); }

    private:
        LightBufferData m_LightsBufferData;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
        std::vector<Light*> m_Lights;
	};
}
