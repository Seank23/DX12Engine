#pragma once
#include <RmlUi/Core.h>

#include "../../Rendering/Heaps/DescriptorHeapHandle.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12RootSignature;

namespace DX12Engine
{
	class RenderContext;
	class UIRenderContext;

	class RmlRenderInterfaceDX12 : public Rml::RenderInterface
	{
	public:
		RmlRenderInterfaceDX12();
		virtual ~RmlRenderInterfaceDX12();

		bool Initialize(RenderContext* renderContext, std::filesystem::path uiAssetRoot = "res/UI");
		void Shutdown();
		void BeginFrame(const UIRenderContext& context);
		void EndFrame();

		virtual Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
		virtual void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
		virtual void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
		void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation);
		void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry);

		virtual Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
		virtual Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
		virtual void ReleaseTexture(Rml::TextureHandle texture_handle) override;

		virtual void SetTransform(const Rml::Matrix4f* transform) override;
		virtual void SetScissorRegion(Rml::Rectanglei region) override;
		virtual void EnableScissorRegion(bool enable) override;

	private:
		struct CompiledGeometryRecord
		{
			std::vector<Rml::Vertex> Vertices;
			std::vector<uint32_t> Indices;
			Rml::TextureHandle CachedTexture = 0;
		};

		struct TextureRecord
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
			Microsoft::WRL::ComPtr<ID3D12Resource> UploadResource;
			DescriptorHeapHandle PersistentSrv;
			uint32_t Width = 0;
			uint32_t Height = 0;
			bool Ready = false;
		};

		struct Constants
		{
			std::array<float, 16> Transform{};
			std::array<float, 2> Translation{};
			std::array<float, 2> Padding{};
		};

		struct PendingUploadRelease
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
			uint64_t FenceValue = 0;
		};

		bool EnsurePipelineResources();
		bool EnsureConstantBuffer();
		bool EnsureTransientBuffers(size_t vertexBytes, size_t indexBytes);
		void ResetTransientBufferOffsets();
		void UpdateConstants(Rml::Vector2f translation);
		bool DrawGeometry(const CompiledGeometryRecord& geometry, Rml::TextureHandle texture, Rml::Vector2f translation);

		Rml::TextureHandle RegisterTexture(TextureRecord&& record);
		Rml::TextureHandle EnsureWhiteTexture();
		bool CreateTextureFromPixels(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions, TextureRecord& outRecord);
		bool CreateTextureFromFile(const std::filesystem::path& filePath, TextureRecord& outRecord, Rml::Vector2i* outDimensions = nullptr);
		std::filesystem::path ResolveTexturePath(const Rml::String& source) const;
		bool UploadTextureRecord(TextureRecord& record);
		DescriptorHeapHandle BuildTransientTextureDescriptor(const TextureRecord& textureRecord);
		void QueueUploadResourceRelease(Microsoft::WRL::ComPtr<ID3D12Resource>& uploadResource);
		void ProcessPendingUploadReleases();

		RenderContext* m_RenderContext = nullptr;
		ID3D12Device* m_Device = nullptr;
		ID3D12GraphicsCommandList* m_CommandList = nullptr;

		D3D12_VIEWPORT m_CurrentViewport = {};
		D3D12_RECT m_DefaultScissor = {};
		D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentRTV = {};
		DXGI_FORMAT m_CurrentRenderTargetFormat = DXGI_FORMAT_UNKNOWN;
		float m_LogicalWidth = 0.0f;
		float m_LogicalHeight = 0.0f;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		DXGI_FORMAT m_PsoRenderTargetFormat = DXGI_FORMAT_UNKNOWN;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBuffer;
		uint8_t* m_MappedConstantBuffer = nullptr;
		size_t m_ConstantBufferSize = 0;
		size_t m_ConstantBufferOffset = 0;

		Microsoft::WRL::ComPtr<ID3D12Resource> m_TransientVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_TransientIndexBuffer;
		uint8_t* m_MappedVertexBuffer = nullptr;
		uint8_t* m_MappedIndexBuffer = nullptr;
		size_t m_TransientVertexBufferSize = 0;
		size_t m_TransientIndexBufferSize = 0;
		size_t m_TransientVertexOffset = 0;
		size_t m_TransientIndexOffset = 0;

		std::unordered_map<Rml::CompiledGeometryHandle, CompiledGeometryRecord> m_GeometryMap;
		std::unordered_map<Rml::TextureHandle, TextureRecord> m_TextureMap;
		std::unordered_map<Rml::TextureHandle, DescriptorHeapHandle> m_FrameTextureTableCache;
		std::vector<PendingUploadRelease> m_PendingUploadReleases;
		Rml::CompiledGeometryHandle m_NextGeometryHandle = 1;
		Rml::TextureHandle m_NextTextureHandle = 1;
		Rml::TextureHandle m_WhiteTextureHandle = 0;

		bool m_ScissorEnabled = false;
		Rml::Rectanglei m_ScissorRegion = {};
		Rml::Matrix4f m_CurrentTransform = Rml::Matrix4f::Identity();
		bool m_HasTransform = false;
		bool m_IsInitialized = false;

		std::filesystem::path m_UIAssetRoot = "res/UI";
	};
}
