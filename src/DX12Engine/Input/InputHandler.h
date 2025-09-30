#pragma once
#include <unordered_map>
#include <wtypes.h>

namespace DX12Engine
{
	class Camera;
	enum class InputCommand;

	class InputHandler
	{
	public:
		InputHandler();
		~InputHandler();

		void SetCamera(Camera* camera) { m_Camera = camera; }

		virtual void ProcessInput(float deltaTime);
		virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam);
		virtual void HandleMouseClick(HWND hwnd, LPARAM lParam);

	protected:
		Camera* m_Camera;

		float m_LastMouseX = 0.0f;
		float m_LastMouseY = 0.0f;
		bool m_FirstMouse = true;

		std::unordered_map<InputCommand, int> m_CommandMap;
	};
}