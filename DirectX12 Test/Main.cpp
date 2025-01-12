#include "Renderer.h"
#include "Shader.h"
#include "ModelLoader.h"
#include "Mesh.h"
#include "RenderObject.h"

int main() 
{
	DX12Engine::Renderer renderer(1600, 900);
	DX12Engine::Shader vertexShader("VertexShader.hlsl", "vertex");
	DX12Engine::Shader pixelShader("PixelShader.hlsl", "pixel");
	renderer.CreatePipeline(&vertexShader, &pixelShader);

	std::string inputfile = "E:\\Projects\\source\\repos\\DirectX12 Test\\cube.obj";
	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj(inputfile);

	DX12Engine::RenderObject cube1(renderer.GetDevice(), mesh);
	DX12Engine::RenderObject cube2(renderer.GetDevice(), mesh);
	cube1.SetModelMatrix(DirectX::XMMatrixTranslation(0.5f, 0.5f, 0.5f));
	cube2.SetModelMatrix(DirectX::XMMatrixTranslation(-0.5f, -0.5f, -0.5f));

	while (renderer.PollWindow())
	{
		renderer.InitFrame(renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
		renderer.UpdateCameraPosition(0.01f, 0.01f, 0.0f);
		renderer.Render(&cube1);
		renderer.Render(&cube2);
		renderer.PresentFrame();
	}
}