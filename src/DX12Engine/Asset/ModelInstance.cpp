#include "ModelInstance.h"

#include <algorithm>

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

		if (!m_ModelAsset)
			return;

		const auto& nodes = m_ModelAsset->GetNodes();
		m_LocalNodeTransforms.reserve(nodes.size());
		m_WorldNodeTransforms.resize(nodes.size());

		for (const ModelNode& node : nodes)
			m_LocalNodeTransforms.push_back(node.LocalTransform);

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

	void ModelInstance::PlayAnimation(const std::string& animationName, bool loop)
	{
		if (!m_ModelAsset || !m_ModelAsset->DoesAnimationExist(animationName))
			return;

		m_CurrentAnimation = animationName;
		m_AnimationTime = 0.0f;
		m_LoopAnimation = loop;
		m_PlayingAnimation = true;
	}

	void ModelInstance::UpdateAnimation(float deltaSeconds)
	{
		if (!m_PlayingAnimation || !m_ModelAsset || m_CurrentAnimation.empty())
			return;

		const AnimationClip& clip = m_ModelAsset->GetAnimation(m_CurrentAnimation);

		m_AnimationTime += deltaSeconds * m_AnimationSpeed;

		if (m_LoopAnimation && clip.Duration > 0.0f)
			m_AnimationTime = std::fmod(m_AnimationTime, clip.Duration);
		else if (m_AnimationTime > clip.Duration)
			m_AnimationTime = clip.Duration;

		ResetNodeTransforms();

		for (const AnimationChannel& channel : clip.Channels)
		{
			if (channel.SamplerIndex >= clip.Samplers.size() || channel.NodeIndex >= m_LocalNodeTransforms.size())
				continue;

			const AnimationSampler& sampler = clip.Samplers[channel.SamplerIndex];
			if (sampler.Times.empty())
				continue;

			std::size_t sampleIndex = 0;
			while (sampleIndex + 1 < sampler.Times.size() && sampler.Times[sampleIndex + 1] <= m_AnimationTime)
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
					blendFactor = (m_AnimationTime - sampleTime) / (nextSampleTime - sampleTime);
			}

			DirectX::XMVECTOR scale;
			DirectX::XMVECTOR rotation;
			DirectX::XMVECTOR translation;
			DirectX::XMMATRIX localTransform = DirectX::XMLoadFloat4x4(&m_LocalNodeTransforms[channel.NodeIndex]);
			if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, localTransform))
			{
				scale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
				rotation = DirectX::XMQuaternionIdentity();
				translation = DirectX::XMVectorZero();
			}

			switch (channel.Path)
			{
			case AnimationPath::Translation:
				if (sampleIndex >= sampler.Translations.size())
					continue;

				translation = DirectX::XMLoadFloat3(&sampler.Translations[sampleIndex]);
				if (!useStepInterpolation && nextSampleIndex < sampler.Translations.size())
				{
					translation = DirectX::XMVectorLerp(
						translation,
						DirectX::XMLoadFloat3(&sampler.Translations[nextSampleIndex]),
						blendFactor);
				}
				break;
			case AnimationPath::Rotation:
				if (sampleIndex >= sampler.Rotations.size())
					continue;

				rotation = DirectX::XMLoadFloat4(&sampler.Rotations[sampleIndex]);
				if (!useStepInterpolation && nextSampleIndex < sampler.Rotations.size())
				{
					rotation = DirectX::XMQuaternionSlerp(
						rotation,
						DirectX::XMLoadFloat4(&sampler.Rotations[nextSampleIndex]),
						blendFactor);
				}
				rotation = DirectX::XMQuaternionNormalize(rotation);
				break;
			case AnimationPath::Scale:
				if (sampleIndex >= sampler.Scales.size())
					continue;

				scale = DirectX::XMLoadFloat3(&sampler.Scales[sampleIndex]);
				if (!useStepInterpolation && nextSampleIndex < sampler.Scales.size())
				{
					scale = DirectX::XMVectorLerp(
						scale,
						DirectX::XMLoadFloat3(&sampler.Scales[nextSampleIndex]),
						blendFactor);
				}
				break;
			}
			DirectX::XMStoreFloat4x4(&m_LocalNodeTransforms[channel.NodeIndex], DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rotation, translation));
		}
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
