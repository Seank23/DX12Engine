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

	void Light::SetRange(float range)
	{
		m_LightData.Range = range;
		UpdateViewProjMatrix();
	}

	float Light::GetFarPlane()
	{
		switch (GetType())
		{
		case LightType::Directional:
			return 50.0f;
		case LightType::Point:
			return m_LightData.Range;
		case LightType::Spot:
			return m_LightData.Range;
		default:
			return 50.0f;
		}
	}

	void Light::UpdateViewProjMatrix()
	{
		DirectX::XMVECTOR lightDir;
		DirectX::XMVECTOR lightPos;
		DirectX::XMMATRIX lightView;
		DirectX::XMMATRIX lightProj;
		DirectX::XMFLOAT3 centre(0.0f, 0.0f, 0.0f);
		m_LightData.Padding.x = GetFarPlane();
		switch (m_LightData.Type)
		{
		case (int)LightType::Directional:
			lightDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_LightData.Direction));
			lightPos = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&centre), DirectX::XMVectorScale(lightDir, 20.0f));
			lightView = DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMLoadFloat3(&centre), UpDirection);
			lightProj = DirectX::XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, GetFarPlane());
			m_LightData.ViewProjMatrix = DirectX::XMMatrixMultiply(lightView, lightProj);
			break;
		case (int)LightType::Spot:
		{
			lightDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_LightData.Direction));
			lightPos = DirectX::XMLoadFloat3(&m_LightData.Position);
			DirectX::XMVECTOR target = DirectX::XMVectorAdd(lightPos, lightDir);
			// A spot aimed straight down is colinear with the default up vector, which makes
			// XMMatrixLookAtLH produce a NaN basis - and straight down is the common case.
			DirectX::XMVECTOR up = fabsf(DirectX::XMVectorGetY(lightDir)) > 0.999f
									   ? DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
									   : UpDirection;
			lightView = DirectX::XMMatrixLookAtLH(lightPos, target, up);
			// Cover the whole lit cone plus a margin, so PCF taps at the cone edge still land
			// inside the map instead of falling through to the unshadowed border.
			float spotFov = DirectX::XMMin(m_LightData.SpotAngle * 2.0f + DirectX::XMConvertToRadians(10.0f), DirectX::XMConvertToRadians(170.0f));
			lightProj = DirectX::XMMatrixPerspectiveFovLH(spotFov, 1.0f, 0.5f, DirectX::XMMax(GetFarPlane(), 1.0f));
			m_LightData.ViewProjMatrix = DirectX::XMMatrixMultiply(lightView, lightProj);
			break;
		}
		case (int)LightType::Point:
			lightProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.0f, 0.2f, GetFarPlane());
			m_LightData.ViewProjMatrix = lightProj;
			break;
		}
	}
}