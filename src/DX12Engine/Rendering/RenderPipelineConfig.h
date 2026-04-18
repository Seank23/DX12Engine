#pragma once
#include <vector>
#include <unordered_map>
#include <string>

namespace DX12Engine
{
	enum class ResourceSlot
	{
		Albedo,
		WorldNormal,
		ObjectNormal,
		Material,
		Position,
		Depth,
		Composite,
		Emissive,
		EnvironmentMap,
		Velocity,
		ReactiveMask,
	};

	enum class RenderPassType
	{
		ShadowMap,
		CubeShadowMap,
		Geometry,
		Lighting,
		Transparent,
		ScreenSpaceReflection,
		TAA,
		UI,
	};

	enum class PipelineResource
	{
		SceneColor,
		Depth,
		GBuffer,
		ReactiveMask,
		ShadowMap,
		CubeShadowMap,
		EnvironmentMap
	};

	enum class InputResourceType
	{
		SceneObjects,
		LightData,
		LightBuffer,
		Camera,
		EnvironmentMap,
		ShadowMap,
		CubeShadowMap,
		GBuffer,
		SceneColor,
		Depth,
		ReactiveMask,
		VertexShader,
		PixelShader,
	};

	static std::vector<InputResourceType> OrderedInputTypes = {
			InputResourceType::EnvironmentMap,
			InputResourceType::GBuffer,
			InputResourceType::ShadowMap,
			InputResourceType::CubeShadowMap,
			InputResourceType::SceneColor,
			InputResourceType::Depth,
			InputResourceType::ReactiveMask,
			InputResourceType::VertexShader,
			InputResourceType::PixelShader,
	};

	class GPUResource;

	struct ResourceBinding
	{
		InputResourceType InputType;
		PipelineResource Resource;
		std::vector<ResourceSlot> Slots;
	};

	struct ResourceWrite
	{
		PipelineResource Resource;
		RenderPassType SourcePass;
	};

	struct RenderPassConfig
	{
		RenderPassType Type;
		int Count = 1;
		std::unordered_map<InputResourceType, void*> InputResources;
		std::vector<ResourceBinding> ResourceBindings;
		std::vector<ResourceWrite> Writes;
	};

	struct RenderPipelineConfig
	{
		std::vector<RenderPassConfig> Passes;
	};
}
