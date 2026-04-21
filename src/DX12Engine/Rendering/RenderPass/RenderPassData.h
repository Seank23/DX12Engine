#pragma once
#include "DirectXMath.h"

struct ScreenData
{
	DirectX::XMFLOAT4 CameraPosition;
	DirectX::XMMATRIX ViewMatrix;
	DirectX::XMMATRIX ProjectionMatrix;
	DirectX::XMMATRIX InvViewMatrix;
	DirectX::XMMATRIX InvProjectionMatrix;
	DirectX::XMFLOAT2 ScreenSize;
	DirectX::XMFLOAT2 Jitter;
	DirectX::XMFLOAT2 PrevJitter;
};

struct SSRTemporalData
{
	DirectX::XMMATRIX PrevViewMatrix;
	DirectX::XMMATRIX PrevProjectionMatrix;
	uint32_t FrameIndex;
	DirectX::XMFLOAT3 Padding;
};

struct TAATemporalData
{
	uint32_t FrameIndex;
	uint32_t EnableHistoryReset;
	float BaseBlend;
	float MinBlend;
	float MaxBlend;
	float VelocityRejection;
	float DepthRejection;
	float ClampGamma;
	float Sharpness;
	float DisocclusionDepthThreshold;
};

struct CascadedShadowData
{
	DirectX::XMMATRIX CascadeViewProj[4];
	DirectX::XMFLOAT4 CascadeSplits;      // view-space split end distances
	DirectX::XMFLOAT4 CascadeTexelSize;   // world units/texel per cascade
	DirectX::XMFLOAT4 Params0;            // x=count, y=maxDist, z=blend, w=unused
	DirectX::XMFLOAT4 BiasParams;         // x=const, y=slope, z=normal, w=unused
};

