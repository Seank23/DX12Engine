#pragma once
#include <DirectXMath.h>
#include "Mesh.h"
#include "../Buffers/VertexBuffer.h"
#include "../Buffers/IndexBuffer.h"
#include "../Rendering/RenderObject.h"
#include "Materials/SkyboxMaterial.h"

namespace DX12Engine
{
	class Skybox : public RenderObject
	{
	public:
		Skybox(std::shared_ptr<Texture> skyboxTexture);
		~Skybox();

	private:
		std::vector<Vertex> skyboxVertices = 
		{
			{{-1.0f, -1.0f, -1.0f}},
			{{1.0f, -1.0f, -1.0f}},
			{{-1.0f, 1.0f, -1.0f}},
			{{1.0f, 1.0f, -1.0f}},
			{{-1.0f, -1.0f, 1.0f}},
			{{1.0f, -1.0f, 1.0f}},
			{{-1.0f, 1.0f, 1.0f}},
			{{1.0f, 1.0f, 1.0f}},
		};

		std::vector<UINT> skyboxIndices = 
		{
			0, 2, 1, 2, 3, 1,  // Back
			1, 3, 5, 3, 7, 5,  // Front
			2, 6, 3, 3, 6, 7,  // Top
			4, 5, 7, 4, 7, 6,  // Bottom
			0, 4, 2, 2, 4, 6,  // Right
			0, 1, 4, 1, 5, 4   // Left
		};

		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;

		std::shared_ptr<SkyboxMaterial> m_Material;
		Mesh m_Mesh;
	};
}

