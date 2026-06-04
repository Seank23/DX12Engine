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
		enum class CookedFallbackMode
		{
			Auto,
			AllowGlbFallback,
			StrictCookedOnly,
		};

		ModelLoader();
		~ModelLoader();

		MeshAsset LoadObj(const std::string& filename);
		std::shared_ptr<ModelAsset> LoadCookedModel(const std::string& modelId);
		std::shared_ptr<ModelAsset> LoadGlb(const std::string& filename);

		static void SetCookedFallbackMode(CookedFallbackMode mode);
		static CookedFallbackMode GetCookedFallbackMode();

	private:
		void TryApplyCookedMeshLods(const std::string& modelName, ModelAsset& modelAsset);
	};
}

