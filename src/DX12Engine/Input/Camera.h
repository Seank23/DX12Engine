#pragma once
#include "DirectXMath.h"
#include <DirectXCollision.h>
#include "InputController.h"

namespace DX12Engine
{
	class Camera : public InputController
	{
	public:
		Camera(float aspectRatio, float zNear, float zFar);
		~Camera();

		void SetAspectRatio(float aspectRatio);
		void SetClippingPlanes(float zNear, float zFar);
		void SetFOV(float fov);
		void SetSpeed(float speed) { m_Speed = speed; }

		virtual void Update(float deltaTime) override;
		virtual void ProcessKeyInput(InputCommand command, float deltaTime) override;
		virtual void ProcessMouseInput(InputCommand command, float dX, float dY) override;

		DirectX::XMMATRIX GetViewMatrix() const { return m_ViewMatrix; }
		DirectX::XMMATRIX GetProjectionMatrix() const { return m_ProjectionMatrix; }
		DirectX::XMFLOAT3 GetPosition() const { return m_Position; }
		float GetNearPlane() const { return m_ZNear; }
		float GetFarPlane() const { return m_ZFar; }
		float GetFOV() const { return m_FOV; }
		float GetAspectRatio() const { return m_AspectRatio; }

		void SetPosition(DirectX::XMFLOAT3 position);
		void SetRotation(float pitch, float yaw);

		void SetFrustum(const DirectX::BoundingFrustum& frustum) { m_Frustum = frustum; }
		DirectX::BoundingFrustum& GetFrustum() { return m_Frustum; }

	private:
		void UpdateProjectionMatrix();
		void UpdateViewMatrix();
		DirectX::XMVECTOR GetForwardVector();

		float m_AspectRatio;
		float m_ZNear;
		float m_ZFar;
		float m_FOV;
		float m_Speed;

		DirectX::XMFLOAT3 m_Position;
		float m_Pitch;
		float m_Yaw;

		bool m_hasChanged;

		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;

		DirectX::BoundingFrustum m_Frustum;
	};
}
