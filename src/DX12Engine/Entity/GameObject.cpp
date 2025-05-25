#include "GameObject.h"

namespace DX12Engine
{
	GameObject::GameObject()
		: m_Position({ 0.0f, 0.0f, 0.0f }),
		m_Scale({ 1.0f, 1.0f, 1.0f }),
		m_Rotation(DirectX::XMQuaternionIdentity())
	{
	}

	void GameObject::Init()
	{
		for (const auto& component : m_Components)
			component->Init();
	}

	void GameObject::Update(float ts, float elapsed)
	{
		for (const auto& component : m_Components)
			component->Update(ts, elapsed);
	}

	void GameObject::Move(DirectX::XMFLOAT3 movement)
	{
		m_Position = DirectX::XMVectorAdd(m_Position, DirectX::XMLoadFloat3(&movement));
	}

	void GameObject::Scale(DirectX::XMFLOAT3 scale)
	{
		m_Scale = DirectX::XMLoadFloat3(&scale);
	}

	void GameObject::Rotate(DirectX::XMFLOAT3 rotation)
	{
		DirectX::XMFLOAT3 toRadians({ DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) });
		DirectX::XMVECTOR quaternion = DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&toRadians));
		m_Rotation = DirectX::XMQuaternionMultiply(m_Rotation, quaternion);
	}
}