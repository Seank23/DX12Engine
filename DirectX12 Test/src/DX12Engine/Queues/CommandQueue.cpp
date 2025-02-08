#define NOMINMAX
#include "CommandQueue.h"
#include "../Utils/EngineUtils.h"
#include <math.h>

namespace DX12Engine
{
	CommandQueue::CommandQueue(RenderDevice* device, D3D12_COMMAND_LIST_TYPE commandType)
		: m_QueueType(commandType), m_CommandQueue(nullptr), m_Fence(nullptr)
	{
		m_RenderDevice = device;
		m_NextFenceValue = ((uint64_t)m_QueueType << 56) + 1;
		m_LastCompletedFenceValue = ((uint64_t)m_QueueType << 56);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = m_QueueType;
		queueDesc.NodeMask = 0;
		EngineUtils::ThrowIfFailed(device->GetDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

		EngineUtils::ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(commandType, IID_PPV_ARGS(&m_CommandAllocator)));
		EngineUtils::ThrowIfFailed(device->GetDevice()->CreateCommandList(0, commandType, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
		m_CommandList->Close();
		ResetCommandAllocatorAndList();
		
		EngineUtils::ThrowIfFailed(device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
		m_Fence->Signal(m_LastCompletedFenceValue);

		m_FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		EngineUtils::Assert(m_FenceEvent != INVALID_HANDLE_VALUE);
	}

	CommandQueue::~CommandQueue()
	{
		FlushQueue();
		CloseHandle(m_FenceEvent);
		m_Fence.Reset();
		m_CommandQueue.Reset();
		m_CommandList.Reset();
	}

	bool CommandQueue::IsFenceComplete(UINT fenceValue)
	{
		if (fenceValue > m_LastCompletedFenceValue)
			PollCurrentFenceValue();
		return fenceValue <= m_LastCompletedFenceValue;
	}

	void CommandQueue::InsertWait(UINT fenceValue)
	{
		m_CommandQueue->Wait(m_Fence.Get(), fenceValue);
	}

	void CommandQueue::InsertWaitForQueueFence(CommandQueue* otherQueue, UINT fenceValue)
	{
		m_CommandQueue->Wait(otherQueue->GetFence().Get(), fenceValue);
	}

	void CommandQueue::InsertWaitForQueue(CommandQueue* otherQueue)
	{
		m_CommandQueue->Wait(otherQueue->GetFence().Get(), otherQueue->GetNextFenceValue() - 1);
	}

	void CommandQueue::WaitForFenceCPUBlocking(UINT fenceValue)
	{
		if (!IsFenceComplete(fenceValue))
		{
			std::lock_guard<std::mutex> lockGuard(m_EventMutex);
			EngineUtils::ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
			WaitForSingleObjectEx(m_FenceEvent, INFINITE, false);
			m_LastCompletedFenceValue = fenceValue;
		}
	}

	UINT CommandQueue::PollCurrentFenceValue()
	{
		m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_Fence->GetCompletedValue());
		return m_LastCompletedFenceValue;
	}

	UINT CommandQueue::ExecuteCommandList()
	{
		EngineUtils::ThrowIfFailed(m_CommandList->Close());
		auto commandList = (ID3D12CommandList*)m_CommandList.Get();
		m_CommandQueue->ExecuteCommandLists(1, &commandList);

		std::lock_guard<std::mutex> lockGuard(m_EventMutex);
		m_CommandQueue->Signal(m_Fence.Get(), m_NextFenceValue);
		return m_NextFenceValue++;
	}

	void CommandQueue::ResetCommandAllocatorAndList()
	{
		m_CommandAllocator->Reset();
		m_CommandList->Reset(m_CommandAllocator.Get(), m_RenderDevice->GetPipelineState().Get());
	}

	void CommandQueue::FlushQueue()
	{
		const UINT64 fenceSignalValue = ++m_NextFenceValue;
		EngineUtils::ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), fenceSignalValue));

		if (m_Fence->GetCompletedValue() < fenceSignalValue) {
			EngineUtils::ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceSignalValue, m_FenceEvent));
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}
}
