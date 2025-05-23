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

	protected:
		GameObject* m_Parent;
		ComponentType m_Type;
	};
}

