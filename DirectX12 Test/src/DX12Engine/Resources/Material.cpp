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
		m_MaterialData.HasTexture = true;
		UpdateConstantBufferData();
	}

	void Material::SetPipelineState(PipelineStateBuilder& psBuilder, RootSignatureBuilder& rsBuilder)
	{
		auto rs = ResourceManager::GetInstance().CreateRootSignature(rsBuilder.Build());
		psBuilder = psBuilder.SetRootSignature(rs.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(psBuilder.Build());
	}

	void Material::UpdateConstantBufferData()
	{
		m_ConstantBuffer->Update(&m_MaterialData, sizeof(MaterialData));
	}
}
