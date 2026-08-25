#define NOMINMAX
#include "CommandQueue.h"
#include "../../Utils/EngineUtils.h"
#include "../../Utils/Constants.h"

#include <math.h>
#include <iostream>

namespace DX12Engine
{
	CommandQueue::CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)
		: m_QueueType(commandType), m_CommandQueue(nullptr), m_Fence(nullptr)
	{
		m_NextFenceValue = 1;
		m_LastCompletedFenceValue = 0;

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = m_QueueType;
		queueDesc.NodeMask = 0;
		EngineUtils::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

		m_CommandAllocatorSlots.resize(COMMAND_ALLOCATOR_POOL_SIZE);
		for (CommandAllocatorSlot& slot : m_CommandAllocatorSlots)
			EngineUtils::ThrowIfFailed(device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&slot.Allocator)));

		m_CurrentAllocatorSlot = 0;
		m_CommandAllocator = m_CommandAllocatorSlots[m_CurrentAllocatorSlot].Allocator;
		EngineUtils::ThrowIfFailed(device->CreateCommandList(0, commandType, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
		m_CommandList->Close();

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

	void CommandQueue::WaitForFenceCPUBlocking(UINT64 fenceValue)
	{
		if (!IsFenceComplete(fenceValue))
		{
			std::lock_guard<std::mutex> lockGuard(m_EventMutex);
			EngineUtils::ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
			WaitForSingleObjectEx(m_FenceEvent, INFINITE, false);
			m_LastCompletedFenceValue = fenceValue;
		}
	}

	UINT64 CommandQueue::PollCurrentFenceValue()
	{
		UINT64 value = m_Fence->GetCompletedValue();
		m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, value);
		return m_LastCompletedFenceValue;
	}

	UINT64 CommandQueue::ExecuteCommandList()
	{
		EngineUtils::ThrowIfFailed(m_CommandList->Close());
		auto commandList = (ID3D12CommandList*)m_CommandList.Get();
		m_CommandQueue->ExecuteCommandLists(1, &commandList);

		const UINT64 fenceValue = m_NextFenceValue;
		m_CommandAllocatorSlots[m_CurrentAllocatorSlot].FenceValue = fenceValue;

		std::lock_guard<std::mutex> lockGuard(m_EventMutex);
		m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
		m_NextFenceValue++;
		m_ListIsRecording = false;
		return static_cast<UINT>(fenceValue);
	}

	UINT64 CommandQueue::SubmitCommandList(ID3D12GraphicsCommandList* commandList)
	{
		EngineUtils::ThrowIfFailed(commandList->Close());
		auto list = (ID3D12CommandList*)commandList;
		m_CommandQueue->ExecuteCommandLists(1, &list);

		std::lock_guard<std::mutex> lockGuard(m_EventMutex);
		UINT64 fenceValue = m_NextFenceValue++;
		m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
		return static_cast<UINT>(fenceValue);
	}

	void CommandQueue::ResetCommandAllocatorAndList()
	{
		if (m_CommandAllocatorSlots.empty())
			return;

		EngineUtils::Assert(!m_ListIsRecording);
		m_ListIsRecording = true;

		size_t selectedSlot = m_CurrentAllocatorSlot;
		bool foundReadySlot = false;
		for (size_t i = 1; i <= m_CommandAllocatorSlots.size(); i++)
		{
			size_t candidate = (m_CurrentAllocatorSlot + i) % m_CommandAllocatorSlots.size();
			if (IsAllocatorSlotReady(m_CommandAllocatorSlots[candidate]))
			{
				selectedSlot = candidate;
				foundReadySlot = true;
				break;
			}
		}

		if (!foundReadySlot)
		{
			selectedSlot = (m_CurrentAllocatorSlot + 1) % m_CommandAllocatorSlots.size();
			if (m_CommandAllocatorSlots[selectedSlot].FenceValue > 0)
			{
				WaitForFenceCPUBlocking(static_cast<UINT>(m_CommandAllocatorSlots[selectedSlot].FenceValue));
				m_CommandAllocatorSlots[selectedSlot].FenceValue = 0;
			}
		}

		m_CurrentAllocatorSlot = selectedSlot;
		m_CommandAllocator = m_CommandAllocatorSlots[m_CurrentAllocatorSlot].Allocator;
		m_CommandAllocator->Reset();
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
	}

	void CommandQueue::ResetCommandList()
	{
		m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
	}

	bool CommandQueue::IsAllocatorSlotReady(const CommandAllocatorSlot& slot) const
	{
		return slot.FenceValue == 0 || slot.FenceValue <= m_Fence->GetCompletedValue();
	}

	void CommandQueue::FlushQueue()
	{
		const UINT64 fenceSignalValue = ++m_NextFenceValue;
		EngineUtils::ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), fenceSignalValue));

		if (m_Fence->GetCompletedValue() < fenceSignalValue)
		{
			EngineUtils::ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceSignalValue, m_FenceEvent));
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}
}
