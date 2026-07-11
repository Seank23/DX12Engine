#include "ModelInstance.h"

#include <algorithm>
#include <cmath>

namespace
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

	DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, float t)
	{
		DirectX::XMFLOAT3 result;
		DirectX::XMStoreFloat3(&result, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&from), DirectX::XMLoadFloat3(&to), t));
		return result;
	}

	DirectX::XMFLOAT4 SlerpFloat4(const DirectX::XMFLOAT4& from, const DirectX::XMFLOAT4& to, float t)
	{
		DirectX::XMFLOAT4 result;
		DirectX::XMStoreFloat4(&result,
			DirectX::XMQuaternionNormalize(
				DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&from), DirectX::XMLoadFloat4(&to), t)));
		return result;
	}

	NodeTransformPose DecomposeNodeTransform(const DirectX::XMFLOAT4X4& transform)
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

	DirectX::XMFLOAT4X4 ComposeNodeTransform(const NodeTransformPose& pose)
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

	bool TryGetSampleRange(const DX12Engine::AnimationSampler& sampler, float animationTime, std::size_t& sampleIndex, std::size_t& nextSampleIndex, float& blendFactor)
	{
		if (sampler.Times.empty())
			return false;

		sampleIndex = 0;
		while (sampleIndex + 1 < sampler.Times.size() && sampler.Times[sampleIndex + 1] <= animationTime)
			++sampleIndex;

		nextSampleIndex = sampleIndex + 1 < sampler.Times.size()
			? sampleIndex + 1
			: sampler.Times.size() - 1;

		const bool useStepInterpolation = sampler.Interpolation == DX12Engine::AnimationInterpolation::Step || sampleIndex == nextSampleIndex;
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

	void ApplyAnimationToTransforms(const DX12Engine::AnimationClip& clip, float animationTime, std::vector<DirectX::XMFLOAT4X4>& transforms)
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
}

namespace DX12Engine
{
	ModelInstance::ModelInstance()
	{
		DirectX::XMStoreFloat4x4(&m_WorldTransform, DirectX::XMMatrixIdentity());
	}

	ModelInstance::ModelInstance(std::shared_ptr<ModelAsset> modelAsset)
		: ModelInstance()
	{
		SetModelAsset(std::move(modelAsset));
	}

	void ModelInstance::SetModelAsset(std::shared_ptr<ModelAsset> modelAsset)
	{
		m_ModelAsset = std::move(modelAsset);
		m_ActiveAnimations.clear();
		m_BaseNodeTransforms.clear();
		ResetNodeTransforms();
		if (!m_ModelAsset)
		{
			m_MaterialOverrides.clear();
			return;
		}

		const std::size_t materialCount = m_ModelAsset->GetMaterialCount();
		if (m_MaterialOverrides.size() < materialCount)
			m_MaterialOverrides.resize(materialCount);
	}

	void ModelInstance::ResetNodeTransforms()
	{
		m_LocalNodeTransforms.clear();
		m_WorldNodeTransforms.clear();
		m_LocalNodeTransforms.shrink_to_fit();

		if (!m_ModelAsset)
		{
			m_BaseNodeTransforms.clear();
			return;
		}

		const auto& nodes = m_ModelAsset->GetNodes();
		if (m_BaseNodeTransforms.size() != nodes.size())
		{
			m_BaseNodeTransforms.clear();
			m_BaseNodeTransforms.reserve(nodes.size());
			for (const ModelNode& node : nodes)
				m_BaseNodeTransforms.push_back(node.LocalTransform);
		}

		m_LocalNodeTransforms = m_BaseNodeTransforms;
		m_WorldNodeTransforms.resize(nodes.size());

		m_NodeTransformsDirty = true;
	}

