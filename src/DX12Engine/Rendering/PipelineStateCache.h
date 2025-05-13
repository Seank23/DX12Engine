#pragma once
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include <stdexcept>

namespace DX12Engine
{
    class PipelineStateCache
    {
    public:
		PipelineStateCache(ID3D12Device* device)
			: m_Device(device)
		{}
		~PipelineStateCache() = default;

        // Retrieve or create a PSO
        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
        {
            size_t hash = HashPSO(desc);

            // Check if PSO already exists
            auto it = m_Cache.find(hash);
            if (it != m_Cache.end())
                return it->second;

            // Create new PSO
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
			auto hr = m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
            if (FAILED(hr))
                throw std::runtime_error("Failed to create pipeline state");

            // Store in cache and return
            m_Cache[hash] = pso;
            return pso;
        }

    private:
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_Cache;
        ID3D12Device* m_Device;

        // Simple hashing function for PSO description
        size_t HashPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
		{
            std::size_t seed = 0;

            // Shaders
            if (desc.VS.pShaderBytecode)
                HashCombine(seed, std::hash<std::string_view>()(
                    std::string_view((const char*)desc.VS.pShaderBytecode, desc.VS.BytecodeLength)));

            if (desc.PS.pShaderBytecode)
                HashCombine(seed, std::hash<std::string_view>()(
                    std::string_view((const char*)desc.PS.pShaderBytecode, desc.PS.BytecodeLength)));

            // Render target formats
            for (UINT i = 0; i < desc.NumRenderTargets; ++i)
                HashCombine(seed, desc.RTVFormats[i]);

            // Depth/stencil format
            HashCombine(seed, desc.DSVFormat);

            // Sample info
            HashCombine(seed, desc.SampleDesc.Count);
            HashCombine(seed, desc.SampleDesc.Quality);

            // Rasterizer, blend, and depth-stencil states (can also hash raw memory if structs are POD)
            HashCombine(seed, std::hash<uint64_t>()(*(const uint64_t*)&desc.RasterizerState));
            HashCombine(seed, std::hash<uint64_t>()(*(const uint64_t*)&desc.BlendState));
            HashCombine(seed, std::hash<uint64_t>()(*(const uint64_t*)&desc.DepthStencilState));

            // Topology, node mask, etc.
            HashCombine(seed, desc.PrimitiveTopologyType);
            HashCombine(seed, desc.SampleMask);
            HashCombine(seed, desc.NodeMask);

            return seed;
        }

        inline void HashCombine(std::size_t& seed, std::size_t hash)
        {
            seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}
