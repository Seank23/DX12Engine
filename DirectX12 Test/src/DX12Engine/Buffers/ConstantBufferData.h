#pragma once
#include <DirectXMath.h>

namespace DX12Engine
{
	struct ConstantBufferData
	{
		DirectX::XMMATRIX WVPMatrix;

		void Reset()
		{
			WVPMatrix = DirectX::XMMatrixIdentity();
		}
	};
}
