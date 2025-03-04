#include "Material.h"
#include "ResourceManager.h"

namespace DX12Engine
{
	Material::Material(DirectX::XMFLOAT4 color)
	{
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(MaterialData));
		SetColor(color);
	}

	Material::~Material()
	{
	}

	void Material::SetColor(DirectX::XMFLOAT4 color)
	{
		m_MaterialData.BaseColor = color;
		UpdateConstantBufferData();
	}

	void Material::SetTexture(std::shared_ptr<Texture> texture)
	{
		m_Texture = texture;
		m_MaterialData.HasTexture = 1;
		UpdateConstantBufferData();
	}

	void Material::BuildPipelineState()
	{
		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(RootSignatureBuilder.Build());
		PipelineStateBuilder = PipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(PipelineStateBuilder.Build());
	}

	void Material::ConfigureFromDefault(Shader* vertexShader, Shader* pixelShader)
	{
		PipelineStateBuilder = PipelineStateBuilder.ConfigureFromDefault().SetVertexShader(vertexShader).SetPixelShader(pixelShader);
		RootSignatureBuilder = RootSignatureBuilder.ConfigureFromDefault();
		BuildPipelineState();
	}

	void Material::UpdateConstantBufferData()
	{
		m_ConstantBuffer->Update(&m_MaterialData, sizeof(MaterialData));
	}
}
