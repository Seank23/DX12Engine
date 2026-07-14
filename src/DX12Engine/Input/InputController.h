#pragma once
#include <Windows.h>
#include <string>
#include <functional>

namespace DX12Engine
{
	enum class InputCommand
	{
		MoveForward,
		MoveBackward,
		MoveLeft,
		MoveRight,
		MoveUp,
		MoveDown,
		Pan,
		Interact,
		Scroll,
		Custom
	};

	class InputController
	{
	public:
		InputController() = default;
		~InputController() = default;

		static bool IsKeyPressed(int key)
		{
			return GetAsyncKeyState(key) & 0x8000;
		}

		static void IsNewKeyPress(UINT msg, WPARAM wParam, LPARAM lParam, std::function<void(WPARAM)> onNewKeyPress)
		{
			if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
			{
				const bool wasPreviouslyPressed = (lParam & 0x40000000) != 0;
				const bool isNowPressed = (lParam & 0x80000000) == 0;

				if (isNowPressed && !wasPreviouslyPressed)
					onNewKeyPress(wParam);
			}
		}

		bool IsKeyPressedThisFrame(int key)
		{
			bool isPressed = IsKeyPressed(key);
			bool wasPressed = m_PreviousKeyStates[key];
			m_PreviousKeyStates[key] = isPressed;
			return isPressed && !wasPressed;
		}

		virtual void Update(float deltaTime) = 0;
		virtual void ProcessKeyInput(InputCommand command, float deltaTime) = 0;
		virtual void ProcessMouseInput(InputCommand command, float dX, float dY) = 0;

	private:
		std::unordered_map<int, bool> m_PreviousKeyStates;
	};
}
