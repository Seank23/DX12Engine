#include "RenderContext.h"
#include "ResourceManager.h"

namespace DX12Engine
{
	RenderContext::RenderContext(int width, int height)
		: m_WindowSize(DirectX::XMFLOAT2(width, height))
	{
		m_RenderWindow = std::make_unique<RenderWindow>();
		HWND windowHandle = m_RenderWindow->Init(m_WindowSize);

		m_RenderDevice = std::make_unique<RenderDevice>();
		m_RenderDevice->Init(windowHandle);

		m_QueueManager = std::make_unique<CommandQueueManager>(m_RenderDevice->GetDevice().Get());

		m_RenderWindow->CreateSwapChain(m_QueueManager->GetGraphicsQueue()->GetCommandQueue().Get());
		m_RenderWindow->CreateRTVHeap(m_RenderDevice->GetDevice().Get());
		m_RenderWindow->CreateDepthStencilBuffer(m_RenderDevice->GetDevice().Get());

		ResourceManager::GetInstance().Init(m_RenderDevice->GetDevice());
	}

	RenderContext::~RenderContext()
	{
		ResourceManager::Shutdown();
		m_QueueManager.reset();
		m_RenderDevice.reset();
		m_RenderWindow.reset();
	}

	void RenderContext::CreatePipeline(Shader* vertexShader, Shader* pixelShader)
	{
		m_RenderDevice->CreatePipelineState(vertexShader, pixelShader);
	}
}
