#pragma once
#include <memory>

namespace DX12Engine
{
	enum class ComponentType
	{
		Render,
		Physics
	};

	class GameObject;

	class Component
	{
	public:
		Component(GameObject* parent, ComponentType type);
		virtual ~Component();

		virtual void Init() = 0;
		virtual void Update(float ts, float elapsed) = 0;

	protected:
		GameObject* m_Parent;
		ComponentType m_Type;
	};
}

