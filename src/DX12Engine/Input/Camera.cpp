#include "Camera.h"
#include <Windows.h>
#include <iostream>

namespace DX12Engine
{
	Camera::Camera(float aspectRatio, float zNear, float zFar)
		: InputController(), m_AspectRatio(aspectRatio), m_ZNear(zNear), m_ZFar(zFar), m_FOV(60.0f), m_Speed(5.0f),
		m_Position({ 0.0f, 0.0f, 0.0f }), m_Pitch(0.0f), m_Yaw(0.0f), m_hasChanged(false)
	{
		UpdateProjectionMatrix();
		UpdateViewMatrix();
	}

	Camera::~Camera()
	{
	}

	void Camera::Update(float deltaTime)
	{
		if (m_hasChanged)
		{
			UpdateViewMatrix();
			m_hasChanged = false;
		}
	}

	void Camera::ProcessKeyInput(InputCommand command, float deltaTime)
	{
		m_hasChanged = true;
		float speed = m_Speed * deltaTime;

		DirectX::XMVECTOR forwardVector = GetForwardVector();
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(forwardVector, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&m_Position);

		switch (command)
		{
		case InputCommand::MoveForward:
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorScale(forwardVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		case InputCommand::MoveBackward:
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorScale(forwardVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		case InputCommand::MoveLeft:
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorScale(rightVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		case InputCommand::MoveRight:
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorScale(rightVector, speed));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		case InputCommand::MoveUp:
			positionVector = DirectX::XMVectorAdd(positionVector, DirectX::XMVectorSet(0.0f, speed, 0.0f, 0.0f));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		case InputCommand::MoveDown:
			positionVector = DirectX::XMVectorSubtract(positionVector, DirectX::XMVectorSet(0.0f, speed, 0.0f, 0.0f));
			DirectX::XMStoreFloat3(&m_Position, positionVector);
			break;
		}
	}

	void Camera::ProcessMouseInput(InputCommand command, float dX, float dY)
	{
		m_hasChanged = true;
		float sensitivity = 0.005f;

		switch (command)
		{
		case InputCommand::Pan:
			m_Yaw -= dX * sensitivity;
			m_Pitch -= dY * sensitivity;
			m_Pitch = max(-DirectX::XM_PIDIV2, min(DirectX::XM_PIDIV2, m_Pitch));
			break;
		}
	}

	void Camera::SetAspectRatio(float aspectRatio)
	{
		m_AspectRatio = aspectRatio;
		UpdateProjectionMatrix();
	}

	void Camera::SetClippingPlanes(float zNear, float zFar)
	{
		m_ZNear = zNear;
		m_ZFar = zFar;
		UpdateProjectionMatrix();
	}

	void Camera::SetFOV(float fov)
	{
		m_FOV = fov;
		UpdateProjectionMatrix();
	}

	void Camera::SetPosition(DirectX::XMFLOAT3 position)
	{
		m_Position = position;
		UpdateViewMatrix();
	}

	void Camera::SetRotation(float pitch, float yaw)
	{
		m_Pitch = DirectX::XMConvertToRadians(pitch);
		m_Yaw = DirectX::XMConvertToRadians(yaw);
		UpdateViewMatrix();
	}

	void Camera::UpdateProjectionMatrix()
	{
		m_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(m_FOV),
			m_AspectRatio,
			m_ZNear,
			m_ZFar
		);
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
