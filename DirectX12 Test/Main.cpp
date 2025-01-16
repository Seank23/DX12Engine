#include "RenderContext.h"
#include "Renderer.h"
#include "Shader.h"
#include "ModelLoader.h"
#include "Mesh.h"
#include "RenderObject.h"

int main() 
{
	std::shared_ptr<DX12Engine::RenderContext> context = std::make_shared<DX12Engine::RenderContext>(1600, 900);

	DX12Engine::Renderer renderer(context);
	DX12Engine::Shader vertexShader("VertexShader.hlsl", "vertex");
	DX12Engine::Shader pixelShader("PixelShader.hlsl", "pixel");

	context->CreatePipeline(&vertexShader, &pixelShader);

	std::string inputfile = "E:\\Projects\\source\\repos\\DirectX12 Test\\cube.obj";
	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj(inputfile);

	DX12Engine::RenderObject cube1(mesh);
	DX12Engine::RenderObject cube2(mesh);
	cube1.SetModelMatrix(DirectX::XMMatrixTranslation(0.5f, 0.5f, 0.5f));
	cube2.SetModelMatrix(DirectX::XMMatrixTranslation(0.0f, -0.5f, -0.5f));
	float count = 0.0f;

	while (renderer.PollWindow())
	{
		renderer.InitFrame(renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
		renderer.UpdateCameraPosition(0.005f, 0.005f, 0.0f);
		cube1.SetModelMatrix(DirectX::XMMatrixTranslation(-2.0f * DirectX::XMScalarSin(count), 0.0f, 0.0f));
		cube2.SetModelMatrix(DirectX::XMMatrixTranslation(2.0f * DirectX::XMScalarSin(count), -0.5f, -0.5f));
		renderer.Render(&cube1);
		renderer.Render(&cube2);
		renderer.PresentFrame();
		count += 0.01f;
	}
}