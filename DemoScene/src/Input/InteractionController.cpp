#include "Input/InteractionController.h"
#include "DX12Engine/Entity/RenderComponent.h"

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
				if (m_Target)
				{ 
					auto& animationState = m_AnimationStates["Hood_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Hood_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			if (IsKeyPressedThisFrame('1'))
			{
				if (m_Target)
				{
					auto& animationState = m_AnimationStates["Door_FL_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Door_FL_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			if (IsKeyPressedThisFrame('2'))
			{
				if (m_Target)
				{
					auto& animationState = m_AnimationStates["Door_FR_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Door_FR_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			if (IsKeyPressedThisFrame('3'))
			{
				if (m_Target)
				{
					auto& animationState = m_AnimationStates["Door_RL_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Door_RL_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			if (IsKeyPressedThisFrame('4'))
			{
				if (m_Target)
				{
					auto& animationState = m_AnimationStates["Door_RR_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Door_RR_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			if (IsKeyPressedThisFrame('5'))
			{
				if (m_Target)
				{
					auto& animationState = m_AnimationStates["Boot_Open"];
					m_Target->GetComponent<DX12Engine::RenderComponent>()->PlayAnimation("Boot_Open", false, animationState == DX12Engine::AnimationState::Reversed);
					animationState = (animationState == DX12Engine::AnimationState::Reversed) ? DX12Engine::AnimationState::Playing : DX12Engine::AnimationState::Reversed;
				}
			}
			break;
		}
	}

	void InteractionController::ProcessMouseInput(DX12Engine::InputCommand command, float dX, float dY)
	{}
}
