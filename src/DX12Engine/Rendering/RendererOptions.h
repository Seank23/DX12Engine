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
		// Baseline history contribution before adaptive rejection terms are applied.
		float BaseBlend = 0.82f;
		// Lower clamp for history weight; higher values reduce flicker but can increase ghosting.
		float MinBlend = 0.08f;
		// Upper clamp for history weight; lower values bias toward current frame and reduce trails.
		float MaxBlend = 0.88f;
		// Scales how strongly motion vectors suppress history reuse.
		float VelocityRejection = 0.14f;
		// Scales how strongly depth disagreement suppresses history reuse.
		float DepthRejection = 250.0f;
		// Variance-clip radius in YCoCg space; smaller values clamp history more aggressively.
		float ClampGamma = 1.25f;
		// Post-resolve sharpening amount (gated by motion/disocclusion in shader).
		float Sharpness = 0.05f;
		// Absolute depth delta that triggers hard disocclusion history rejection.
		float DisocclusionDepthThreshold = 0.004f;
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