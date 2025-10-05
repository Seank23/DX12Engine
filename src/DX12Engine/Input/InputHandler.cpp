#include "InputHandler.h"
#include <windowsx.h>
#include <cstdio>
#include "InputController.h"
#include "Camera.h"

namespace DX12Engine
{
	InputHandler::InputHandler()
		: m_Camera(nullptr)
	{
		m_CommandMap = {
			{ InputCommand::MoveForward,  'W'},
			{ InputCommand::MoveBackward, 'S'},
			{ InputCommand::MoveLeft,     'A'},
			{ InputCommand::MoveRight,    'D'},
			{ InputCommand::MoveUp,       'E'},
			{ InputCommand::MoveDown,     'Q'},
			{ InputCommand::Pan,		 VK_RBUTTON },
			{ InputCommand::Interact,	 VK_LBUTTON },
		};
	}

	InputHandler::~InputHandler()
	{
	}

	void InputHandler::ProcessInput(float deltaTime)
	{
		if (!m_Camera) return;
		for (const auto& [command, key] : m_CommandMap)
		{
			if (InputController::IsKeyPressed(key))
			{
				m_Camera->ProcessKeyInput(command, deltaTime);
			}
		}
	}

	void InputHandler::HandleMouseMovement(HWND hwnd, LPARAM lParam)
	{
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

		if (!m_Camera) return;
		for (const auto& [command, key] : m_CommandMap)
		{
			if (InputController::IsKeyPressed(key))
			{
				m_Camera->ProcessMouseInput(command, deltaX, deltaY);
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