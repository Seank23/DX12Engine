#include "CommandQueueManager.h"

namespace DX12Engine
{
	CommandQueueManager::CommandQueueManager(RenderDevice* device)
	{
		m_GraphicsQueue = std::make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_ComputeQueue = std::make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		m_CopyQueue = std::make_unique<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_COPY);
	}

	CommandQueueManager::~CommandQueueManager()
	{
	}

	CommandQueue* CommandQueueManager::GetQueue(D3D12_COMMAND_LIST_TYPE commandType)
	{
		switch (commandType)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return m_GraphicsQueue.get();
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return m_ComputeQueue.get();
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return m_CopyQueue.get();
		default:
			std::runtime_error("Invalid command type");
		}
		return nullptr;
	}

	bool CommandQueueManager::IsFenceComplete(UINT fenceValue)
	{
		return GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56))->IsFenceComplete(fenceValue);
	}

	void CommandQueueManager::WaitForFenceCPUBlocking(UINT fenceValue)
	{
		CommandQueue* queue = GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
		queue->WaitForFenceCPUBlocking(fenceValue);
	}

	void CommandQueueManager::WaitForAllIdle()
	{
		m_GraphicsQueue->WaitForIdle();
		m_ComputeQueue->WaitForIdle();
		m_CopyQueue->WaitForIdle();
	}
}
