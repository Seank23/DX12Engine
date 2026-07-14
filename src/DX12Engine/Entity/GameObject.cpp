#include "GameObject.h"

namespace DX12Engine
{
	GameObject::GameObject()
		: m_Position({ 0.0f, 0.0f, 0.0f }),
		  m_Scale({ 1.0f, 1.0f, 1.0f }),
		  m_Rotation(DirectX::XMQuaternionIdentity()),
		  m_ModelMatrix(DirectX::XMMatrixIdentity())
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
		{
			if (!component->ShouldSkipUpdate())
				component->Update(ts, elapsed);
		}
	}

	void GameObject::Move(DirectX::XMVECTOR movement)
	{
		m_Position = DirectX::XMVectorAdd(m_Position, movement);
		UpdateModelMatrix();
		for (const auto& component : m_Components)
			component->OnTransformChanged(TransformType::Position);
	}

	void GameObject::Scale(DirectX::XMVECTOR scale)
	{
		m_Scale = scale;
		UpdateModelMatrix();
		for (const auto& component : m_Components)
			component->OnTransformChanged(TransformType::Scale);
	}

	void GameObject::Rotate(DirectX::XMFLOAT3 rotation)
	{
		DirectX::XMFLOAT3 toRadians({ DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) });
		DirectX::XMVECTOR quaternion = DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&toRadians));
		m_Rotation = DirectX::XMQuaternionMultiply(m_Rotation, quaternion);
		UpdateModelMatrix();
		for (const auto& component : m_Components)
			component->OnTransformChanged(TransformType::Rotation);
	}

	void GameObject::SetRotationQuaternion(DirectX::XMVECTOR rotation)
	{
		m_Rotation = rotation;
		UpdateModelMatrix();
		for (const auto& component : m_Components)
			component->OnTransformChanged(TransformType::Rotation);
	}

	void GameObject::UpdateModelMatrix()
	{
		m_ModelMatrix = DirectX::XMMatrixScalingFromVector(m_Scale) * DirectX::XMMatrixRotationQuaternion(m_Rotation) * DirectX::XMMatrixTranslationFromVector(m_Position);
	}

	void GameObject::RegisterColliderListener(IColliderListener* listener)
	{
		if (!listener)
			return;

		auto listenerIt = std::find(m_ColliderListeners.begin(), m_ColliderListeners.end(), listener);
		if (listenerIt == m_ColliderListeners.end())
			m_ColliderListeners.push_back(listener);
	}

	void GameObject::UnregisterColliderListener(IColliderListener* listener)
	{
		auto listenerIt = std::remove(m_ColliderListeners.begin(), m_ColliderListeners.end(), listener);
		m_ColliderListeners.erase(listenerIt, m_ColliderListeners.end());
	}

	void GameObject::DispatchColliderChanged(ColliderComponent* colliderComponent)
	{
		for (IColliderListener* listener : m_ColliderListeners)
		{
			if (listener)
				listener->OnColliderChanged(colliderComponent);
		}
	}
}