#include "Input/DemoInputHandler.h"

namespace DX12EngineDemo
{
	DemoInputHandler::DemoInputHandler(std::shared_ptr<DX12Engine::UISystem> uiSystem)
		: InputHandler(uiSystem)
	{
	}

	void DemoInputHandler::HandleMouseWheel(HWND hwnd, WPARAM wParam)
	{
		(void)hwnd;
		(void)wParam;
	}
}
