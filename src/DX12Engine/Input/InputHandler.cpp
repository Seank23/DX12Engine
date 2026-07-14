#include "InputHandler.h"
#include <windowsx.h>
#include "InputController.h"
#include "../UI/UISystem.h"

namespace DX12Engine
{
	InputHandler::InputHandler(std::shared_ptr<UISystem> uiSystem)
		: m_UISystem(uiSystem)
	{
		m_CommandMap = {
			{ InputCommand::MoveForward, 'W' },
			{ InputCommand::MoveBackward, 'S' },
			{ InputCommand::MoveLeft, 'A' },
			{ InputCommand::MoveRight, 'D' },
			{ InputCommand::MoveUp, 'E' },
			{ InputCommand::MoveDown, 'Q' },
			{ InputCommand::Pan, VK_RBUTTON },
			{ InputCommand::Interact, VK_LBUTTON },
		};
	}

	InputHandler::~InputHandler()
	{
	}

	void InputHandler::ProcessInput(float deltaTime)
	{
		if (m_UISystem->WantsKeyboardCapture())
			return;
		for (const auto& [command, key] : m_CommandMap)
		{
			if (InputController::IsKeyPressed(key))
			{
				for (auto* controller : m_InputControllers)
					controller->ProcessKeyInput(command, deltaTime);
			}
		}
		for (auto* controller : m_InputControllers)
			controller->ProcessKeyInput(InputCommand::Custom, deltaTime);
	}

	void InputHandler::HandleMouseMovement(HWND hwnd, LPARAM lParam)
	{
		const bool isPanning = InputController::IsKeyPressed(VK_RBUTTON);
		if (m_UISystem->WantsMouseCapture() && !isPanning)
			return;

		int mouseX = GET_X_LPARAM(lParam);
		int mouseY = GET_Y_LPARAM(lParam);

		if (m_FirstMouse)
		{
			m_LastMouseX = (float)mouseX;
			m_LastMouseY = (float)mouseY;
			m_FirstMouse = false;
		}

		float deltaX = (float)(mouseX - m_LastMouseX);
		float deltaY = (float)(mouseY - m_LastMouseY);

		m_LastMouseX = (float)mouseX;
		m_LastMouseY = (float)mouseY;

		for (const auto& [command, key] : m_CommandMap)
		{
			if (InputController::IsKeyPressed(key))
			{
				for (auto* controller : m_InputControllers)
					controller->ProcessMouseInput(command, deltaX, deltaY);
			}
		}
	}

	void InputHandler::HandleMouseClick(HWND hwnd, LPARAM lParam)
	{
		POINT mousePos;
		GetCursorPos(&mousePos);
		ScreenToClient(hwnd, &mousePos);
		m_LastMouseX = static_cast<float>(mousePos.x);
		m_LastMouseY = static_cast<float>(mousePos.y);
	}
}