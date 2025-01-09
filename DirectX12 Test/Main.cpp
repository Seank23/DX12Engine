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

	std::string inputfile = "E:\\Projects\\source\\repos\\DirectX12 Test\\cube.obj";
	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj(inputfile);

	//std::vector<DX12Engine::Vertex> axisVertices = {
	//	// X-axis (Red)
	//	{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Origin
	//	{{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // X direction

	//	// Y-axis (Green)
	//	{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Origin
	//	{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Y direction

	//	// Z-axis (Blue)
	//	{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Origin
	//	{{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Z direction
	//};

	//std::vector<UINT> axisIndices = {
	//	0, 1, // X-axis
	//	2, 3, // Y-axis
	//	4, 5, // Z-axis
	//};

	DX12Engine::VertexBuffer vertexBuffer;
	vertexBuffer.SetData(renderer.GetDevice(), mesh.Vertices);
	DX12Engine::IndexBuffer indexBuffer;
	indexBuffer.SetData(renderer.GetDevice(), mesh.Indices);

	while (renderer.PollWindow())
	{
		renderer.UpdateCameraPosition(0.01f, 0.01f, 0.0f);
		renderer.Render(vertexBuffer.GetVertexBufferView(), indexBuffer.GetIndexBufferView(), renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
	}
}