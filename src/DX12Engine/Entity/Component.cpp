#include "Component.h"
#include "GameObject.h"
#include <stdexcept>

namespace DX12Engine
{
    Component::Component(GameObject* parent, ComponentType type, bool skipUpdate)
		: m_Type(type), m_SkipUpdate(skipUpdate)
	{
		// Ensure the parent is not null
		if (parent == nullptr)
		{
			throw std::invalid_argument("Parent GameObject cannot be null");
		}
		m_Parent = parent;
	}

	Component::~Component()
	{
	}
}
