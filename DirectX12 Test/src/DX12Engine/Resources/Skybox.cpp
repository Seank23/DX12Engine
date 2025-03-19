#include "Skybox.h"
#include "../Resources/ResourceManager.h"
#include "../IO/ModelLoader.h"

namespace DX12Engine
{
	Skybox::Skybox(std::shared_ptr<Texture> skyboxTexture)
		: RenderObject()
	{
		m_Mesh.Vertices = skyboxVertices;
		m_Mesh.Indices = skyboxIndices;
		SetMesh(m_Mesh);

		m_Material = std::make_shared<SkyboxMaterial>();
		m_Material->SetTexture(skyboxTexture);
		SetMaterial(m_Material);
	}

	Skybox::~Skybox()
	{
	}
}
