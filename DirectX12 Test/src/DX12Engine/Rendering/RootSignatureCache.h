#pragma once
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include <stdexcept>

namespace DX12Engine
{
    class RootSignatureCache 
    {
    public:
		RootSignatureCache(ID3D12Device* device)
			: m_Device(device)
		{}
		~RootSignatureCache() = default;

        // Retrieve or create a root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> GetOrCreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
        {
            size_t hash = HashRootSignature(desc);

            // Check if the root signature already exists
            auto it = m_Cache.find(hash);
            if (it != m_Cache.end())
                return it->second;

            // Serialize the root signature
            Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
            if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) 
            {
                if (errorBlob)
                    OutputDebugStringA((char*)errorBlob->GetBufferPointer());
                throw std::runtime_error("Failed to serialize root signature");
            }

            // Create the root signature
            Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
            if (FAILED(m_Device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) 
            {
                throw std::runtime_error("Failed to create root signature");
            }

            // Store in cache and return
            m_Cache[hash] = rootSignature;
            return rootSignature;
        }

    private:
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_Cache;
        ID3D12Device* m_Device;

        // Simple hash function for the root signature
		size_t HashRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
		{
			size_t seed = 0;
			HashCombine(seed, desc.NumParameters);
			HashCombine(seed, desc.NumStaticSamplers);
			HashCombine(seed, desc.Flags);
			for (UINT i = 0; i < desc.NumParameters; ++i)
			{
				HashCombine(seed, desc.pParameters[i].ParameterType);
			}
			return seed;
		}

        inline void HashCombine(std::size_t& seed, std::size_t hash)
        {
            seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };
}

