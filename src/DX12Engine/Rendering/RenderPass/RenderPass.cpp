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
		if (m_InputResources.size() > 0)
		{
			ResourceManager::GetInstance().UpdateSRVDescriptors(EngineUtils::VectorSharedPtrToPtrs(m_InputResources));
			UINT baseRegister = 0;
			for (UINT size : m_ResourceBlockSizes)
			{
				AddDescriptorTableConfig({ size, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, baseRegister });
				if (size > 0 && baseRegister < m_InputResources.size())
				{
					DescriptorHeapHandle* descriptor = m_InputResources[baseRegister]->GetDescriptor();
					if (descriptor)
						m_InputResourceBlockHandles.push_back(*descriptor);
				}
				baseRegister += size;
			}
		}

		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_ScreenDataCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(ScreenData));
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2(windowSize.x, windowSize.y);
	}

	void RenderPass::Execute()
	{
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
