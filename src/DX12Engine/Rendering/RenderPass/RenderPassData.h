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

