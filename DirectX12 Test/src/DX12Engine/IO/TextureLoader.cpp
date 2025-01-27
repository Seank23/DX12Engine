#include "TextureLoader.h"
#include "DirectXTex.h"
#include "../Utils/EngineUtils.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	TextureLoader::TextureLoader()
	{
	}

	TextureLoader::~TextureLoader()
	{
	}

	std::unique_ptr<Texture> TextureLoader::LoadDDS(const std::string& filename)
	{
		DirectX::ScratchImage* imageData = new DirectX::ScratchImage();
		EngineUtils::ThrowIfFailed(DirectX::LoadFromDDSFile((wchar_t*)filename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData));
		return ResourceManager::GetInstance().CreateTexture(imageData);
	}

	std::unique_ptr<Texture> TextureLoader::LoadWIC(const std::wstring& filename)
	{
		DirectX::ScratchImage* imageData = new DirectX::ScratchImage();
		EngineUtils::ThrowIfFailed(DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, *imageData));
		return ResourceManager::GetInstance().CreateTexture(imageData);
	}
}
