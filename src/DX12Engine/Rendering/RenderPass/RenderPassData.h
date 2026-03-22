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
};

struct SSRTemporalData
{
	DirectX::XMMATRIX PrevViewMatrix;
	DirectX::XMMATRIX PrevProjectionMatrix;
	uint32_t FrameIndex;
	DirectX::XMFLOAT3 Padding;
};

