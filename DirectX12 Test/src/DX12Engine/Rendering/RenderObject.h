#pragma once
#include "RenderContext.h"
#include "../Resources/Mesh.h"
#include "../Buffers/VertexBuffer.h"
#include "../Buffers/IndexBuffer.h"
#include "../Buffers/ConstantBuffer.h"
#include "../Buffers/ConstantBufferData.h"
#include "../Resources/Texture.h"

namespace DX12Engine
{
	class RenderObject
	{
	public:
		friend class Renderer;

		RenderObject(Mesh mesh);
		~RenderObject();

		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	
		void SetTexture(Texture* texture) { m_Texture = texture; }

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix);

		Mesh m_Mesh;
		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		ConstantBufferData m_ConstantBufferData;
		DirectX::XMMATRIX m_ModelMatrix;
		Texture* m_Texture;
	};
}