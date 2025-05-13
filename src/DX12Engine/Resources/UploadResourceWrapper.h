#pragma once
#include "d3d12.h"
#include "GPUResource.h"

namespace DX12Engine
{
	struct UploadResourceWrapper
	{
		GPUResource* GPUResource;
		ID3D12Resource* UploadResource;
		D3D12_RESOURCE_STATES UploadState;
		D3D12_SUBRESOURCE_DATA  Data;
	};
}

