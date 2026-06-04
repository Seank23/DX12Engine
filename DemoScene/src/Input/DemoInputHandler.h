#pragma once

#include "DX12Engine/Input/InputHandler.h"
#include "DX12Engine/UI/UISystem.h"

namespace DX12EngineDemo
{
	class DemoInputHandler : public DX12Engine::InputHandler
	{
	public:
		DemoInputHandler(std::shared_ptr<DX12Engine::UISystem> uiSystem);
		~DemoInputHandler() = default;

		void HandleMouseWheel(HWND hwnd, WPARAM wParam) override;
	};
}
