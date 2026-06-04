#pragma once
#include <unordered_map>
#include <wtypes.h>
#include <memory>

namespace DX12Engine
{
	class Camera;
	class UISystem;
	enum class InputCommand;

	class InputHandler
	{
	public:
		InputHandler(std::shared_ptr<UISystem> uiSystem);
		~InputHandler();

		void SetCamera(Camera* camera) { m_Camera = camera; }

		virtual void ProcessInput(float deltaTime);
		virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam);
		virtual void HandleMouseClick(HWND hwnd, LPARAM lParam);
		virtual void HandleMouseWheel(HWND hwnd, WPARAM wParam) = 0;

	protected:
		Camera* m_Camera;
		std::shared_ptr<UISystem> m_UISystem;


		float m_LastMouseX = 0.0f;
		float m_LastMouseY = 0.0f;
		bool m_FirstMouse = true;

		std::unordered_map<InputCommand, int> m_CommandMap;
	};
}