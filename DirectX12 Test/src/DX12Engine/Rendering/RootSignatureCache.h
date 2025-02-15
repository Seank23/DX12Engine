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
        // Retrieve or create a root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> GetOrCreateRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc)
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
            if (FAILED(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) 
            {
                throw std::runtime_error("Failed to create root signature");
            }

            // Store in cache and return
            m_Cache[hash] = rootSignature;
            return rootSignature;
        }

    private:
        std::unordered_map<size_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_Cache;

        // Simple hash function for the root signature
        size_t HashRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc) 
        {
            return std::hash<size_t>()(reinterpret_cast<size_t>(&desc)); // Can be improved
        }
    };
}

