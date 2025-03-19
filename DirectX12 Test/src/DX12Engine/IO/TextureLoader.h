#pragma once
#include "../Resources/Texture.h"

namespace DX12Engine
{
	class TextureLoader
	{
	public:
		TextureLoader();
		~TextureLoader();

		std::unique_ptr<Texture> LoadDDS(const std::wstring& filename);
		std::unique_ptr<Texture> LoadWIC(const std::wstring& filename);
	};
}


