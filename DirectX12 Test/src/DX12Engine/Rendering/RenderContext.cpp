#include "RenderContext.h"
#include "../Resources/ResourceManager.h"
#include "./PipelineStateCache.h"
#include "./RootSignatureCache.h"

namespace DX12Engine
{
	RenderContext::RenderContext(int width, int height)
		: m_WindowSize(DirectX::XMFLOAT2(width, height))
	{
		m_RenderWindow = std::make_unique<RenderWindow>();
		HWND windowHandle = m_RenderWindow->Init(m_WindowSize);

		m_RenderDevice = std::make_unique<RenderDevice>();
		m_RenderDevice->Init(windowHandle);

		m_QueueManager = std::make_unique<CommandQueueManager>(m_RenderDevice.get());
		m_HeapManager = std::make_unique<DescriptorHeapManager>(m_RenderDevice->GetDevice());
		m_Uploader = std::make_unique<GPUUploader>(*this);

		m_RenderWindow->CreateSwapChain(m_QueueManager->GetGraphicsQueue().GetCommandQueue().Get());
		m_RenderWindow->CreateRTVHeap(m_RenderDevice->GetDevice().Get());
		m_RenderWindow->CreateDepthStencilBuffer(m_RenderDevice->GetDevice().Get());

		ResourceManager::GetInstance().Init(*this);
	}

	RenderContext::~RenderContext()
	{
		ResourceManager::Shutdown();
		m_QueueManager.reset();
		m_RenderDevice.reset();
		m_RenderWindow.reset();
		m_Uploader.reset();
	}
}
