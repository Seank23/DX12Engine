#include "AnimationComponent.h"
#include "GameObject.h"
#include "../Asset/ModelInstance.h"
#include "RenderComponent.h"

namespace DX12Engine
{
	AnimationComponent::AnimationComponent(GameObject* parent)
		: Component(parent, ComponentType::Animation)
	{
		if (m_Parent && m_Parent->GetComponent<RenderComponent>())
			m_ModelInstance = m_Parent->GetComponent<RenderComponent>()->GetAssetShared();
	}

	AnimationComponent::~AnimationComponent()
	{
	}

	void AnimationComponent::Init()
	{
		if (!m_ModelInstance)
			return;

		m_ModelAsset = m_ModelInstance->GetModelAssetShared();
		m_ActiveAnimations.clear();
		m_BaseNodeTransforms.clear();
		ResetNodeTransforms();
	}

	void AnimationComponent::Update(float ts, float elapsed)
	{
		UpdateAnimation(ts);
	}

	void AnimationComponent::OnTransformChanged(TransformType type)
	{
	}

	void AnimationComponent::PlayAnimation(const std::string& animationName, bool loop, bool reverse)
	{
		if (!m_ModelAsset || !m_ModelAsset->DoesAnimationExist(animationName))
			return;

		const AnimationClip& clip = m_ModelAsset->GetAnimation(animationName);

		auto existingAnimation = std::find_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(), [&clip](const ActiveAnimationState& activeAnimation)
											  { return activeAnimation.Clip == &clip; });

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

	void AnimationComponent::UpdateAnimation(float deltaSeconds)
	{
		if (!m_ModelInstance || m_ActiveAnimations.empty())
			return;

		ResetNodeTransforms();
		auto& localTransforms = m_ModelInstance->GetLocalNodeTransforms();
		std::vector<BlendedNodePose> blendedNodePoses(localTransforms.size());

		for (std::size_t nodeIndex = 0; nodeIndex < localTransforms.size(); ++nodeIndex)
		{
			const NodeTransformPose transform = AnimationUtils::DecomposeNodeTransform(localTransforms[nodeIndex]);
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
				if (channel.SamplerIndex >= clip->Samplers.size() || channel.NodeIndex >= localTransforms.size())
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
							sampledTranslation = AnimationUtils::LerpFloat3(sampledTranslation, sampler.Translations[nextSampleIndex], blendFactor);

						if (nodePose.TranslationWeight <= 0.0f)
							nodePose.Transform.Translation = sampledTranslation;
						else
							nodePose.Transform.Translation = AnimationUtils::LerpFloat3(nodePose.Transform.Translation, sampledTranslation, weight / (nodePose.TranslationWeight + weight));

						nodePose.TranslationWeight += weight;
					}
					break;
				case AnimationPath::Rotation:
					if (sampleIndex >= sampler.Rotations.size())
						continue;

					{
						DirectX::XMFLOAT4 sampledRotation = sampler.Rotations[sampleIndex];
						if (!useStepInterpolation && nextSampleIndex < sampler.Rotations.size())
							sampledRotation = AnimationUtils::SlerpFloat4(sampledRotation, sampler.Rotations[nextSampleIndex], blendFactor);

						if (nodePose.RotationWeight <= 0.0f)
							nodePose.Transform.Rotation = sampledRotation;
						else
							nodePose.Transform.Rotation = AnimationUtils::SlerpFloat4(nodePose.Transform.Rotation, sampledRotation, weight / (nodePose.RotationWeight + weight));

						nodePose.RotationWeight += weight;
					}
					break;
				case AnimationPath::Scale:
					if (sampleIndex >= sampler.Scales.size())
						continue;

					{
						DirectX::XMFLOAT3 sampledScale = sampler.Scales[sampleIndex];
						if (!useStepInterpolation && nextSampleIndex < sampler.Scales.size())
							sampledScale = AnimationUtils::LerpFloat3(sampledScale, sampler.Scales[nextSampleIndex], blendFactor);

						if (nodePose.ScaleWeight <= 0.0f)
							nodePose.Transform.Scale = sampledScale;
						else
							nodePose.Transform.Scale = AnimationUtils::LerpFloat3(nodePose.Transform.Scale, sampledScale, weight / (nodePose.ScaleWeight + weight));

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
				AnimationUtils::ApplyAnimationToTransforms(*activeAnimation.Clip, finalTime, m_BaseNodeTransforms);
			}
		}

		for (std::size_t nodeIndex = 0; nodeIndex < blendedNodePoses.size(); ++nodeIndex)
		{
			const BlendedNodePose& nodePose = blendedNodePoses[nodeIndex];
			DirectX::XMStoreFloat4x4(
				&localTransforms[nodeIndex],
				DirectX::XMMatrixAffineTransformation(
					DirectX::XMLoadFloat3(&nodePose.Transform.Scale),
					DirectX::XMVectorZero(),
					DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&nodePose.Transform.Rotation)),
					DirectX::XMLoadFloat3(&nodePose.Transform.Translation)));
		}

		m_ActiveAnimations.erase(std::remove_if(m_ActiveAnimations.begin(), m_ActiveAnimations.end(), [](const ActiveAnimationState& activeAnimation)
												{
				if (!activeAnimation.Clip || activeAnimation.Loop)
					return false;

				return activeAnimation.Time <= 0.0f || activeAnimation.Time >= activeAnimation.Clip->Duration; }),
								 m_ActiveAnimations.end());

		m_ModelInstance->InvalidateNodeTransforms();
	}

	void AnimationComponent::ResetNodeTransforms()
	{
		m_ModelInstance->ResetNodeTransforms();

		if (!m_ModelAsset || !m_ModelInstance)
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

		auto& localTranforms = m_ModelInstance->GetLocalNodeTransforms();
		localTranforms = m_BaseNodeTransforms;
		m_ModelInstance->InvalidateNodeTransforms();
	}
}
