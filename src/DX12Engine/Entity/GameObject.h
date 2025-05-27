#pragma once
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>

namespace DX12Engine
{
	class GameObject
	{
	public:
		GameObject();
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

		virtual void Init();
		virtual void Update(float ts, float elapsed);

		void Move(DirectX::XMVECTOR movement);
		void Scale(DirectX::XMVECTOR scale);
		void Rotate(DirectX::XMFLOAT3 rotation);

		DirectX::XMVECTOR GetPosition() const { return m_Position; }
		DirectX::XMVECTOR GetScale() const { return m_Scale; }
		DirectX::XMVECTOR GetRotation() const { return m_Rotation; }

	private:
		std::vector<std::unique_ptr<Component>> m_Components;

		DirectX::XMVECTOR m_Position;
		DirectX::XMVECTOR m_Scale;
		DirectX::XMVECTOR m_Rotation;
	};

	struct GameObjectContainer
	{
		template<typename T>
		std::vector<T*> GetAllComponents()
		{
			std::vector<T*> components;
			auto values = GetAll();
			for (std::shared_ptr<GameObject> obj : values)
			{
				components.push_back(obj->GetComponent<T>());
			}
			return components;
		}

		void Add(std::string name, std::shared_ptr<GameObject> gameObject)
		{
			Objects[name] = gameObject;
		}

		std::shared_ptr<GameObject> Get(std::string name)
		{
			auto it = Objects.find(name);
			if (it != Objects.end())
			{
				return it->second;
			}
			return nullptr;
		}

		std::vector<std::shared_ptr<GameObject>> GetAll()
		{
			std::vector<std::shared_ptr<GameObject>> values;
			for (auto& obj : Objects)
			{
				values.push_back(obj.second);
			}
			return values;
		}

		void Update(float ts, float elapsed)
		{
			for (auto& obj : Objects)
			{
				obj.second->Update(ts, elapsed);
			}
		}

		std::unordered_map<std::string, std::shared_ptr<GameObject>> Objects;
	};
}

