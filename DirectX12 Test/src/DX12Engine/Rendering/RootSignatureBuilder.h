#pragma once
#include <d3dx12.h>

namespace DX12Engine
{
    class RootSignatureBuilder 
    {
    public:
        RootSignatureBuilder() 
        {
            ZeroMemory(&m_RootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
        }

        RootSignatureBuilder& ConfigureFromDefault()
		{
            return AddConstantBuffer(0)
                .AddConstantBuffer(1)
                .AddDescriptorTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0)
                .AddSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
		}

        RootSignatureBuilder& AddDescriptorTable(UINT numDescriptors, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT baseShaderRegister, UINT space = 0) 
        {
            D3D12_DESCRIPTOR_RANGE range = {};
            range.RangeType = type;
            range.NumDescriptors = numDescriptors;
            range.BaseShaderRegister = baseShaderRegister;
            range.RegisterSpace = space;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            m_DescriptorRanges.push_back(range);

            // Create descriptor table entry
            D3D12_ROOT_PARAMETER param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.DescriptorTable.NumDescriptorRanges = 1;
            param.DescriptorTable.pDescriptorRanges = &m_DescriptorRanges.back();
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            m_Parameters.push_back(param);
            return *this;
        }

        RootSignatureBuilder& AddConstantBuffer(UINT shaderRegister, UINT space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) 
        {
            D3D12_ROOT_PARAMETER param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            param.Descriptor.ShaderRegister = shaderRegister;
            param.Descriptor.RegisterSpace = space;
            param.ShaderVisibility = visibility;

            m_Parameters.push_back(param);
            return *this;
        }

        RootSignatureBuilder& AddSampler(UINT shaderRegister, D3D12_FILTER filter) 
        {
            D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};
            staticSamplerDesc.Filter = filter;
            staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            staticSamplerDesc.ShaderRegister = shaderRegister;
            m_StaticSamplers.push_back(staticSamplerDesc);
            return *this;
        }

        D3D12_ROOT_SIGNATURE_DESC Build() 
        {
            m_RootSignatureDesc.NumParameters = static_cast<UINT>(m_Parameters.size());
            m_RootSignatureDesc.pParameters = m_Parameters.data();
            m_RootSignatureDesc.NumStaticSamplers = static_cast<UINT>(m_StaticSamplers.size());
            m_RootSignatureDesc.pStaticSamplers = m_StaticSamplers.data();
            m_RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            return m_RootSignatureDesc;
        }

    private:
        D3D12_ROOT_SIGNATURE_DESC m_RootSignatureDesc;
        std::vector<D3D12_ROOT_PARAMETER> m_Parameters;
        std::vector<D3D12_DESCRIPTOR_RANGE> m_DescriptorRanges;
        std::vector<D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers;
    };

}
