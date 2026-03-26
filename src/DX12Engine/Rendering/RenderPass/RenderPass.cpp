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
		for (UINT size : m_ResourceBlockSizes)
		{
			AddDescriptorTableConfig({ size, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, baseRegister });
			baseRegister += size;
		}

		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_ScreenDataCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(ScreenData));
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2(windowSize.x, windowSize.y);
	}

	void RenderPass::RebuildTransientDescriptors()
	{
		if (m_InputResources.empty())
			return;

		m_InputResourceBlockHandles.clear();

		// Allocate one contiguous transient block for all input resources and capture
		// the base handle. Block-start GPU handles are computed by offset from this base
		// so that clobbers from other passes' UpdateSRVDescriptors calls cannot corrupt them.
		DescriptorHeapHandle blockBase = ResourceManager::GetInstance().UpdateSRVDescriptors(EngineUtils::VectorSharedPtrToPtrs(m_InputResources));

		UINT descriptorSize = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetDescriptorSize();
		UINT baseRegister = 0;
		for (UINT size : m_ResourceBlockSizes)
		{
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
				m_InputResourceBlockHandles.push_back(blockStart);
			}
			baseRegister += size;
		}
	}

	void RenderPass::Execute()
	{
		RebuildTransientDescriptors();
		UpdateCB();
		m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();
	}

	void RenderPass::OnResize(DirectX::XMINT2 newSize)
	{
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2((float)newSize.x, (float)newSize.y);
		if (m_ScreenDataCB)
			m_ScreenDataCB->Update(&m_ScreenData, sizeof(ScreenData));
	}

	void RenderPass::UpdateCB()
	{
		if (m_Camera != nullptr)
		{
			m_ScreenData.CameraPosition = DirectX::XMFLOAT4(m_Camera->GetPosition().x, m_Camera->GetPosition().y, m_Camera->GetPosition().z, 1.0f);
			m_ScreenData.ViewMatrix = m_Camera->GetViewMatrix();
			m_ScreenData.ProjectionMatrix = m_Camera->GetProjectionMatrix();
			m_ScreenData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, m_Camera->GetViewMatrix());
			m_ScreenData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, m_Camera->GetProjectionMatrix());
			m_ScreenDataCB->Update(&m_ScreenData, sizeof(ScreenData));
		}
	}
}
