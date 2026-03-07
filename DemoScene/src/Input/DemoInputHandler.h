#pragma once

#include "DX12Engine/Input/InputHandler.h"

namespace DX12EngineDemo
{
	class DemoInputHandler : public DX12Engine::InputHandler
	{
	public:
		DemoInputHandler() = default;
		~DemoInputHandler() = default;

		void HandleMouseWheel(HWND hwnd, WPARAM wParam) override;
	};
}
