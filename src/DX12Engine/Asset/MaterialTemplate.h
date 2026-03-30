#pragma once
#include <string>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include "../Rendering/PipelineStateBuilder.h"
#include "../Rendering/RootSignatureBuilder.h"

namespace DX12Engine
{
	// Rasterizer policy: fields that differ between template variants (e.g. glass vs opaque).
	struct RasterizerPolicy
	{
		D3D12_CULL_MODE CullMode  = D3D12_CULL_MODE_BACK;
		D3D12_FILL_MODE FillMode  = D3D12_FILL_MODE_SOLID;
		int             DepthBias = 0;
		float           SlopeScaledDepthBias = 0.0f;
	};

	// Blend policy: maps to a D3D12 blend desc and controls transparency behaviour.
	enum class BlendPolicy : uint8_t
	{
		Opaque  = 0,
		Masked  = 1,
		Blend   = 2,
	};

	// Output targets that the template is compiled against (drives RT formats in the PSO).
	enum class PassTarget : uint8_t
	{
		Geometry = 0, // deferred GBuffer pass (5 MRT + depth)
	};

	// MaterialTemplate owns shader references and PSO configuration policy.
	// Many MaterialAssets can share one template (e.g. all opaque PBR car parts).
	// The actual PSO is resolved once via ResolvePSO() and cached here.
	class MaterialTemplate
	{
	public:
		MaterialTemplate() = default;
		~MaterialTemplate() = default;

		// ----- Shader refs (names into ResourceManager's shader map) -----
		void SetVertexShader(std::string name) { m_VertexShaderName = std::move(name); InvalidateCache(); }
		void SetPixelShader(std::string name)  { m_PixelShaderName  = std::move(name); InvalidateCache(); }
		const std::string& GetVertexShaderName() const { return m_VertexShaderName; }
		const std::string& GetPixelShaderName()  const { return m_PixelShaderName;  }

		// ----- PSO policy -----
		void SetRasterizerPolicy(RasterizerPolicy policy) { m_RasterizerPolicy = policy; InvalidateCache(); }
		void SetBlendPolicy(BlendPolicy policy)           { m_BlendPolicy      = policy; InvalidateCache(); }
		void SetPassTarget(PassTarget target)             { m_PassTarget       = target; InvalidateCache(); }

		const RasterizerPolicy& GetRasterizerPolicy() const { return m_RasterizerPolicy; }
		BlendPolicy             GetBlendPolicy()      const { return m_BlendPolicy;      }
		PassTarget              GetPassTarget()       const { return m_PassTarget;       }

		// A stable integer key that uniquely identifies this template's PSO variant.
		// Used directly as DrawItem::PipelineKey to drive front-to-back PSO sorting.
		uint64_t GetPipelineKey() const { return m_PipelineKey; }

		// Resolve (or return cached) PSO and root signature for this template.
		// Must be called after ResourceManager::Init().
		void ResolvePSO();

		bool HasResolvedPSO() const { return m_PipelineState != nullptr; }

		ID3D12PipelineState*   GetPipelineState()   const { return m_PipelineState.Get();   }
		ID3D12RootSignature*   GetRootSignature()   const { return m_RootSignature.Get();   }

	private:
		void InvalidateCache();
		void RebuildPipelineKey();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC BuildPSODesc();
		D3D12_ROOT_SIGNATURE_DESC           BuildRootSignatureDesc();

		std::string       m_VertexShaderName = "Geometry_VS";
		std::string       m_PixelShaderName  = "Geometry_PS";
		RasterizerPolicy  m_RasterizerPolicy;
		BlendPolicy       m_BlendPolicy = BlendPolicy::Opaque;
		PassTarget        m_PassTarget  = PassTarget::Geometry;
		uint64_t          m_PipelineKey = 0;

		// Cached resolved objects (null until ResolvePSO() is called).
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

		// Root-signature ranges and input-layout elements must outlive the descs used
		// during creation; keep the builders as members so their storage is stable.
		RootSignatureBuilder  m_RootSignatureBuilder;
		PipelineStateBuilder  m_PipelineStateBuilder;
	};
}
