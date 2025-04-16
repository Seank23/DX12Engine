#pragma once
#include "d3d12.h"
#include <wrl.h>
#include <DirectXMath.h>
#include "../../Rendering/PipelineStateBuilder.h" 
#include "../../Rendering/RootSignatureBuilder.h"
#include "../../Buffers/ConstantBuffer.h"
#include "./MaterialData.h"

namespace DX12Engine
{
	class Texture;

	enum TextureType
	{
		Albedo,
		Normal,
		Metallic,
		Roughness,
		AOMap
	};

	class Material
	{
	public:
		Material();
		~Material();

		void BuildPipelineState();

		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }
		Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() { return m_PipelineState; }
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() { return m_RootSignature; }

		void ConfigureFromDefault(Shader* vertexShader, Shader* pixelShader, int numTextures = 1);

		virtual Texture* GetTexture(TextureType type) = 0;
		virtual bool HasTexture(TextureType type) = 0;

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex, bool bindPipelineState = true);

		void SetEnvironmentMapHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle) { m_EnvironmentMapHandle = handle; }
		void SetShadowMapHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle) { m_ShadowMapHandle = handle; }
		void SetShadowCubeMapHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle) { m_ShadowCubeMapHandle = handle; }

		PipelineStateBuilder PipelineStateBuilder;
		RootSignatureBuilder RootSignatureBuilder;

	protected:
		void UpdateConstantBufferData(MaterialData materialData);

		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

		D3D12_GPU_DESCRIPTOR_HANDLE* m_EnvironmentMapHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE* m_ShadowMapHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE* m_ShadowCubeMapHandle;
	};
}
