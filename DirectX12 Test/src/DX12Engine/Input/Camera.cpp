#include "Camera.h"
#include <Windows.h>

namespace DX12Engine
{
	Camera::Camera(float aspectRatio, float zNear, float zFar)
		: m_Position({ 0.0f, 0.0f, 0.0f }), m_Pitch(0.0f), m_Yaw(0.0f)
	{
		m_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(60.0f),
			aspectRatio,
			zNear,
			zFar
		);
		UpdateViewMatrix();
	}

	Camera::~Camera()
	{
	}

	void Camera::Update(float deltaTime)
	{
	}

	void Camera::ProcessKeyboardInput(float deltaTime)
	{
		float speed = 5.0f * deltaTime;

		DirectX::XMVECTOR forwardVector = GetForwardVector();
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(forwardVector, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		if (GetAsyncKeyState('W') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorScale(forwardVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		if (GetAsyncKeyState('S') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorScale(forwardVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		if (GetAsyncKeyState('A') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorScale(rightVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		if (GetAsyncKeyState('D') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorScale(rightVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		if (GetAsyncKeyState('Q') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorSet(0.0f, speed, 0.0f, 0.0f));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		if (GetAsyncKeyState('E') & 0x8000)
		{
			DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorSet(0.0f, speed, 0.0f, 0.0f));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
		}
		UpdateViewMatrix();
	}

	void Camera::ProcessMouseInput(float dX, float dY)
	{
		float sensitivity = 0.002f;
		m_Yaw -= dX * sensitivity;
		m_Pitch -= dY * sensitivity;
		m_Pitch = max(-DirectX::XM_PIDIV2, min(DirectX::XM_PIDIV2, m_Pitch));
		UpdateViewMatrix();
	}

	void Camera::SetPosition(DirectX::XMFLOAT3 position)
	{
		m_Position = position;
		UpdateViewMatrix();
	}

	void Camera::SetRotation(float pitch, float yaw)
	{
		m_Pitch = pitch;
		m_Yaw = yaw;
		UpdateViewMatrix();
	}

	void Camera::UpdateViewMatrix()
	{
		DirectX::XMVECTOR forwardVector = GetForwardVector();
		DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);
		m_ViewMatrix = DirectX::XMMatrixLookToLH(positionVector, forwardVector, upVector);
	}

	DirectX::XMVECTOR Camera::GetForwardVector()
	{
		DirectX::XMFLOAT3 forward;
		forward.x = cosf(m_Yaw) * cosf(m_Pitch);
		forward.y = sinf(m_Pitch);
		forward.z = sinf(m_Yaw) * cosf(m_Pitch);
		return DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&forward));
	}
}
