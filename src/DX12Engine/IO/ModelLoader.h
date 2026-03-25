#pragma once
#include <string>

namespace DX12Engine
{
	class MeshAsset;

	class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		MeshAsset LoadObj(const std::string& filename);
	};
}

