#define NOMINMAX
#include "CommandQueue.h"
#include "EngineUtils.h"
#include <math.h>

namespace DX12Engine
{
	CommandQueue::CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)
		: m_QueueType(commandType), m_CommandQueue(nullptr), m_Fence(nullptr)
	{
		m_NextFenceValue = ((uint64_t)m_QueueType << 56) + 1;
		m_LastCompletedFenceValue = ((uint64_t)m_QueueType << 56);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = m_QueueType;
		queueDesc.NodeMask = 0;
		EngineUtils::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

		EngineUtils::ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
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

	UINT CommandQueue::ExecuteCommandList(ID3D12CommandList* commandList)
	{
		EngineUtils::ThrowIfFailed(((ID3D12GraphicsCommandList*)commandList)->Close());
		m_CommandQueue->ExecuteCommandLists(1, &commandList);

		std::lock_guard<std::mutex> lockGuard(m_EventMutex);
		m_CommandQueue->Signal(m_Fence.Get(), m_NextFenceValue);
		return m_NextFenceValue++;
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
