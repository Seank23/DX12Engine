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
			{{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{ {1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{ {1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{ {1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{ {1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
			{{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
		};

		std::vector<UINT> skyboxIndices = 
		{
			0, 1, 2, 2, 3, 0,  // Back
			4, 5, 6, 6, 7, 4,  // Front
			4, 5, 1, 1, 0, 4,  // Top
			7, 6, 2, 2, 3, 7,  // Bottom
			5, 1, 2, 2, 6, 5,  // Right
			4, 0, 3, 3, 7, 4   // Left
		};

		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;

		std::shared_ptr<SkyboxMaterial> m_Material;
		Mesh m_Mesh;
	};
}

