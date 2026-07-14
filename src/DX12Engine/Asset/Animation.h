#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>

namespace DX12Engine
{
	enum class AnimationPath
	{
		Translation,
		Rotation,
		Scale
	};
	enum class AnimationInterpolation
	{
		Linear,
		Step,
		CubicSpline
	};

	struct AnimationSampler
	{
		AnimationInterpolation Interpolation = AnimationInterpolation::Linear;
		std::vector<float> Times;
		std::vector<DirectX::XMFLOAT3> Translations;
		std::vector<DirectX::XMFLOAT4> Rotations;
		std::vector<DirectX::XMFLOAT3> Scales;
	};

	struct AnimationChannel
	{
		uint32_t NodeIndex = 0;
		AnimationPath Path = AnimationPath::Translation;
		uint32_t SamplerIndex = 0;
	};

	struct AnimationClip
	{
		std::string Name;
		float Duration = 0.0f;
		std::vector<AnimationSampler> Samplers;
		std::vector<AnimationChannel> Channels;
	};

	enum class AnimationState
	{
		Stopped,
		Playing,
		Paused,
		Reversed
	};

	struct ActiveAnimationState
	{
		const AnimationClip* Clip = nullptr;
		float Time = 0.0f;
		float Weight = 1.0f;
		bool Loop = true;
		bool Reverse = false;
	};
}