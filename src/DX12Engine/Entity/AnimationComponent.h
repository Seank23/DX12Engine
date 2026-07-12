#pragma once
#include "Component.h"
#include "../Utils/AnimationUtils.h"

namespace DX12Engine
{
	class ModelInstance;
	class ModelAsset;

	class AnimationComponent : public Component
	{
	public:
		AnimationComponent(GameObject* parent);
		~AnimationComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;

		int GetActiveAnimationCount() const { return static_cast<int>(m_ActiveAnimations.size()); }

		virtual void OnTransformChanged(TransformType type) override;

		void PlayAnimation(const std::string& animationName, bool loop = true, bool reverse = false);

	private:
		void UpdateAnimation(float deltaSeconds);
		void ResetNodeTransforms();

		std::shared_ptr<ModelInstance> m_ModelInstance;
		std::shared_ptr<ModelAsset> m_ModelAsset;

		std::vector<DirectX::XMFLOAT4X4> m_BaseNodeTransforms;

		std::vector<ActiveAnimationState> m_ActiveAnimations;
		float m_AnimationSpeed = 1.0f;
	};
}
