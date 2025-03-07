#pragma once
#include <DirectXMath.h>
#include "./ConstantBuffer.h"

namespace DX12Engine
{
    enum class LightType 
    {
        Directional,
        Point,
        Spot
    };

    struct Light 
    {
        int Type = 0;       // 0 = Directional, 1 = Point, 2 = Spot
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float Intensity = 1.0f;              
        DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
        float Range = 10.0f;
        DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
        float SpotAngle = 0.0f;              // Spot cone angle (for spotlights)
        DirectX::XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
    };

    struct LightBufferData
    {
        int LightCount = 0;
        DirectX::XMFLOAT3 Padding;
        Light Lights[4];
    };

	class LightBuffer
	{
    public:
		LightBuffer();
		~LightBuffer();

		void Update();
		void AddLight(Light light);
        D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }
        Light* GetLight(int index) { return &(Lights.Lights[index]); }

    private:
        LightBufferData Lights;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
	};
}
