#pragma once
#include <wtypes.h>

namespace DX12Engine
{
	class Application
	{
	public:
		Application() = default;
		~Application() = default;

		virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam) = 0;
	};
}

