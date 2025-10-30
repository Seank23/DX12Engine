#pragma once
#include <Windows.h>
#include <string>

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

		virtual void Update(float deltaTime) = 0;
		virtual void ProcessKeyInput(InputCommand command, float deltaTime) = 0;
		virtual void ProcessMouseInput(InputCommand command, float dX, float dY) = 0;
	};
}

