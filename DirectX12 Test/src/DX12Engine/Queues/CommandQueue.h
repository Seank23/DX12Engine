#pragma once
#include "d3dx12.h"
#include <mutex>
#include "../Rendering/RenderDevice.h"

namespace DX12Engine
{
	class CommandQueue
	{
	public:
        CommandQueue(RenderDevice* device, D3D12_COMMAND_LIST_TYPE commandType);
        ~CommandQueue();

        bool IsFenceComplete(UINT fenceValue);
        void InsertWait(UINT fenceValue);
        void InsertWaitForQueueFence(CommandQueue* otherQueue, UINT fenceValue);
        void InsertWaitForQueue(CommandQueue* otherQueue);

        void WaitForFenceCPUBlocking(UINT fenceValue);
        void WaitForIdle() { WaitForFenceCPUBlocking(m_NextFenceValue - 1); }

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return m_CommandQueue; }

        UINT PollCurrentFenceValue();
        UINT GetLastCompletedFence() { return m_LastCompletedFenceValue; }
        UINT GetNextFenceValue() { return m_NextFenceValue; }
        Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return m_Fence; }

        UINT ExecuteCommandList();
        ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList.Get(); }
        void ResetCommandAllocatorAndList();

    private:
        void FlushQueue();

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
		UINT64 m_NextFenceValue;
		UINT64 m_LastCompletedFenceValue;
		HANDLE m_FenceEvent;
		D3D12_COMMAND_LIST_TYPE m_QueueType;

        std::mutex m_FenceMutex;
        std::mutex m_EventMutex;

        RenderDevice* m_RenderDevice;
	};
}

