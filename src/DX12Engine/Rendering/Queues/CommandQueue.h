#pragma once
#include "d3dx12.h"
#include <mutex>
#include <vector>

namespace DX12Engine
{
	class CommandQueue
	{
		struct CommandAllocatorSlot
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Allocator;
			UINT64 FenceValue = 0;
		};

	public:
		CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
		~CommandQueue();

		bool IsFenceComplete(UINT fenceValue);
		void InsertWait(UINT fenceValue);
		void InsertWaitForQueueFence(CommandQueue* otherQueue, UINT fenceValue);
		void InsertWaitForQueue(CommandQueue* otherQueue);

		void WaitForFenceCPUBlocking(UINT64 fenceValue);
		void WaitForIdle() { WaitForFenceCPUBlocking(m_NextFenceValue - 1); }

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return m_CommandQueue; }

		UINT64 PollCurrentFenceValue();
		UINT64 GetLastCompletedFence() { return m_LastCompletedFenceValue; }
		UINT64 GetNextFenceValue() { return m_NextFenceValue; }
		Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return m_Fence; }

		UINT64 ExecuteCommandList();
		UINT64 SubmitCommandList(ID3D12GraphicsCommandList* commandList);

		ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList.Get(); }
		void ResetCommandAllocatorAndList();
		void ResetCommandList();

	private:
		void FlushQueue();
		bool IsAllocatorSlotReady(const CommandAllocatorSlot& slot) const;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		std::vector<CommandAllocatorSlot> m_CommandAllocatorSlots;
		size_t m_CurrentAllocatorSlot = 0;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
		UINT64 m_NextFenceValue;
		UINT64 m_LastCompletedFenceValue;
		HANDLE m_FenceEvent;
		D3D12_COMMAND_LIST_TYPE m_QueueType;

		std::mutex m_FenceMutex;
		std::mutex m_EventMutex;

		bool m_ListIsRecording = false;
	};
}
