#include "TextureLoader.h"
#include "DirectXTex.h"
#include "../Utils/EngineUtils.h"
#include "../Resources/ResourceManager.h"
#include <iostream>

namespace DX12Engine
{
	TextureLoader::TextureLoader()
	{
	}

	TextureLoader::~TextureLoader()
	{
	}

	std::unique_ptr<Texture> TextureLoader::LoadDDS(const std::wstring& filename)
	{
		DirectX::ScratchImage* imageData = new DirectX::ScratchImage();
		EngineUtils::ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData));
		return ResourceManager::GetInstance().CreateTexture(imageData);
	}

	std::unique_ptr<Texture> TextureLoader::LoadCubemapDDS(const std::wstring& filename)
	{
		DirectX::ScratchImage* imageData = new DirectX::ScratchImage();
		EngineUtils::ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData));
		const DirectX::TexMetadata& metadata = imageData->GetMetadata();
		if (!metadata.IsCubemap())
			throw std::runtime_error("Loaded texture is not a cubemap!");

		return ResourceManager::GetInstance().CreateCubeMap(imageData);
	}

	std::unique_ptr<Texture> TextureLoader::LoadWIC(const std::wstring& filename)
	{
		DirectX::ScratchImage* imageData = new DirectX::ScratchImage();
		EngineUtils::ThrowIfFailed(DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, *imageData));
		return ResourceManager::GetInstance().CreateTexture(imageData);
	}
}
