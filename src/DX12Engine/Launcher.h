#pragma once
#include "Application.h"
#include "Rendering/RenderContext.h"
#include <DirectXMath.h>
#include <chrono>

namespace DX12Engine
{
	class Launcher
	{
	public:
		static void Launch(Application* app, int windowSize[])
		{
			auto renderContext = std::make_shared<RenderContext>(app, windowSize[0], windowSize[1]);
			app->Init(renderContext, { (float)windowSize[0], (float)windowSize[1] });

			auto startTime = std::chrono::high_resolution_clock::now();
			auto lastFrameTime = startTime;

			while (renderContext->ProcessWindowMessages())
			{
				auto currentTime = std::chrono::high_resolution_clock::now();
				std::chrono::duration<float> ts = currentTime - lastFrameTime;
				std::chrono::duration<float> elapsed = currentTime - startTime;

				app->Update(ts.count(), elapsed.count());

				lastFrameTime = currentTime;
			}
		}
	};
}
