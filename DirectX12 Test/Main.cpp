#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h" 

int main() 
{
	DX12Engine::Renderer renderer(1600, 900);
	DX12Engine::Shader vertexShader("VertexShader.hlsl", "vertex");
	DX12Engine::Shader pixelShader("PixelShader.hlsl", "pixel");
	renderer.CreatePipeline(&vertexShader, &pixelShader);

	std::vector<DX12Engine::Vertex> triangleVertices =
	{
		{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // Top vertex (Red)
		{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // Right vertex (Green)
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // Left vertex (Blue)
	};

	DX12Engine::VertexBuffer vertexBuffer;
	vertexBuffer.SetVertices(renderer.GetDevice(), triangleVertices);

	while (renderer.PollWindow())
	{
		renderer.Render(vertexBuffer.GetVertexBufferView(), renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
	}
}