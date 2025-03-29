#include "Light.h"

namespace DX12Engine
{
	Light::Light()
	{
        m_LightData.ViewProjMatrix = DirectX::XMMatrixIdentity();
	}

	Light::~Light()
    {
	}

    void Light::SetPosition(DirectX::XMFLOAT3 position)
    {
        m_LightData.Position = position;
        UpdateViewProjMatrix();
    }

    void Light::SetDirection(DirectX::XMFLOAT3 direction)
    {
        m_LightData.Direction = direction;
        UpdateViewProjMatrix();
    }

    void Light::SetSpotAngle(float angle)
    {
        m_LightData.SpotAngle = DirectX::XMConvertToRadians(angle);
        UpdateViewProjMatrix();
    }

    void Light::UpdateViewProjMatrix()
    {
        DirectX::XMVECTOR lightDir;
        DirectX::XMVECTOR lightPos;
        DirectX::XMMATRIX lightView;
        DirectX::XMMATRIX lightProj;
        DirectX::XMFLOAT3 centre(0.0f, 0.0f, 0.0f);
        switch (m_LightData.Type)
        {
        case (int)LightType::Directional:
            lightDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_LightData.Direction));
            lightPos = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&centre), DirectX::XMVectorScale(lightDir, 10.0f));
            lightView = DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMLoadFloat3(&centre), UpDirection);
            lightProj = DirectX::XMMatrixOrthographicLH(15.0f, 15.0f, 1.f, 50.0f);
            m_LightData.ViewProjMatrix = DirectX::XMMatrixMultiply(lightView, lightProj);
            break;
        case (int)LightType::Spot:
            lightDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_LightData.Direction));
            lightPos = DirectX::XMLoadFloat3(&m_LightData.Position);
            DirectX::XMVECTOR target = DirectX::XMVectorAdd(lightPos, lightDir);
            lightView = DirectX::XMMatrixLookAtLH(lightPos, target, UpDirection);
            lightProj = DirectX::XMMatrixPerspectiveFovLH(m_LightData.SpotAngle * 2.0f, 1.0, 1.0f, 20.0f);
            m_LightData.ViewProjMatrix = DirectX::XMMatrixMultiply(lightView, lightProj);
            break;
        }
    }
}