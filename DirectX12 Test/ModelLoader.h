#pragma once
#include "Mesh.h"
#include <string>

namespace DX12Engine
{
	class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		Mesh LoadObj(const std::string& filename);
	};
}

