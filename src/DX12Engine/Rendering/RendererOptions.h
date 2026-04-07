#pragma once

namespace DX12Engine
{
	enum class AntiAliasingMode
	{
		None,
		FXAA,
		TAA
	};

	struct TAASettings
	{
		float BlendFactor = 0.1f;
		float VelocitySpread = 1.0f;
		float Sharpness = 0.5f;
	};

	struct RendererOptions
	{
		AntiAliasingMode AA_Mode = AntiAliasingMode::None;
		TAASettings TAA;
		bool EnableGammaCorrection = true;
	};

	struct PostProcessingData
	{
		int EnableGammaCorrection = 1;
		int EnableFXAA = 0;
	};
}