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
        // Retrieve or create a PSO
        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreatePSO(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
        {
            size_t hash = HashPSO(desc);

            // Check if PSO already exists
            auto it = m_Cache.find(hash);
            if (it != m_Cache.end())
                return it->second;

            // Create new PSO
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
			auto hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
            if (FAILED(hr))
                throw std::runtime_error("Failed to create pipeline state");

            // Store in cache and return
            m_Cache[hash] = pso;
            return pso;
        }

    private:
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_Cache;

        // Simple hashing function for PSO description
        size_t HashPSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
        {
            return std::hash<size_t>()(reinterpret_cast<size_t>(&desc)); // Can be improved for more uniqueness
        }
    };
}
