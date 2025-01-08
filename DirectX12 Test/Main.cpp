#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h" 
#include "IndexBuffer.h"
#include "ModelLoader.h"

int main() 
{
	DX12Engine::Renderer renderer(1600, 900);
	DX12Engine::Shader vertexShader("VertexShader.hlsl", "vertex");
	DX12Engine::Shader pixelShader("PixelShader.hlsl", "pixel");
	renderer.CreatePipeline(&vertexShader, &pixelShader);

	std::string inputfile = "E:\\Projects\\source\\repos\\DirectX12 Test\\plane.obj";
	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj(inputfile);

    //std::vector<DX12Engine::Vertex> cubeVertices = {
    //    {{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}}, // Top-left

    //    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}}, // Top-left
    //                            
    //    {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // Top-left
    //                            
    //    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // Top-left
    //                            
    //    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}}, // Top-left
    //                            
    //    {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}}, // Bottom-left
    //    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}}, // Bottom-right
    //    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}}, // Top-right
    //    {{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}}, // Top-left
    //};

    //std::vector<UINT16> cubeIndices = {
    //    // Front face
    //    0, 1, 2, 0, 2, 3,
    //    // Back face
    //    4, 5, 6, 4, 6, 7,
    //    // Left face
    //    8, 9, 10, 8, 10, 11,
    //    // Right face
    //    12, 13, 14, 12, 14, 15,
    //    // Top face
    //    16, 17, 18, 16, 18, 19,
    //    // Bottom face
    //    20, 21, 22, 20, 22, 23
    //};

	DX12Engine::VertexBuffer vertexBuffer;
	vertexBuffer.SetData(renderer.GetDevice(), mesh.Vertices);
	DX12Engine::IndexBuffer indexBuffer;
	indexBuffer.SetData(renderer.GetDevice(), mesh.Indices);

	while (renderer.PollWindow())
	{
		renderer.Render(vertexBuffer.GetVertexBufferView(), indexBuffer.GetIndexBufferView(), renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
	}
}