#pragma once
#include "RenderPipelineConfig.h"

namespace DX12Engine
{
	class PipelineBuilder
	{
	public:
		PipelineBuilder& AddPass(const RenderPassConfig& pass)
		{
			m_Passes.push_back(pass);
			return *this;
		}
		PipelineBuilder& AddPassIf(bool enabled, const RenderPassConfig& pass)
		{
			if (enabled)
			{
				m_Passes.push_back(pass);
			}
			return *this;
		}

		RenderPipelineConfig Build()
		{
			RenderPipelineConfig config;
			config.Passes = m_Passes;
			return config;
		}

	private:
		std::vector<RenderPassConfig> m_Passes;
	};
}