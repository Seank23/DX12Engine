#pragma once
#include <vector>
#include <unordered_map>
#include <string>

namespace DX12Engine
{
	enum class RenderPassType
	{
		ShadowMap,
		CubeShadowMap,
		Geometry,
		Lighting,
		Transparent,
		ScreenSpaceReflection,
		UI,
	};

	enum class InputResourceType
	{
		SceneObjects,
		LightData,
		LightBuffer,
		Camera,
		RenderTargets_ShadowMap,
		RenderTargets_CubeShadowMap,
		RenderTargets_Geometry,
		RenderTargets_Lighting,
		RenderTargets_Transparent,
		RenderTargets_SSR,
		EnvironmentMap,
		VertexShader,
		PixelShader,
	};

	class GPUResource;

	struct RenderPassConfig
	{
		RenderPassType Type;
		int Count = 1;
		std::unordered_map<InputResourceType, void*> InputResources;
	};

	struct RenderPipelineConfig
	{
		std::vector<RenderPassConfig> Passes;
	};
}
