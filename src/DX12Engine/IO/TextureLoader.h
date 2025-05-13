#pragma once
#include "../Resources/Texture.h"
#include <unordered_map>

namespace DX12Engine
{
	class TextureLoader
	{
	public:
		TextureLoader();
		~TextureLoader();

		std::unique_ptr<Texture> LoadDDS(const std::wstring& filename);
		std::unique_ptr<Texture> LoadCubemapDDS(const std::wstring& filename);
		std::unique_ptr<Texture> LoadWIC(const std::wstring& filename);

		std::unordered_map<TextureType, std::shared_ptr<Texture>> LoadMaterial(std::wstring path);

		static std::vector<Texture*> GetTextureArray(std::unordered_map<TextureType, std::shared_ptr<Texture>> textures)
		{
			std::vector<Texture*> textureArray;
			for (auto& texture : textures)
				textureArray.push_back(texture.second.get());
			return textureArray;
		}
	};
}


