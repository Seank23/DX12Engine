#pragma once
#include "DirectXMath.h"

namespace DX12Engine
{
	class Camera
	{
	public:
		Camera(float aspectRatio, float zNear, float zFar);
		~Camera();

		void Update(float deltaTime);
		void ProcessKeyboardInput(float deltaTime);
		void ProcessMouseInput(float dX, float dY);

		DirectX::XMMATRIX GetViewMatrix() const { return m_ViewMatrix; }
		DirectX::XMMATRIX GetProjectionMatrix() const { return m_ProjectionMatrix; }
		DirectX::XMFLOAT3 GetPosition() const { return m_Position; }

		void SetPosition(DirectX::XMFLOAT3 position);
		void SetRotation(float pitch, float yaw);

	private:
		void UpdateViewMatrix();
		DirectX::XMVECTOR GetForwardVector();

		DirectX::XMFLOAT3 m_Position;
		float m_Pitch;
		float m_Yaw;

		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;
	};
}

