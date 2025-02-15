#pragma once
#include <d3d12.h>

namespace DX12Engine
{
    class PipelineStateBuilder 
    {
    public:
        PipelineStateBuilder() 
        {
            ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        }

        PipelineStateBuilder& SetRootSignature(ID3D12RootSignature* rootSig) 
        {
            psoDesc.pRootSignature = rootSig;
            return *this;
        }

        PipelineStateBuilder& SetVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength) 
        {
            psoDesc.VS = { shaderBytecode, bytecodeLength };
            return *this;
        }

        PipelineStateBuilder& SetPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength) 
        {
            psoDesc.PS = { shaderBytecode, bytecodeLength };
            return *this;
        }

        PipelineStateBuilder& SetBlendState(const D3D12_BLEND_DESC& blendState) 
        {
            psoDesc.BlendState = blendState;
            return *this;
        }

        PipelineStateBuilder& SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizerState) 
        {
            psoDesc.RasterizerState = rasterizerState;
            return *this;
        }

        PipelineStateBuilder& SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencilState) 
        {
            psoDesc.DepthStencilState = depthStencilState;
            return *this;
        }

        PipelineStateBuilder& SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology) 
        {
            psoDesc.PrimitiveTopologyType = topology;
            return *this;
        }

        PipelineStateBuilder& AddInputLayout(D3D12_INPUT_ELEMENT_DESC* inputLayout = nullptr, UINT count = 4)
        {
			if (inputLayout == nullptr)
                psoDesc.InputLayout = { inputElementDescs, count };
            else
				psoDesc.InputLayout = { inputLayout, count };
            return *this;
        }

        PipelineStateBuilder& SetRenderTargetFormats(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat) 
        {
            psoDesc.RTVFormats[0] = rtvFormat;
            psoDesc.DSVFormat = dsvFormat;
            psoDesc.NumRenderTargets = 1;
            return *this;
        }

        PipelineStateBuilder& SetSampleDesc(UINT mask, UINT count, UINT quality)
        {
            DXGI_SAMPLE_DESC sampleDesc{ count, quality };
            psoDesc.SampleMask = mask;
            psoDesc.SampleDesc = sampleDesc;
            return *this;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC Build() 
        {
            return psoDesc;
        }

    private:
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
    };

}
