#pragma once
#include <DirectXMath.h>
#include "../Asset/Animation.h"

namespace DX12Engine
{
	struct NodeTransformPose
	{
		DirectX::XMFLOAT3 Translation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
	};

	struct BlendedNodePose
	{
		NodeTransformPose Transform;
		float TranslationWeight = 0.0f;
		float RotationWeight = 0.0f;
		float ScaleWeight = 0.0f;
	};

	class AnimationUtils
	{
	public:
		static DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, float t)
		{
			DirectX::XMFLOAT3 result;
			DirectX::XMStoreFloat3(&result, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&from), DirectX::XMLoadFloat3(&to), t));
			return result;
		}

		static DirectX::XMFLOAT4 SlerpFloat4(const DirectX::XMFLOAT4& from, const DirectX::XMFLOAT4& to, float t)
		{
			DirectX::XMFLOAT4 result;
			DirectX::XMStoreFloat4(&result,
								   DirectX::XMQuaternionNormalize(
									   DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&from), DirectX::XMLoadFloat4(&to), t)));
			return result;
		}

		static NodeTransformPose DecomposeNodeTransform(const DirectX::XMFLOAT4X4& transform)
		{
			NodeTransformPose pose;
			DirectX::XMVECTOR scale;
			DirectX::XMVECTOR rotation;
			DirectX::XMVECTOR translation;
			if (DirectX::XMMatrixDecompose(&scale, &rotation, &translation, DirectX::XMLoadFloat4x4(&transform)))
			{
				DirectX::XMStoreFloat3(&pose.Scale, scale);
				DirectX::XMStoreFloat4(&pose.Rotation, DirectX::XMQuaternionNormalize(rotation));
				DirectX::XMStoreFloat3(&pose.Translation, translation);
			}
			return pose;
		}

		static DirectX::XMFLOAT4X4 ComposeNodeTransform(const NodeTransformPose& pose)
		{
			DirectX::XMFLOAT4X4 transform{};
			DirectX::XMStoreFloat4x4(
				&transform,
				DirectX::XMMatrixAffineTransformation(
					DirectX::XMLoadFloat3(&pose.Scale),
					DirectX::XMVectorZero(),
					DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&pose.Rotation)),
					DirectX::XMLoadFloat3(&pose.Translation)));
			return transform;
		}

		static bool TryGetSampleRange(const AnimationSampler& sampler, float animationTime, std::size_t& sampleIndex, std::size_t& nextSampleIndex, float& blendFactor)
		{
			if (sampler.Times.empty())
				return false;

			sampleIndex = 0;
			while (sampleIndex + 1 < sampler.Times.size() && sampler.Times[sampleIndex + 1] <= animationTime)
				++sampleIndex;

			nextSampleIndex = sampleIndex + 1 < sampler.Times.size()
								  ? sampleIndex + 1
								  : sampler.Times.size() - 1;

			const bool useStepInterpolation = sampler.Interpolation == AnimationInterpolation::Step || sampleIndex == nextSampleIndex;
			blendFactor = 0.0f;
			if (!useStepInterpolation)
			{
				const float sampleTime = sampler.Times[sampleIndex];
				const float nextSampleTime = sampler.Times[nextSampleIndex];
				if (nextSampleTime > sampleTime)
					blendFactor = (animationTime - sampleTime) / (nextSampleTime - sampleTime);
			}

			return true;
		}

		static void ApplyAnimationToTransforms(const DX12Engine::AnimationClip& clip, float animationTime, std::vector<DirectX::XMFLOAT4X4>& transforms)
		{
			for (const DX12Engine::AnimationChannel& channel : clip.Channels)
			{
				if (channel.SamplerIndex >= clip.Samplers.size() || channel.NodeIndex >= transforms.size())
					continue;

				const DX12Engine::AnimationSampler& sampler = clip.Samplers[channel.SamplerIndex];
				std::size_t sampleIndex = 0;
				std::size_t nextSampleIndex = 0;
				float blendFactor = 0.0f;
				if (!TryGetSampleRange(sampler, animationTime, sampleIndex, nextSampleIndex, blendFactor))
					continue;

				const bool useStepInterpolation = sampler.Interpolation == DX12Engine::AnimationInterpolation::Step || sampleIndex == nextSampleIndex;
				NodeTransformPose pose = DecomposeNodeTransform(transforms[channel.NodeIndex]);

				switch (channel.Path)
				{
				case DX12Engine::AnimationPath::Translation:
					if (sampleIndex >= sampler.Translations.size())
						continue;

					pose.Translation = sampler.Translations[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Translations.size())
						pose.Translation = LerpFloat3(pose.Translation, sampler.Translations[nextSampleIndex], blendFactor);
					break;

				case DX12Engine::AnimationPath::Rotation:
					if (sampleIndex >= sampler.Rotations.size())
						continue;

					pose.Rotation = sampler.Rotations[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Rotations.size())
						pose.Rotation = SlerpFloat4(pose.Rotation, sampler.Rotations[nextSampleIndex], blendFactor);
					break;

				case DX12Engine::AnimationPath::Scale:
					if (sampleIndex >= sampler.Scales.size())
						continue;

					pose.Scale = sampler.Scales[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Scales.size())
						pose.Scale = LerpFloat3(pose.Scale, sampler.Scales[nextSampleIndex], blendFactor);
					break;
				}

				transforms[channel.NodeIndex] = ComposeNodeTransform(pose);
			}
		}
	};
}