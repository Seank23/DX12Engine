#pragma once
#include "StagingDescriptorHeap.h"
#include "RenderPassDescriptorHeap.h"

namespace DX12Engine
{
	// Capacity constants � increase these when adding more assets.
	static constexpr UINT SRV_PERSISTENT_CAPACITY = 1024;
	static constexpr UINT SRV_TRANSIENT_CAPACITY_FRAME = 512;
	static constexpr UINT DSV_CAPACITY = 128;
	static constexpr UINT RTV_CAPACITY = 128;

	// Warn at these fill fractions (checked via GetStats in debug builds).
	static constexpr float HEAP_WARN_THRESHOLD_HIGH = 0.95f;
	static constexpr float HEAP_WARN_THRESHOLD_MED = 0.80f;

	class DescriptorHeapManager
	{
	public:
		DescriptorHeapManager(Microsoft::WRL::ComPtr<ID3D12Device> device);
		~DescriptorHeapManager();

		// Persistent allocation: one handle per resource, stable for its lifetime.
		// Allocates from the non-shader-visible staging heap so the CPU handle can be
		// used as a copy source into the shader-visible transient region each frame.
		DescriptorHeapHandle AllocatePersistentSRV() { return m_StagingHeap->AllocatePersistentHandle(); }
		DescriptorHeapHandle AllocatePersistentDSV() { return m_DepthStencilHeap->AllocatePersistentHandle(); }
		DescriptorHeapHandle AllocatePersistentRTV() { return m_RenderTargetHeap->AllocatePersistentHandle(); }
		void ReleasePersistentSRV(const DescriptorHeapHandle& handle) { m_StagingHeap->ReleasePersistentHandle(handle); }
		void ReleasePersistentDSV(const DescriptorHeapHandle& handle) { m_DepthStencilHeap->ReleasePersistentHandle(handle); }
		void ReleasePersistentRTV(const DescriptorHeapHandle& handle) { m_RenderTargetHeap->ReleasePersistentHandle(handle); }

		// Transient allocation: valid only within the current frame.
		DescriptorHeapHandle AllocateTransientSRVBlock(UINT count) { return m_RenderPassHeap->AllocateHandleBlock(count); }

		// Must be called once per frame before any transient allocations.
		void BeginFrame(UINT frameIndex) { m_RenderPassHeap->BeginFrame(frameIndex); }

		StagingDescriptorHeap& GetStagingHeap() { return *m_StagingHeap; }
		RenderPassDescriptorHeap& GetRenderPassHeap() { return *m_RenderPassHeap; }
		StagingDescriptorHeap& GetDepthStencilHeap() { return *m_DepthStencilHeap; }
		StagingDescriptorHeap& GetRenderTargetHeap() { return *m_RenderTargetHeap; }

		DescriptorHeapStats GetStats() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		std::unique_ptr<StagingDescriptorHeap> m_StagingHeap;
		std::unique_ptr<RenderPassDescriptorHeap> m_RenderPassHeap;
		std::unique_ptr<StagingDescriptorHeap> m_DepthStencilHeap;
		std::unique_ptr<StagingDescriptorHeap> m_RenderTargetHeap;
	};
}