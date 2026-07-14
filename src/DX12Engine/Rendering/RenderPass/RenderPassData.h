#pragma once
#include "DirectXMath.h"
#include "../../Utils/Constants.h"

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

struct alignas(16) AlignedFloat
{
	float Value;
	float Padding[3];
};

struct alignas(16) CascadedShadowData
{
	DirectX::XMMATRIX CascadeViewProj[MAX_CSM_CASCADES];
	// HLSL cbuffer arrays of scalar values consume one 16-byte register per element.
	AlignedFloat CascadeSplits[MAX_CSM_CASCADES];	 // view-space split end distances
	AlignedFloat CascadeTexelSize[MAX_CSM_CASCADES]; // world units/texel per cascade
	DirectX::XMFLOAT4 Params0;						 // x=count, y=maxDist, z=blend, w=unused
	DirectX::XMFLOAT4 BiasParams;					 // x=const, y=slope, z=normal, w=unused
};

static_assert(sizeof(AlignedFloat) == 16, "AlignedFloat must map to one 16-byte cbuffer register");
static_assert(sizeof(CascadedShadowData) == 800, "CascadedShadowData size no longer matches HLSL cbuffer layout");
