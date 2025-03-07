#include "DX12Engine/Rendering/RenderContext.h"
#include "DX12Engine/Rendering/Renderer.h"
#include "DX12Engine/Resources/Shader.h"
#include "DX12Engine/IO/ModelLoader.h"
#include "DX12Engine/Resources/Mesh.h"
#include "DX12Engine/Rendering/RenderObject.h"
#include "DX12Engine/IO/TextureLoader.h"
#include "DX12Engine/Resources/Texture.h"
#include "DX12Engine/Rendering/GPUUploader.h"
#include "DX12Engine/Resources/Material.h"
#include "DX12Engine/Rendering/PipelineStateBuilder.h"
#include "DX12Engine/Rendering/RootSignatureBuilder.h"
#include "DX12Engine/Buffers/LightBuffer.h"

int main() 
{
	std::shared_ptr<DX12Engine::RenderContext> context = std::make_shared<DX12Engine::RenderContext>(1600, 900);

	DX12Engine::Renderer renderer(context);
	DX12Engine::Shader vertexShader("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\VertexShader.hlsl", "vertex");
	DX12Engine::Shader pixelShader("E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\PixelShader.hlsl", "pixel");

	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\cube2.obj");
	DX12Engine::Mesh mesh2 = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\cylinder.obj");

	DX12Engine::TextureLoader textureLoader;
	std::shared_ptr<DX12Engine::Texture> textureMC = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\minecraft_block_uv.png");
	std::shared_ptr<DX12Engine::Texture> textureWall = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\TCom_Wall_Stone3_2x2_512_albedo.tiff");

	DX12Engine::GPUUploader uploader = context->GetUploader();
	std::vector<DX12Engine::Texture*> textures = { textureMC.get(), textureWall.get() };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::Material> material1 = std::make_shared<DX12Engine::Material>();
	material1->ConfigureFromDefault(&vertexShader, &pixelShader);
	//material1->SetColor({ 0.1f, 0.2f, 1.0f, 1.0f });
	material1->SetTexture(textureMC);

	std::shared_ptr<DX12Engine::Material> material2 = std::make_shared<DX12Engine::Material>();
	material2->ConfigureFromDefault(&vertexShader, &pixelShader);
	//material2->SetColor({ 1.0f, 0.2f, 0.1f, 1.0f });
	material2->SetTexture(textureWall);

	DX12Engine::RenderObject cube1(mesh);
	DX12Engine::RenderObject cube2(mesh2);
	cube1.SetMaterial(material1);
	cube2.SetMaterial(material2);
	cube1.SetModelMatrix(DirectX::XMMatrixTranslation(1.0f, -1.0f, 0.0f));
	cube2.SetModelMatrix(DirectX::XMMatrixTranslation(-1.0f, 1.0f, 0.0f));
	float count = 0.0f;

	DX12Engine::LightBuffer lightBuffer;
	DX12Engine::Light pointLight;
	pointLight.Type = (int)DX12Engine::LightType::Point;
	pointLight.Position = { 0.0f, 5.0f, -5.0f };
	pointLight.Intensity = 0.2f;
	pointLight.Range = 200.0f;
	pointLight.Color = { 1.0f, 1.0f, 1.0f };
	lightBuffer.AddLight(pointLight);
	DX12Engine::Light spotLight1;
	spotLight1.Type = (int)DX12Engine::LightType::Spot;
	spotLight1.Position = { -2.0f, 2.5f, 2.0f };
	spotLight1.Intensity = 5.0f;
	spotLight1.Color = { 1.0f, 0.0f, 0.0f };
	spotLight1.SpotAngle = cos(DirectX::XMConvertToRadians(-60.0f));
	lightBuffer.AddLight(spotLight1);
	DX12Engine::Light spotLight2;
	spotLight2.Type = (int)DX12Engine::LightType::Spot;
	spotLight2.Position = { 2.0f, 2.5f, -2.0f };
	spotLight2.Intensity = 5.0f;
	spotLight2.Color = { 0.0f, 0.0f, 1.0f };
	spotLight2.SpotAngle = cos(DirectX::XMConvertToRadians(60.0f));
	lightBuffer.AddLight(spotLight2);
	renderer.SetLightBuffer(&lightBuffer);

	while (renderer.PollWindow())
	{
		renderer.InitFrame(renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
		renderer.UpdateCameraPosition(0.005f, 0.005f, 0.0f);
		lightBuffer.GetLight(1)->SpotAngle = cos(DirectX::XMConvertToRadians(60.0f + count * 10));
		lightBuffer.GetLight(2)->SpotAngle = sin(DirectX::XMConvertToRadians(60.0f + count * 10));
		lightBuffer.Update();
		cube1.SetModelMatrix(DirectX::XMMatrixTranslation(-2.0f * DirectX::XMScalarSin(count), 0.0f, 0.0f));
		cube2.SetModelMatrix(DirectX::XMMatrixTranslation(2.0f * DirectX::XMScalarSin(count), -0.5f, -0.5f));
		renderer.Render(&cube1);
		renderer.Render(&cube2);
		renderer.PresentFrame();
		count += 0.01f;
	}
}