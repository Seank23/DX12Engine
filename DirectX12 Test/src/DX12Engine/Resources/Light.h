#pragma once
#include <DirectXMath.h>

namespace DX12Engine
{
    enum class LightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    struct LightData
    {
        int Type = 0;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float Intensity = 1.0f;
        DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
        float Range = 10.0f;
        DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
        float SpotAngle = 0.01f;
        DirectX::XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
        DirectX::XMMATRIX ViewProjMatrix;
    };

	class Light
	{
	public:
        Light();
        ~Light();

        void SetType(int type) { m_LightData.Type = type; }
        void SetIntensity(float intensity) { m_LightData.Intensity = intensity; }
        void SetRange(float range) { m_LightData.Range = range; }
        void SetColor(DirectX::XMFLOAT3 color) { m_LightData.Color = color; }
        void SetPosition(DirectX::XMFLOAT3 position);
        void SetDirection(DirectX::XMFLOAT3 direction);
        void SetSpotAngle(float angle);

        LightData& GetLightData() { return m_LightData; }
        DirectX::XMMATRIX GetViewProjMatrix() { return m_LightData.ViewProjMatrix; }

    private:
        void UpdateViewProjMatrix();

        LightData m_LightData;

        DirectX::XMVECTOR UpDirection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	};
}

