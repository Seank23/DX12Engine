#include "RmlRenderInterfaceDX12.h"

#include "../../Rendering/Heaps/DescriptorHeapManager.h"
#include "../../Rendering/RenderContext.h"
#include "../../Resources/ResourceManager.h"
#include "../../Resources/Shader.h"
#include "../../UI/UIContext.h"
#include "../../Utils/EngineUtils.h"
#include "d3dx12.h"

#include <DirectXTex.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace DX12Engine
{
	namespace
	{
		size_t AlignTo(size_t value, size_t alignment)
		{
			return (value + (alignment - 1)) & ~(alignment - 1);
		}

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
			return value;
		}

		constexpr size_t kDefaultTransientVertexBufferSize = 1024 * 1024;
		constexpr size_t kDefaultTransientIndexBufferSize = 512 * 1024;
		constexpr UINT kConstantBufferSize = 256;
		constexpr UINT kConstantBufferDrawCapacity = 2048;
	}

	RmlRenderInterfaceDX12::RmlRenderInterfaceDX12()
	{
	}

	RmlRenderInterfaceDX12::~RmlRenderInterfaceDX12()
	{
		Shutdown();
	}

	bool RmlRenderInterfaceDX12::Initialize(RenderContext* renderContext, std::filesystem::path uiAssetRoot)
	{
		if (m_IsInitialized)
			return true;

		if (renderContext == nullptr)
			return false;

		m_RenderContext = renderContext;
		m_Device = renderContext->GetDevice().Get();
		m_UIAssetRoot = std::move(uiAssetRoot);
		m_ScissorEnabled = false;
		m_HasTransform = false;
		m_CurrentTransform = Rml::Matrix4f::Identity();

		if (!EnsureConstantBuffer())
			return false;

		if (!EnsureTransientBuffers(kDefaultTransientVertexBufferSize, kDefaultTransientIndexBufferSize))
			return false;

		m_IsInitialized = true;
		return true;
	}

	void RmlRenderInterfaceDX12::Shutdown()
	{
		if (m_RenderContext)
		{
			// Ensure pending upload buffers can be safely released during shutdown.
			m_RenderContext->GetQueueManager().GetGraphicsQueue().WaitForIdle();
		}

		ProcessPendingUploadReleases();

		if (m_RenderContext)
		{
			for (auto& [handle, record] : m_TextureMap)
			{
				if (record.PersistentSrv.IsValid())
					m_RenderContext->GetHeapManager().ReleasePersistentSRV(record.PersistentSrv);
			}
		}

		m_GeometryMap.clear();
		m_TextureMap.clear();
		m_PendingUploadReleases.clear();
		m_WhiteTextureHandle = 0;
		m_NextGeometryHandle = 1;
		m_NextTextureHandle = 1;

		if (m_ConstantBuffer && m_MappedConstantBuffer)
		{
			m_ConstantBuffer->Unmap(0, nullptr);
			m_MappedConstantBuffer = nullptr;
		}

		if (m_TransientVertexBuffer && m_MappedVertexBuffer)
		{
			m_TransientVertexBuffer->Unmap(0, nullptr);
			m_MappedVertexBuffer = nullptr;
		}

		if (m_TransientIndexBuffer && m_MappedIndexBuffer)
		{
			m_TransientIndexBuffer->Unmap(0, nullptr);
			m_MappedIndexBuffer = nullptr;
		}

		m_ConstantBuffer.Reset();
		m_TransientVertexBuffer.Reset();
		m_TransientIndexBuffer.Reset();
		m_PipelineState.Reset();
		m_RootSignature.Reset();

		m_CommandList = nullptr;
		m_Device = nullptr;
		m_RenderContext = nullptr;
		m_LogicalWidth = 0.0f;
		m_LogicalHeight = 0.0f;
		m_TransientVertexBufferSize = 0;
		m_TransientIndexBufferSize = 0;
		m_TransientVertexOffset = 0;
		m_TransientIndexOffset = 0;
		m_ConstantBufferSize = 0;
		m_ConstantBufferOffset = 0;
		m_PsoRenderTargetFormat = DXGI_FORMAT_UNKNOWN;
		m_IsInitialized = false;
	}

	void RmlRenderInterfaceDX12::BeginFrame(const UIRenderContext& context)
	{
		if (!m_IsInitialized)
			return;

		ProcessPendingUploadReleases();

		m_Device = context.Device;
		m_CommandList = context.CommandList;
		m_CurrentViewport = context.Viewport;
		m_DefaultScissor = context.ScissorRect;
		m_CurrentRTV = context.RenderTargetView;
		m_CurrentRenderTargetFormat = context.RenderTargetFormat;
		m_LogicalWidth = context.LogicalWidth > 0 ? static_cast<float>(context.LogicalWidth) : m_CurrentViewport.Width;
		m_LogicalHeight = context.LogicalHeight > 0 ? static_cast<float>(context.LogicalHeight) : m_CurrentViewport.Height;
		ResetTransientBufferOffsets();
		m_ConstantBufferOffset = 0;
		m_FrameTextureTableCache.clear();

		const bool pipelineReady = EnsurePipelineResources();
		if (!pipelineReady || m_CommandList == nullptr)
			return;

		ID3D12DescriptorHeap* srvHeap = m_RenderContext->GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList->SetPipelineState(m_PipelineState.Get());
		m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
		m_CommandList->SetDescriptorHeaps(1, &srvHeap);
		m_CommandList->OMSetRenderTargets(1, &m_CurrentRTV, FALSE, nullptr);
		m_CommandList->RSSetViewports(1, &m_CurrentViewport);
		m_CommandList->RSSetScissorRects(1, &m_DefaultScissor);
		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void RmlRenderInterfaceDX12::EndFrame()
	{
		for (auto& [handle, record] : m_TextureMap)
		{
			if (record.Ready && record.UploadResource)
				QueueUploadResourceRelease(record.UploadResource);
		}

		ProcessPendingUploadReleases();

		m_CommandList = nullptr;
	}

	void RmlRenderInterfaceDX12::QueueUploadResourceRelease(Microsoft::WRL::ComPtr<ID3D12Resource>& uploadResource)
	{
		if (!uploadResource)
			return;

		PendingUploadRelease pending;
		pending.Resource = std::move(uploadResource);

		if (m_RenderContext)
			pending.FenceValue = static_cast<uint64_t>(m_RenderContext->GetQueueManager().GetGraphicsQueue().GetNextFenceValue());

		m_PendingUploadReleases.emplace_back(std::move(pending));
	}

	void RmlRenderInterfaceDX12::ProcessPendingUploadReleases()
	{
		if (m_PendingUploadReleases.empty())
			return;

		uint64_t completedFence = (std::numeric_limits<uint64_t>::max)();
		if (m_RenderContext)
			completedFence = static_cast<uint64_t>(m_RenderContext->GetQueueManager().GetGraphicsQueue().PollCurrentFenceValue());

		auto releaseIt = std::remove_if(m_PendingUploadReleases.begin(), m_PendingUploadReleases.end(),
			[completedFence](const PendingUploadRelease& pending)
			{
				if (!pending.Resource)
					return true;
				if (pending.FenceValue == 0)
					return true;
				return pending.FenceValue <= completedFence;
			});

		m_PendingUploadReleases.erase(releaseIt, m_PendingUploadReleases.end());
	}

	Rml::CompiledGeometryHandle RmlRenderInterfaceDX12::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
	{
		if (vertices.empty() || indices.empty())
			return 0;

		CompiledGeometryRecord record;
		record.Vertices.assign(vertices.begin(), vertices.end());
		record.Indices.reserve(indices.size());
		for (const int index : indices)
			record.Indices.push_back(static_cast<uint32_t>(index));

		const Rml::CompiledGeometryHandle handle = m_NextGeometryHandle++;
		m_GeometryMap.emplace(handle, std::move(record));
		return handle;
	}

	void RmlRenderInterfaceDX12::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
	{
		auto geometryIt = m_GeometryMap.find(geometry);
		if (geometryIt == m_GeometryMap.end())
			return;

		CompiledGeometryRecord& record = geometryIt->second;
		if (texture)
			record.CachedTexture = texture;

		Rml::TextureHandle textureToDraw = texture ? texture : record.CachedTexture;
		if (!textureToDraw)
			textureToDraw = EnsureWhiteTexture();

		DrawGeometry(record, textureToDraw, translation);
	}

	void RmlRenderInterfaceDX12::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
	{
		m_GeometryMap.erase(geometry);
	}

	void RmlRenderInterfaceDX12::RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation)
	{
		auto geometryIt = m_GeometryMap.find(geometry);
		if (geometryIt == m_GeometryMap.end())
			return;

		Rml::TextureHandle textureToDraw = geometryIt->second.CachedTexture;
		if (!textureToDraw)
			textureToDraw = EnsureWhiteTexture();

		DrawGeometry(geometryIt->second, textureToDraw, translation);
	}

	void RmlRenderInterfaceDX12::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry)
	{
		ReleaseGeometry(geometry);
	}

	Rml::TextureHandle RmlRenderInterfaceDX12::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
	{
		texture_dimensions = { 0, 0 };

		const std::filesystem::path filePath = ResolveTexturePath(source);
		if (filePath.empty())
			return 0;

		TextureRecord record;
		if (!CreateTextureFromFile(filePath, record, &texture_dimensions))
			return 0;

		return RegisterTexture(std::move(record));
	}

	Rml::TextureHandle RmlRenderInterfaceDX12::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
	{
		TextureRecord record;
		if (!CreateTextureFromPixels(source, source_dimensions, record))
			return 0;

		return RegisterTexture(std::move(record));
	}

	void RmlRenderInterfaceDX12::ReleaseTexture(Rml::TextureHandle texture_handle)
	{
		if (!texture_handle)
			return;

		auto textureIt = m_TextureMap.find(texture_handle);
		if (textureIt != m_TextureMap.end())
		{
			if (m_RenderContext && textureIt->second.PersistentSrv.IsValid())
				m_RenderContext->GetHeapManager().ReleasePersistentSRV(textureIt->second.PersistentSrv);
			m_TextureMap.erase(textureIt);
		}

		if (m_WhiteTextureHandle == texture_handle)
			m_WhiteTextureHandle = 0;
	}

	void RmlRenderInterfaceDX12::SetTransform(const Rml::Matrix4f* transform)
	{
		if (transform)
		{
			m_CurrentTransform = *transform;
			m_HasTransform = true;
		}
		else
		{
			m_HasTransform = false;
			m_CurrentTransform = Rml::Matrix4f::Identity();
		}
	}

	void RmlRenderInterfaceDX12::SetScissorRegion(Rml::Rectanglei region)
	{
		m_ScissorRegion = region;
	}

	void RmlRenderInterfaceDX12::EnableScissorRegion(bool enable)
	{
		m_ScissorEnabled = enable;
	}

	bool RmlRenderInterfaceDX12::EnsurePipelineResources()
	{
		if (!m_RenderContext)
			return false;

		if (m_CurrentRenderTargetFormat == DXGI_FORMAT_UNKNOWN)
			m_CurrentRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		if (!m_RootSignature)
		{
			D3D12_DESCRIPTOR_RANGE textureRange = {};
			textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			textureRange.NumDescriptors = 1;
			textureRange.BaseShaderRegister = 0;
			textureRange.RegisterSpace = 0;
			textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER rootParameters[2] = {};
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[0].Descriptor.ShaderRegister = 0;
			rootParameters[0].Descriptor.RegisterSpace = 0;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

			rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[1].DescriptorTable.pDescriptorRanges = &textureRange;
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.MipLODBias = 0.0f;
			samplerDesc.MaxAnisotropy = 1;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.MinLOD = 0.0f;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			samplerDesc.ShaderRegister = 0;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
			rootDesc.NumParameters = 2;
			rootDesc.pParameters = rootParameters;
			rootDesc.NumStaticSamplers = 1;
			rootDesc.pStaticSamplers = &samplerDesc;
			rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootDesc);
			if (!m_RootSignature)
				return false;
		}

		if (m_PipelineState && m_PsoRenderTargetFormat == m_CurrentRenderTargetFormat)
			return true;

		Shader* vertexShader = ResourceManager::GetInstance().GetShader("RmlUI_VS");
		Shader* pixelShader = ResourceManager::GetInstance().GetShader("RmlUI_PS");
		if (!vertexShader || !pixelShader)
			return false;

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = { vertexShader->GetShader()->GetBufferPointer(), vertexShader->GetShader()->GetBufferSize() };
		psoDesc.PS = { pixelShader->GetShader()->GetBufferPointer(), pixelShader->GetShader()->GetBufferSize() };
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.InputLayout = { inputLayout, static_cast<UINT>(std::size(inputLayout)) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = m_CurrentRenderTargetFormat;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.SampleDesc.Count = 1;

		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(psoDesc);
		if (!m_PipelineState)
			return false;

		m_PsoRenderTargetFormat = m_CurrentRenderTargetFormat;
		return true;
	}

	bool RmlRenderInterfaceDX12::EnsureConstantBuffer()
	{
		if (m_ConstantBuffer)
			return true;

		if (!m_Device)
			return false;

		auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const size_t totalBufferBytes = static_cast<size_t>(kConstantBufferSize) * static_cast<size_t>(kConstantBufferDrawCapacity);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBufferBytes);
		HRESULT hr = m_Device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_ConstantBuffer));
		if (FAILED(hr))
			return false;

		hr = m_ConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedConstantBuffer));
		if (SUCCEEDED(hr))
		{
			m_ConstantBufferSize = totalBufferBytes;
			m_ConstantBufferOffset = 0;
		}
		return SUCCEEDED(hr);
	}

	bool RmlRenderInterfaceDX12::EnsureTransientBuffers(size_t vertexBytes, size_t indexBytes)
	{
		if (!m_Device)
			return false;

		if (!m_TransientVertexBuffer || m_TransientVertexBufferSize < vertexBytes)
		{
			if (m_TransientVertexBuffer && m_MappedVertexBuffer)
			{
				m_TransientVertexBuffer->Unmap(0, nullptr);
				m_MappedVertexBuffer = nullptr;
			}

			m_TransientVertexBuffer.Reset();
			m_TransientVertexBufferSize = (std::max)(vertexBytes, m_TransientVertexBufferSize ? m_TransientVertexBufferSize * 2 : kDefaultTransientVertexBufferSize);
			auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TransientVertexBufferSize);
			if (FAILED(m_Device->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_TransientVertexBuffer))))
			{
				return false;
			}

			if (FAILED(m_TransientVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedVertexBuffer))))
				return false;
		}

		if (!m_TransientIndexBuffer || m_TransientIndexBufferSize < indexBytes)
		{
			if (m_TransientIndexBuffer && m_MappedIndexBuffer)
			{
				m_TransientIndexBuffer->Unmap(0, nullptr);
				m_MappedIndexBuffer = nullptr;
			}

			m_TransientIndexBuffer.Reset();
			m_TransientIndexBufferSize = (std::max)(indexBytes, m_TransientIndexBufferSize ? m_TransientIndexBufferSize * 2 : kDefaultTransientIndexBufferSize);
			auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_TransientIndexBufferSize);
			if (FAILED(m_Device->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_TransientIndexBuffer))))
			{
				return false;
			}

			if (FAILED(m_TransientIndexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedIndexBuffer))))
				return false;
		}

		return true;
	}

	void RmlRenderInterfaceDX12::ResetTransientBufferOffsets()
	{
		m_TransientVertexOffset = 0;
		m_TransientIndexOffset = 0;
	}

	void RmlRenderInterfaceDX12::UpdateConstants(Rml::Vector2f translation)
	{
		if (!m_MappedConstantBuffer)
			return;

		constexpr size_t kAlignedConstantSize = kConstantBufferSize;
		static_assert(sizeof(Constants) <= kAlignedConstantSize, "UI constants must fit in 256-byte aligned constant buffer slot.");

		if (m_ConstantBufferOffset + kAlignedConstantSize > m_ConstantBufferSize)
			return;

		const float width = (std::max)(1.0f, m_LogicalWidth);
		const float height = (std::max)(1.0f, m_LogicalHeight);
		const Rml::Matrix4f projection = Rml::Matrix4f::ProjectOrtho(0.0f, width, height, 0.0f, -10000.0f, 10000.0f);
		const Rml::Matrix4f combined = m_HasTransform ? (projection * m_CurrentTransform) : projection;

		Constants constants = {};
		std::memcpy(constants.Transform.data(), combined.data(), sizeof(float) * constants.Transform.size());
		constants.Translation[0] = translation.x;
		constants.Translation[1] = translation.y;
		std::memcpy(m_MappedConstantBuffer + m_ConstantBufferOffset, &constants, sizeof(constants));

		if (m_CommandList)
		{
			const D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = m_ConstantBuffer->GetGPUVirtualAddress() + m_ConstantBufferOffset;
			m_CommandList->SetGraphicsRootConstantBufferView(0, cbvAddress);
		}

		m_ConstantBufferOffset += kAlignedConstantSize;
	}

	bool RmlRenderInterfaceDX12::DrawGeometry(const CompiledGeometryRecord& geometry, Rml::TextureHandle texture, Rml::Vector2f translation)
	{
		if (!m_CommandList || !EnsurePipelineResources())
			return false;

		const size_t vertexBytes = geometry.Vertices.size() * sizeof(Rml::Vertex);
		const size_t indexBytes = geometry.Indices.size() * sizeof(uint32_t);
		if (vertexBytes == 0 || indexBytes == 0)
			return false;

		if (!EnsureTransientBuffers(m_TransientVertexOffset + vertexBytes, m_TransientIndexOffset + indexBytes))
			return false;

		std::memcpy(m_MappedVertexBuffer + m_TransientVertexOffset, geometry.Vertices.data(), vertexBytes);
		std::memcpy(m_MappedIndexBuffer + m_TransientIndexOffset, geometry.Indices.data(), indexBytes);

		D3D12_VERTEX_BUFFER_VIEW vertexView = {};
		vertexView.BufferLocation = m_TransientVertexBuffer->GetGPUVirtualAddress() + m_TransientVertexOffset;
		vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
		vertexView.StrideInBytes = sizeof(Rml::Vertex);

		D3D12_INDEX_BUFFER_VIEW indexView = {};
		indexView.BufferLocation = m_TransientIndexBuffer->GetGPUVirtualAddress() + m_TransientIndexOffset;
		indexView.SizeInBytes = static_cast<UINT>(indexBytes);
		indexView.Format = DXGI_FORMAT_R32_UINT;

		const auto textureIt = m_TextureMap.find(texture);
		if (textureIt == m_TextureMap.end())
			return false;

		DescriptorHeapHandle textureTable;
		auto textureTableIt = m_FrameTextureTableCache.find(texture);
		if (textureTableIt != m_FrameTextureTableCache.end())
		{
			textureTable = textureTableIt->second;
		}
		else
		{
			textureTable = BuildTransientTextureDescriptor(textureIt->second);
			if (textureTable.IsReferencedByShader())
				m_FrameTextureTableCache.emplace(texture, textureTable);
		}
		if (!textureTable.IsReferencedByShader())
			return false;

		D3D12_RECT scissorRect = m_DefaultScissor;
		if (m_ScissorEnabled)
		{
			const float scaleX = m_LogicalWidth > 0.0f ? m_CurrentViewport.Width / m_LogicalWidth : 1.0f;
			const float scaleY = m_LogicalHeight > 0.0f ? m_CurrentViewport.Height / m_LogicalHeight : 1.0f;

			const LONG logicalLeft = static_cast<LONG>(std::floor(static_cast<float>(m_ScissorRegion.Left()) * scaleX));
			const LONG logicalTop = static_cast<LONG>(std::floor(static_cast<float>(m_ScissorRegion.Top()) * scaleY));
			const LONG logicalRight = static_cast<LONG>(std::ceil(static_cast<float>(m_ScissorRegion.Right()) * scaleX));
			const LONG logicalBottom = static_cast<LONG>(std::ceil(static_cast<float>(m_ScissorRegion.Bottom()) * scaleY));

			scissorRect.left = (std::max)(m_DefaultScissor.left, logicalLeft);
			scissorRect.top = (std::max)(m_DefaultScissor.top, logicalTop);
			scissorRect.right = (std::min)(m_DefaultScissor.right, logicalRight);
			scissorRect.bottom = (std::min)(m_DefaultScissor.bottom, logicalBottom);
			if (scissorRect.right < scissorRect.left)
				scissorRect.right = scissorRect.left;
			if (scissorRect.bottom < scissorRect.top)
				scissorRect.bottom = scissorRect.top;
		}

		ID3D12DescriptorHeap* srvHeap = m_RenderContext->GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList->SetPipelineState(m_PipelineState.Get());
		m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
		m_CommandList->SetDescriptorHeaps(1, &srvHeap);
		m_CommandList->OMSetRenderTargets(1, &m_CurrentRTV, FALSE, nullptr);
		m_CommandList->RSSetViewports(1, &m_CurrentViewport);
		m_CommandList->RSSetScissorRects(1, &scissorRect);
		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList->IASetVertexBuffers(0, 1, &vertexView);
		m_CommandList->IASetIndexBuffer(&indexView);

		UpdateConstants(translation);
		m_CommandList->SetGraphicsRootDescriptorTable(1, textureTable.GetGPUHandle());
		m_CommandList->DrawIndexedInstanced(static_cast<UINT>(geometry.Indices.size()), 1, 0, 0, 0);

		m_TransientVertexOffset = AlignTo(m_TransientVertexOffset + vertexBytes, 16);
		m_TransientIndexOffset = AlignTo(m_TransientIndexOffset + indexBytes, 4);
		return true;
	}

	Rml::TextureHandle RmlRenderInterfaceDX12::RegisterTexture(TextureRecord&& record)
	{
		const Rml::TextureHandle handle = m_NextTextureHandle++;
		m_TextureMap.emplace(handle, std::move(record));
		return handle;
	}

	Rml::TextureHandle RmlRenderInterfaceDX12::EnsureWhiteTexture()
	{
		if (m_WhiteTextureHandle && m_TextureMap.find(m_WhiteTextureHandle) != m_TextureMap.end())
			return m_WhiteTextureHandle;

		static constexpr std::array<Rml::byte, 4> whitePixel = { 255, 255, 255, 255 };
		TextureRecord record;
		if (!CreateTextureFromPixels({ whitePixel.data(), whitePixel.size() }, Rml::Vector2i(1, 1), record))
			return 0;

		m_WhiteTextureHandle = RegisterTexture(std::move(record));
		return m_WhiteTextureHandle;
	}

	bool RmlRenderInterfaceDX12::CreateTextureFromPixels(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions, TextureRecord& outRecord)
	{
		if (!m_Device || dimensions.x <= 0 || dimensions.y <= 0)
			return false;

		const size_t pixelCount = static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y);
		const size_t expectedRgbaBytes = pixelCount * 4;
		const bool isRgbaSource = source.size() >= expectedRgbaBytes;
		const bool isAlphaSource = source.size() >= pixelCount;
		if (!isRgbaSource && !isAlphaSource)
			return false;

		std::vector<Rml::byte> expandedRgba;
		const Rml::byte* uploadPixels = source.data();
		if (!isRgbaSource)
		{
			// Rml generates A8 glyph data in premultiplied space, so copy alpha into all channels.
			expandedRgba.resize(expectedRgbaBytes);
			for (size_t index = 0; index < pixelCount; ++index)
			{
				const Rml::byte alpha = source[index];
				expandedRgba[index * 4 + 0] = alpha;
				expandedRgba[index * 4 + 1] = alpha;
				expandedRgba[index * 4 + 2] = alpha;
				expandedRgba[index * 4 + 3] = alpha;
			}
			uploadPixels = expandedRgba.data();
		}

		auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT>(dimensions.x), static_cast<UINT>(dimensions.y));
		if (FAILED(m_Device->CreateCommittedResource(
			&heapDefault,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&outRecord.Resource))))
		{
			return false;
		}

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(outRecord.Resource.Get(), 0, 1);
		auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		if (FAILED(m_Device->CreateCommittedResource(
			&heapUpload,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&outRecord.UploadResource))))
		{
			return false;
		}

		D3D12_SUBRESOURCE_DATA subresource = {};
		subresource.pData = uploadPixels;
		subresource.RowPitch = static_cast<LONG_PTR>(dimensions.x) * 4;
		subresource.SlicePitch = subresource.RowPitch * static_cast<LONG_PTR>(dimensions.y);

		if (m_CommandList)
		{
			UpdateSubresources(m_CommandList, outRecord.Resource.Get(), outRecord.UploadResource.Get(), 0, 0, 1, &subresource);
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				outRecord.Resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_CommandList->ResourceBarrier(1, &barrier);
			outRecord.Ready = true;
		}
		else
		{
			auto& queueManager = m_RenderContext->GetQueueManager();
			auto& copyQueue = queueManager.GetCopyQueue();
			auto& graphicsQueue = queueManager.GetGraphicsQueue();
			copyQueue.ResetCommandAllocatorAndList();
			graphicsQueue.ResetCommandAllocatorAndList();

			UpdateSubresources(copyQueue.GetCommandList(), outRecord.Resource.Get(), outRecord.UploadResource.Get(), 0, 0, 1, &subresource);
			const UINT copyFence = copyQueue.ExecuteCommandList();
			graphicsQueue.InsertWaitForQueueFence(&copyQueue, copyFence);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				outRecord.Resource.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			graphicsQueue.GetCommandList()->ResourceBarrier(1, &barrier);
			const UINT graphicsFence = graphicsQueue.ExecuteCommandList();
			graphicsQueue.WaitForFenceCPUBlocking(graphicsFence);
			outRecord.Ready = true;
			outRecord.UploadResource.Reset();
		}

		outRecord.PersistentSrv = m_RenderContext->GetHeapManager().AllocatePersistentSRV();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_Device->CreateShaderResourceView(outRecord.Resource.Get(), &srvDesc, outRecord.PersistentSrv.GetCPUHandle());

		outRecord.Width = static_cast<uint32_t>(dimensions.x);
		outRecord.Height = static_cast<uint32_t>(dimensions.y);
		return true;
	}

	bool RmlRenderInterfaceDX12::CreateTextureFromFile(const std::filesystem::path& filePath, TextureRecord& outRecord, Rml::Vector2i* outDimensions)
	{
		DirectX::ScratchImage image;
		const std::wstring path = filePath.wstring();
		const std::string ext = ToLower(filePath.extension().string());
		HRESULT hr = (ext == ".dds")
			? DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)
			: DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);

		if (FAILED(hr))
			return false;

		const DirectX::Image* srcImage = image.GetImage(0, 0, 0);
		if (!srcImage)
			return false;

		DirectX::ScratchImage converted;
		if (srcImage->format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			if (FAILED(DirectX::Convert(*srcImage, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted)))
				return false;
			srcImage = converted.GetImage(0, 0, 0);
			if (!srcImage)
				return false;
		}

		Rml::Vector2i dimensions(static_cast<int>(srcImage->width), static_cast<int>(srcImage->height));
		std::vector<Rml::byte> packedPixels(static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y) * 4);
		for (int y = 0; y < dimensions.y; y++)
		{
			const uint8_t* sourceRow = srcImage->pixels + static_cast<size_t>(y) * srcImage->rowPitch;
			uint8_t* destinationRow = packedPixels.data() + static_cast<size_t>(y) * static_cast<size_t>(dimensions.x) * 4;
			std::memcpy(destinationRow, sourceRow, static_cast<size_t>(dimensions.x) * 4);
		}

		if (!CreateTextureFromPixels(packedPixels, dimensions, outRecord))
			return false;

		if (outDimensions)
			*outDimensions = dimensions;

		return true;
	}

	std::filesystem::path RmlRenderInterfaceDX12::ResolveTexturePath(const Rml::String& source) const
	{
		if (source.empty())
			return {};

		const std::filesystem::path sourcePath(source);
		if (sourcePath.is_absolute() && std::filesystem::exists(sourcePath))
			return sourcePath;

		const std::filesystem::path cwd = std::filesystem::current_path();
		const std::array<std::filesystem::path, 4> candidates = {
			sourcePath,
			m_UIAssetRoot / sourcePath,
			cwd / sourcePath,
			cwd / m_UIAssetRoot / sourcePath,
		};

		for (const std::filesystem::path& candidate : candidates)
		{
			if (std::filesystem::exists(candidate))
				return candidate;
		}

		return {};
	}

	bool RmlRenderInterfaceDX12::UploadTextureRecord(TextureRecord& record)
	{
		return record.Ready;
	}

	DescriptorHeapHandle RmlRenderInterfaceDX12::BuildTransientTextureDescriptor(const TextureRecord& textureRecord)
	{
		DescriptorHeapHandle handle;
		if (!m_RenderContext || !m_Device || !textureRecord.PersistentSrv.IsValid())
			return handle;

		try
		{
			handle = m_RenderContext->GetHeapManager().AllocateTransientSRVBlock(1);
			m_Device->CopyDescriptorsSimple(
				1,
				handle.GetCPUHandle(),
				textureRecord.PersistentSrv.GetCPUHandle(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		catch (...)
		{
			return DescriptorHeapHandle{};
		}

		return handle;
	}
}
