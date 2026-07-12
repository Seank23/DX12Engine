#include "Input/InteractionController.h"
#include "DX12Engine/Entity/AnimationComponent.h"

namespace DX12EngineDemo
{
	InteractionController::InteractionController(std::shared_ptr<DX12Engine::UISystem> uiSystem)
	{}

	void InteractionController::Update(float deltaTime)
	{}

	void InteractionController::ProcessKeyInput(DX12Engine::InputCommand command, float deltaTime)
	{
		switch (command)
		{
		case DX12Engine::InputCommand::Custom:
			if (IsKeyPressedThisFrame('H'))
			{
				PlayAnimation("Hood_Open");
			}
			if (IsKeyPressedThisFrame('1'))
			{
				PlayAnimation("Door_FL_Open");
			}
			if (IsKeyPressedThisFrame('2'))
			{
				PlayAnimation("Door_FR_Open");
			}
			if (IsKeyPressedThisFrame('3'))
			{
				PlayAnimation("Door_RL_Open");
			}
			if (IsKeyPressedThisFrame('4'))
			{
				PlayAnimation("Door_RR_Open");
			}
			if (IsKeyPressedThisFrame('5'))
			{
				PlayAnimation("Boot_Open");
			}
			break;
		}
	}

	void InteractionController::ProcessMouseInput(DX12Engine::InputCommand command, float dX, float dY)
	{}

	void InteractionController::PlayAnimation(const std::string& animationName)
	{
		if (m_Target)
		{
			auto animationComp = m_Target->GetComponent<DX12Engine::AnimationComponent>();
			if (!animationComp) return;

			auto& animationState = m_AnimationStates[animationName];
			animationComp->PlayAnimation(animationName, false, animationState == DX12Engine::AnimationState::Reversed);
			animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
		}
	}
}
