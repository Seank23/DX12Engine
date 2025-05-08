#include "TextureLoader.h"
#include "DirectXTex.h"
#include "../Utils/EngineUtils.h"
#include "../Resources/ResourceManager.h"
#include <iostream>
#include <filesystem>

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

	std::unordered_map<TextureType, std::shared_ptr<Texture>> TextureLoader::LoadMaterial(std::wstring path)
	{
		std::unordered_map<TextureType, std::shared_ptr<Texture>> textures;
		if (std::filesystem::exists(path + L"/albedo.png"))
			textures[TextureType::Albedo] = LoadWIC(path + L"/albedo.png");
		if (std::filesystem::exists(path + L"/normal.png"))
			textures[TextureType::Normal] = LoadWIC(path + L"/normal.png");
		if (std::filesystem::exists(path + L"/metallic.png"))
			textures[TextureType::Metallic] = LoadWIC(path + L"/metallic.png");
		if (std::filesystem::exists(path + L"/roughness.png"))
			textures[TextureType::Roughness] = LoadWIC(path + L"/roughness.png");
		if (std::filesystem::exists(path + L"/ao.png"))
			textures[TextureType::AOMap] = LoadWIC(path + L"/ao.png");
		return textures;
	}
}
