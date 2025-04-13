#include "Material.h"
#include "../ResourceManager.h"

namespace DX12Engine
{
	Material::Material()
		: m_EnvironmentMapHandle(nullptr), m_ShadowMapHandle(nullptr), m_ShadowCubeMapHandle(nullptr)
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(MaterialData));
	}

	Material::~Material()
	{
	}

	void Material::BuildPipelineState()
	{
		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(RootSignatureBuilder.Build());
		PipelineStateBuilder = PipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(PipelineStateBuilder.Build());
	}

	void Material::ConfigureFromDefault(Shader* vertexShader, Shader* pixelShader, int numTextures)
	{
		PipelineStateBuilder = PipelineStateBuilder.ConfigureFromDefault().SetVertexShader(vertexShader).SetPixelShader(pixelShader);
		RootSignatureBuilder = RootSignatureBuilder.ConfigureFromDefault(numTextures);
		BuildPipelineState();
	}

	void Material::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex)
	{
		commandList->SetPipelineState(m_PipelineState.Get());
		commandList->SetGraphicsRootConstantBufferView((*startIndex)++, GetCBVAddress());
	}

	void Material::UpdateConstantBufferData(MaterialData materialData)
	{
		m_ConstantBuffer->Update(&materialData, sizeof(materialData));
	}
}
