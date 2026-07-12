#pragma once

#include "DX12Engine/Input/InputController.h"
#include "DX12Engine/UI/UISystem.h"
#include "DX12Engine/Entity/GameObject.h"
#include "DX12Engine/Asset/Animation.h"

#include <unordered_map>
#include <string>

namespace DX12EngineDemo
{
	class InteractionController : public DX12Engine::InputController
	{
	public:
		InteractionController(std::shared_ptr<DX12Engine::UISystem> uiSystem);
		~InteractionController() = default;

		virtual void Update(float deltaTime) override;
		virtual void ProcessKeyInput(DX12Engine::InputCommand command, float deltaTime) override;
		virtual void ProcessMouseInput(DX12Engine::InputCommand command, float dX, float dY) override;

		void SetTarget(DX12Engine::GameObject* target) { m_Target = target; }

	private:
		void PlayAnimation(const std::string& animationName);

		DX12Engine::GameObject* m_Target = nullptr;

		std::unordered_map<std::string, DX12Engine::AnimationState> m_AnimationStates;
	};
}
