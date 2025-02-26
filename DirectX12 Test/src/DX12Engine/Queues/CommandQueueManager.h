#pragma once
#include <d3d12.h>
#include "CommandQueue.h"

namespace DX12Engine
{
	class CommandQueueManager
	{
	public:
		CommandQueueManager(ID3D12Device* device);
		~CommandQueueManager();

		CommandQueue& GetGraphicsQueue() const { return *m_GraphicsQueue; }
		CommandQueue& GetComputeQueue() { return *m_ComputeQueue; }
		CommandQueue& GetCopyQueue() { return *m_CopyQueue; }

		CommandQueue* GetQueue(D3D12_COMMAND_LIST_TYPE commandType);

		bool IsFenceComplete(UINT fenceValue);
		void WaitForFenceCPUBlocking(UINT fenceValue);
		void WaitForAllIdle();

	private:
		std::unique_ptr<CommandQueue> m_GraphicsQueue;
		std::unique_ptr<CommandQueue> m_ComputeQueue;
		std::unique_ptr<CommandQueue> m_CopyQueue;
	};
}

