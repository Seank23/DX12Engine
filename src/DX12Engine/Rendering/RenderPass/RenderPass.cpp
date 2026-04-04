#pragma once
#include "RenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../../Input/Camera.h"
#include "../Buffers/ConstantBuffer.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	void RenderPass::Init()
	{
		m_InputResourceBlockHandles.clear();
		m_DescriptorTableConfigs.clear();

		// Build descriptor table configs for PSO root signature construction only.
		// No transient allocation here -- RebuildTransientDescriptors allocates fresh
		// shader-visible slots every frame from the current frame's transient region.
		UINT baseRegister = 0;
		for (InputResourceType type : m_ResourceBlockOrder)
		{
			auto blockIt = m_ResourceBlocks.find(type);
			if (blockIt == m_ResourceBlocks.end())
				continue;

			AddDescriptorTableConfig({ blockIt->second, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, baseRegister });
			baseRegister += blockIt->second;
		}
	}

	void RenderPass::RebuildTransientDescriptors()
	{
		if (m_InputResources.empty())
			return;

		m_InputResourceBlockHandles.clear();

		// Allocate one contiguous transient block for all input resources and capture
		// the base handle. Block-start GPU handles are computed by offset from this base
		// so that clobbers from other passes' UpdateSRVDescriptors calls cannot corrupt them.
		std::vector<GPUResource*> inputResources;
		for (const auto& resource : m_InputResources)
			inputResources.push_back(resource.get());
		DescriptorHeapHandle blockBase = ResourceManager::GetInstance().UpdateSRVDescriptors(inputResources);

		UINT descriptorSize = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetDescriptorSize();
		UINT baseRegister = 0;
		for (InputResourceType type : OrderedInputTypes)
		{
			auto sizeIt = m_ResourceBlocks.find(type);
			if (sizeIt == m_ResourceBlocks.end())
				continue;
			UINT size = sizeIt->second;

			if (size > 0 && baseRegister < static_cast<UINT>(m_InputResources.size()))
			{
				DescriptorHeapHandle blockStart;
				D3D12_CPU_DESCRIPTOR_HANDLE cpu = blockBase.GetCPUHandle();
				cpu.ptr += baseRegister * descriptorSize;
				D3D12_GPU_DESCRIPTOR_HANDLE gpu = blockBase.GetGPUHandle();
				gpu.ptr += baseRegister * descriptorSize;
				blockStart.SetCPUHandle(cpu);
				blockStart.SetGPUHandle(gpu);
				blockStart.SetHeapIndex(blockBase.GetHeapIndex() + baseRegister);
				m_InputResourceBlockHandles[type] = blockStart;
			}
			baseRegister += size;
		}
	}

	void RenderPass::Execute()
	{
		RebuildTransientDescriptors();
		m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();
	}
}