	void ModelInstance::UpdateNodeWorldTransforms()
	{
		if (!m_ModelAsset || !m_NodeTransformsDirty)
			return;

		const auto& nodes = m_ModelAsset->GetNodes();

		for (std::size_t i = 0; i < nodes.size(); ++i)
		{
			DirectX::XMMATRIX local = DirectX::XMLoadFloat4x4(&m_LocalNodeTransforms[i]);

			if (nodes[i].ParentIndex >= 0 && static_cast<std::size_t>(nodes[i].ParentIndex) < i)
			{
				DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&m_WorldNodeTransforms[nodes[i].ParentIndex]);
				DirectX::XMStoreFloat4x4(&m_WorldNodeTransforms[i], parentWorld * local);
			}
			else
			{
				DirectX::XMStoreFloat4x4(&m_WorldNodeTransforms[i], local);
			}
		}
		m_NodeTransformsDirty = false;
	}

	DirectX::XMMATRIX ModelInstance::GetNodeWorldTransform(std::size_t nodeIndex)
	{
		UpdateNodeWorldTransforms();

		if (nodeIndex >= m_WorldNodeTransforms.size())
			return DirectX::XMMatrixIdentity();

		return DirectX::XMLoadFloat4x4(&m_WorldNodeTransforms[nodeIndex]);
	}

	void ModelInstance::PlayAnimation(const std::string& animationName, bool loop, bool reverse)
	{
		if (!m_ModelAsset || !m_ModelAsset->DoesAnimationExist(animationName))
			return;

		const AnimationClip& clip = m_ModelAsset->GetAnimation(animationName);

		auto existingAnimation = std::find_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(),
			[&clip](const ActiveAnimationState& activeAnimation)
			{
				return activeAnimation.Clip == &clip;
			});

		if (existingAnimation != m_ActiveAnimations.end())
		{
			existingAnimation->Time = reverse ? clip.Duration : 0.0f;
			existingAnimation->Loop = loop;
			existingAnimation->Reverse = reverse;
			existingAnimation->Weight = 1.0f;
			return;
		}

		m_ActiveAnimations.push_back({ &clip, reverse ? clip.Duration : 0.0f, 1.0f, loop, reverse });
	}

	void ModelInstance::UpdateAnimation(float deltaSeconds)
	{
		if (!m_ModelAsset || m_ActiveAnimations.empty())
			return;

		ResetNodeTransforms();
		std::vector<BlendedNodePose> blendedNodePoses(m_LocalNodeTransforms.size());

		for (std::size_t nodeIndex = 0; nodeIndex < m_LocalNodeTransforms.size(); ++nodeIndex)
		{
			const NodeTransformPose transform = DecomposeNodeTransform(m_LocalNodeTransforms[nodeIndex]);
			blendedNodePoses[nodeIndex].Transform = transform;
		}

		for (ActiveAnimationState& activeAnimation : m_ActiveAnimations)
		{
			const AnimationClip* clip = activeAnimation.Clip;
			if (!clip)
				continue;

			activeAnimation.Time += deltaSeconds * m_AnimationSpeed * (activeAnimation.Reverse ? -1.0f : 1.0f);

			if (activeAnimation.Loop && clip->Duration > 0.0f)
			{
				activeAnimation.Time = std::fmod(activeAnimation.Time, clip->Duration);
				if (activeAnimation.Time < 0.0f)
					activeAnimation.Time += clip->Duration;
			}
			else if (activeAnimation.Time > clip->Duration)
				activeAnimation.Time = clip->Duration;
			else if (activeAnimation.Time < 0.0f)
				activeAnimation.Time = 0.0f;

			const float animationTime = activeAnimation.Time;

			for (const AnimationChannel& channel : clip->Channels)
			{
				if (channel.SamplerIndex >= clip->Samplers.size() || channel.NodeIndex >= m_LocalNodeTransforms.size())
					continue;

				const AnimationSampler& sampler = clip->Samplers[channel.SamplerIndex];
				if (sampler.Times.empty())
					continue;

				std::size_t sampleIndex = 0;
				while (sampleIndex + 1 < sampler.Times.size() && sampler.Times[sampleIndex + 1] <= animationTime)
					++sampleIndex;

				const std::size_t nextSampleIndex = sampleIndex + 1 < sampler.Times.size()
					? sampleIndex + 1
					: sampler.Times.size() - 1;
				const bool useStepInterpolation = sampler.Interpolation == AnimationInterpolation::Step || sampleIndex == nextSampleIndex;

				float blendFactor = 0.0f;
				if (!useStepInterpolation)
				{
					const float sampleTime = sampler.Times[sampleIndex];
					const float nextSampleTime = sampler.Times[nextSampleIndex];
					if (nextSampleTime > sampleTime)
						blendFactor = (animationTime - sampleTime) / (nextSampleTime - sampleTime);
				}

				BlendedNodePose& nodePose = blendedNodePoses[channel.NodeIndex];
				const float weight = activeAnimation.Weight;

				switch (channel.Path)
				{
				case AnimationPath::Translation:
					if (sampleIndex >= sampler.Translations.size())
						continue;

					{
						DirectX::XMFLOAT3 sampledTranslation = sampler.Translations[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Translations.size())
							sampledTranslation = LerpFloat3(sampledTranslation, sampler.Translations[nextSampleIndex], blendFactor);

						if (nodePose.TranslationWeight <= 0.0f)
							nodePose.Transform.Translation = sampledTranslation;
						else
							nodePose.Transform.Translation = LerpFloat3(nodePose.Transform.Translation, sampledTranslation, weight / (nodePose.TranslationWeight + weight));

						nodePose.TranslationWeight += weight;
					}
					break;
				case AnimationPath::Rotation:
					if (sampleIndex >= sampler.Rotations.size())
						continue;

					{
						DirectX::XMFLOAT4 sampledRotation = sampler.Rotations[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Rotations.size())
							sampledRotation = SlerpFloat4(sampledRotation, sampler.Rotations[nextSampleIndex], blendFactor);

						if (nodePose.RotationWeight <= 0.0f)
							nodePose.Transform.Rotation = sampledRotation;
						else
							nodePose.Transform.Rotation = SlerpFloat4(nodePose.Transform.Rotation, sampledRotation, weight / (nodePose.RotationWeight + weight));

						nodePose.RotationWeight += weight;
					}
					break;
				case AnimationPath::Scale:
					if (sampleIndex >= sampler.Scales.size())
						continue;

					{
						DirectX::XMFLOAT3 sampledScale = sampler.Scales[sampleIndex];
					if (!useStepInterpolation && nextSampleIndex < sampler.Scales.size())
							sampledScale = LerpFloat3(sampledScale, sampler.Scales[nextSampleIndex], blendFactor);

						if (nodePose.ScaleWeight <= 0.0f)
							nodePose.Transform.Scale = sampledScale;
						else
							nodePose.Transform.Scale = LerpFloat3(nodePose.Transform.Scale, sampledScale, weight / (nodePose.ScaleWeight + weight));

						nodePose.ScaleWeight += weight;
					}
					break;
				}
			}

			if (!activeAnimation.Clip || activeAnimation.Loop)
				continue;

			if (activeAnimation.Time <= 0.0f || activeAnimation.Time >= activeAnimation.Clip->Duration)
			{
				const float finalTime = activeAnimation.Reverse ? 0.0f : activeAnimation.Clip->Duration;
				ApplyAnimationToTransforms(*activeAnimation.Clip, finalTime, m_BaseNodeTransforms);
			}
		}

		for (std::size_t nodeIndex = 0; nodeIndex < blendedNodePoses.size(); ++nodeIndex)
		{
			const BlendedNodePose& nodePose = blendedNodePoses[nodeIndex];
			DirectX::XMStoreFloat4x4(
				&m_LocalNodeTransforms[nodeIndex],
				DirectX::XMMatrixAffineTransformation(
					DirectX::XMLoadFloat3(&nodePose.Transform.Scale),
					DirectX::XMVectorZero(),
					DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&nodePose.Transform.Rotation)),
					DirectX::XMLoadFloat3(&nodePose.Transform.Translation)));
		}

		m_ActiveAnimations.erase(std::remove_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(),
			[](const ActiveAnimationState& activeAnimation)
			{
				if (!activeAnimation.Clip || activeAnimation.Loop)
					return false;

				return activeAnimation.Time <= 0.0f || activeAnimation.Time >= activeAnimation.Clip->Duration;
			}), m_ActiveAnimations.end());
		m_NodeTransformsDirty = true;
	}

	void ModelInstance::SetMaterialOverride(std::size_t materialIndex, std::shared_ptr<MaterialAsset> materialAsset)
	{
		if (m_MaterialOverrides.size() <= materialIndex)
			m_MaterialOverrides.resize(materialIndex + 1);

		m_MaterialOverrides[materialIndex] = std::move(materialAsset);
	}

	void ModelInstance::ClearMaterialOverride(std::size_t materialIndex)
	{
		if (materialIndex < m_MaterialOverrides.size())
			m_MaterialOverrides[materialIndex].reset();
	}

	void ModelInstance::ClearMaterialOverrides()
	{
		for (auto& overrideMaterial : m_MaterialOverrides)
			overrideMaterial.reset();
	}

	MaterialAsset* ModelInstance::ResolveMaterial(std::size_t materialIndex) const
	{
		if (materialIndex < m_MaterialOverrides.size() && m_MaterialOverrides[materialIndex] != nullptr)
			return m_MaterialOverrides[materialIndex].get();

		if (!m_ModelAsset)
			return nullptr;

		return m_ModelAsset->GetMaterial(materialIndex);
	}
}
