#pragma once
#include <memory>

namespace DX12Engine
{
	enum class ComponentType
	{
		Render,
		Physics,
		Collider
	};

	enum TransformType
	{
		Position,
		Scale,
		Rotation
	};

	class GameObject;
	class ModelInstance;
	class ColliderComponent;

	class IColliderListener
	{
	public:
		virtual ~IColliderListener() = default;
		virtual void OnColliderChanged(ColliderComponent* colliderComponent) = 0;
	};

	class Component
	{
	public:
		Component(GameObject* parent, ComponentType type, bool skipUpdate = false);
		virtual ~Component();

		virtual void Init() = 0;
		virtual void Update(float ts, float elapsed) = 0;

		virtual void OnTransformChanged(TransformType type) = 0;

		inline bool ShouldSkipUpdate() const { return m_SkipUpdate; }

	protected:
		GameObject* m_Parent;
		ComponentType m_Type;
		bool m_SkipUpdate;
	};
}

