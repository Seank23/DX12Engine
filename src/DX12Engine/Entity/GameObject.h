#pragma once
#include "Component.h"
#include <vector>
#include <memory>

namespace DX12Engine
{
	class GameObject
	{
	public:
		GameObject() = default;
		~GameObject() = default;

		template<typename T>
		inline T* CreateComponent()
		{
			static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
			m_Components.emplace_back(std::make_unique<T>(this));
			return static_cast<T*>(m_Components.back().get());
		}

		template<typename T>
		inline T* GetComponent()
		{
			static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
			for (const auto& component : m_Components)
			{
				if (T* castedComponent = dynamic_cast<T*>(component.get()))
				{
					return castedComponent;
				}
			}
			return nullptr;
		}

	private:
		std::vector<std::unique_ptr<Component>> m_Components;
	};

	struct GameObjectContainer
	{
		template<typename T>
		std::vector<T*> GetAllComponents()
		{
			std::vector<T*> components;
			for (std::shared_ptr<GameObject> obj : Objects)
			{
				components.push_back(obj->GetComponent<T>());
			}
			return components;
		}

		void Add(std::shared_ptr<GameObject> gameObject)
		{
			Objects.push_back(gameObject);
		}

		std::vector<std::shared_ptr<GameObject>> Objects;
	};
}

