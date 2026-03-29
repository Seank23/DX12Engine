#pragma once
#include <string>
#include <memory>

namespace DX12Engine
{
	class MeshAsset;
	class ModelAsset;

	class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		MeshAsset LoadObj(const std::string& filename);
		std::shared_ptr<ModelAsset> LoadGlb(const std::string& filename);
	};
}

