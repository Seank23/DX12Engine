#pragma once
#include "MeshPrimitive.h"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace DX12Engine
{
	class MeshAsset
	{
	public:
		MeshAsset() = default;
		explicit MeshAsset(std::string name)
			: m_Name(std::move(name))
		{
		}

		MeshAsset(const MeshAsset&) = delete;
		MeshAsset& operator=(const MeshAsset&) = delete;
		MeshAsset(MeshAsset&&) noexcept = default;
		MeshAsset& operator=(MeshAsset&&) noexcept = default;
		~MeshAsset() = default;

		void SetName(std::string name) { m_Name = std::move(name); }
		const std::string& GetName() const { return m_Name; }

		std::size_t AddPrimitive(MeshPrimitive primitive)
		{
			m_Primitives.emplace_back(std::move(primitive));
			return m_Primitives.size() - 1;
		}

		void ClearPrimitives() { m_Primitives.clear(); }
		bool HasPrimitives() const { return !m_Primitives.empty(); }
		std::size_t GetPrimitiveCount() const { return m_Primitives.size(); }

		MeshPrimitive* GetPrimitive(std::size_t index)
		{
			if (index >= m_Primitives.size())
				return nullptr;
			return &m_Primitives[index];
		}

		const MeshPrimitive* GetPrimitive(std::size_t index) const
		{
			if (index >= m_Primitives.size())
				return nullptr;
			return &m_Primitives[index];
		}

		std::vector<MeshPrimitive>& GetPrimitives() { return m_Primitives; }
		const std::vector<MeshPrimitive>& GetPrimitives() const { return m_Primitives; }

	private:
		std::string m_Name;
		std::vector<MeshPrimitive> m_Primitives;
	};
}
