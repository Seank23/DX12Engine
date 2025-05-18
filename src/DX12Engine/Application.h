#pragma once
#include <wtypes.h>
#include <DirectXMath.h>
#include <memory>

namespace DX12Engine
{
	class RenderContext;

	class Application
	{
	public:
		Application() = default;
		~Application() = default;

		virtual void Init(std::shared_ptr<RenderContext> renderContext, DirectX::XMFLOAT2 windowSize) = 0;
		virtual void Update(float ts, float elapsed) = 0;

		virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam) = 0;

	protected:
		std::shared_ptr<RenderContext> m_RenderContext;
	};
}

