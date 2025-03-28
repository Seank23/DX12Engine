#pragma once
#include <d3dx12.h>

namespace DX12Engine
{
    struct DescriptorTableConfig
    {
        UINT NumDescriptors;
        D3D12_DESCRIPTOR_RANGE_TYPE Type; 
        UINT BaseShaderRegister;
        UINT Space = 0;

        DescriptorTableConfig(UINT numDescriptors, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT baseShaderRegister)
            : NumDescriptors(numDescriptors), Type(type), BaseShaderRegister(baseShaderRegister) {}
    };

    class RootSignatureBuilder 
    {
    public:
        RootSignatureBuilder() 
        {
            ZeroMemory(&m_RootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
        }

        RootSignatureBuilder& ConfigureFromDefault(int numTextures = 1)
        {
            DescriptorTableConfig config(numTextures, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
            return AddConstantBuffer(0)
                .AddConstantBuffer(1)
                .AddConstantBuffer(2)
                .AddDescriptorTables({config})
                .AddSampler(0, D3D12_FILTER_ANISOTROPIC);
        }

        RootSignatureBuilder& AddDescriptorTables(std::vector<DescriptorTableConfig> configs) 
        {
            for (int i = 0; i < configs.size(); i++)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = configs[i].Type;
                range.NumDescriptors = configs[i].NumDescriptors;
                range.BaseShaderRegister = configs[i].BaseShaderRegister;
                range.RegisterSpace = configs[i].Space;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                m_DescriptorRanges.push_back(range);
            }
            for (int i = 0; i < configs.size(); i++)
            {
                CD3DX12_ROOT_PARAMETER param{};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = 1;
                param.DescriptorTable.pDescriptorRanges = &m_DescriptorRanges[i];
                param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

                m_Parameters.push_back(param);
            }
            return *this;
        }

        RootSignatureBuilder& AddConstantBuffer(UINT shaderRegister, UINT space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
        {
            CD3DX12_ROOT_PARAMETER param = {};
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
            staticSamplerDesc.MaxAnisotropy = 16;
            staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            staticSamplerDesc.ShaderRegister = shaderRegister;
            m_StaticSamplers.push_back(staticSamplerDesc);
            return *this;
        }

        RootSignatureBuilder& AddShadowMapSampler(UINT shaderRegister)
        {
            D3D12_STATIC_SAMPLER_DESC shadowSampler = {};
            shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
            shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            shadowSampler.MipLODBias = 0.0f;
            shadowSampler.MaxAnisotropy = 1;
            shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            shadowSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
            shadowSampler.MinLOD = 0.0f;
            shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
            shadowSampler.ShaderRegister = shaderRegister; // Register "s0"
            shadowSampler.RegisterSpace = 0;
            shadowSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            m_StaticSamplers.push_back(shadowSampler);
            return *this;
        }

        void AddCustomParam(CD3DX12_ROOT_PARAMETER param)
        {
            m_Parameters.push_back(param);
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
        std::vector<CD3DX12_ROOT_PARAMETER> m_Parameters;
        std::vector<D3D12_DESCRIPTOR_RANGE> m_DescriptorRanges;
        std::vector<D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers;
    };

}
