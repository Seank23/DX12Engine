#include "DX12Engine/Rendering/RenderContext.h"
#include "DX12Engine/Rendering/Renderer.h"
#include "DX12Engine/Resources/Shader.h"
#include "DX12Engine/IO/ModelLoader.h"
#include "DX12Engine/Resources/Mesh.h"
#include "DX12Engine/Rendering/RenderObject.h"
#include "DX12Engine/IO/TextureLoader.h"
#include "DX12Engine/Resources/Texture.h"
#include "DX12Engine/Rendering/GPUUploader.h"
#include "DX12Engine/Resources/Materials/BasicMaterial.h"
#include "DX12Engine/Resources/Materials/PBRMaterial.h"
#include "DX12Engine/Rendering/PipelineStateBuilder.h"
#include "DX12Engine/Rendering/RootSignatureBuilder.h"
#include "DX12Engine/Buffers/LightBuffer.h"

int main() 
{
	std::shared_ptr<DX12Engine::RenderContext> context = std::make_shared<DX12Engine::RenderContext>(1600, 900);

	DX12Engine::Renderer renderer(context);

	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\cube2.obj");
	DX12Engine::Mesh mesh2 = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\cylinder.obj");

	DX12Engine::TextureLoader textureLoader;
	std::shared_ptr<DX12Engine::Texture> textureMC = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\minecraft_block_uv.png");
	std::shared_ptr<DX12Engine::Texture> textureWall = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\TCom_Wall_Stone3_2x2_512_albedo.tiff");

	DX12Engine::GPUUploader uploader = context->GetUploader();
	std::vector<DX12Engine::Texture*> textures = { textureMC.get(), textureWall.get() };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::Texture> stoneAlbedo = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_albedo.png");
	std::shared_ptr<DX12Engine::Texture> stoneNormal = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_normal-dx.png");
	std::shared_ptr<DX12Engine::Texture> stoneMetallic = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_metallic.png");
	std::shared_ptr<DX12Engine::Texture> stoneRoughness = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_roughness.png");
	std::shared_ptr<DX12Engine::Texture> stoneAO = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_ao.png");
	textures = { stoneAlbedo.get(), stoneNormal.get(), stoneMetallic.get(), stoneRoughness.get(), stoneAO.get() };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::BasicMaterial> basicMaterial = std::make_shared<DX12Engine::BasicMaterial>();
	basicMaterial->SetTexture(stoneAlbedo);


	std::shared_ptr<DX12Engine::PBRMaterial> pbrMaterial = std::make_shared<DX12Engine::PBRMaterial>();
	pbrMaterial->SetAlbedoMap(stoneAlbedo);
	pbrMaterial->SetNormalMap(stoneNormal);
	pbrMaterial->SetMetallicMap(stoneMetallic);
	pbrMaterial->SetRoughnessMap(stoneRoughness);
	pbrMaterial->SetAOMap(stoneAO);

	DX12Engine::RenderObject cube1(mesh);
	DX12Engine::RenderObject cube2(mesh);
	cube1.SetMaterial(pbrMaterial);
	cube2.SetMaterial(basicMaterial);
	cube1.SetModelMatrix(DirectX::XMMatrixTranslation(-1.5f, 0.0f, 0.0f));
	cube2.SetModelMatrix(DirectX::XMMatrixTranslation(1.5f, 0.0f, 0.0f));
	float count = 0.0f;

	DX12Engine::LightBuffer lightBuffer;
	DX12Engine::Light pointLight;
	pointLight.Type = (int)DX12Engine::LightType::Point;
	pointLight.Position = { 0.0f, 2.0f, 0.0f };
	pointLight.Intensity = 8.0f;
	pointLight.Range = 10.0f;
	pointLight.Color = { 1.0f, 1.0f, 1.0f };
	lightBuffer.AddLight(pointLight);
	DX12Engine::Light spotLight1;
	spotLight1.Type = (int)DX12Engine::LightType::Spot;
	spotLight1.Position = { -2.0f, 2.5f, 2.0f };
	spotLight1.Intensity = 1.0f;
	spotLight1.Color = { 0.0f, 0.0f, 1.0f };
	spotLight1.SpotAngle = cos(DirectX::XMConvertToRadians(-60.0f));
	lightBuffer.AddLight(spotLight1);
	DX12Engine::Light spotLight2;
	spotLight2.Type = (int)DX12Engine::LightType::Spot;
	spotLight2.Position = { 2.0f, 2.5f, -2.0f };
	spotLight2.Intensity = 1.0f;
	spotLight2.Color = { 1.0f, 0.0f, 0.0f };
	spotLight2.SpotAngle = cos(DirectX::XMConvertToRadians(60.0f));
	lightBuffer.AddLight(spotLight2);
	renderer.SetLightBuffer(&lightBuffer);

	while (renderer.PollWindow())
	{
		renderer.InitFrame(renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
		renderer.UpdateCameraPosition(0.002f, 0.002f, 0.0f);
		//lightBuffer.GetLight(1)->SpotAngle = cos(DirectX::XMConvertToRadians(60.0f + count * 10));
		//lightBuffer.GetLight(2)->SpotAngle = sin(DirectX::XMConvertToRadians(60.0f + count * 10));
		lightBuffer.GetLight(0)->Position = { 2.0f * DirectX::XMScalarSin(count), 2.0f, 2.0f * DirectX::XMScalarCos(count) };
		lightBuffer.Update();
		//cube1.SetModelMatrix(DirectX::XMMatrixTranslation(-2.0f * DirectX::XMScalarSin(count), 0.0f, 0.0f));
		//cube2.SetModelMatrix(DirectX::XMMatrixTranslation(2.0f * DirectX::XMScalarSin(count), -0.5f, -0.5f));
		renderer.Render(&cube1);
		renderer.Render(&cube2);
		renderer.PresentFrame();
		count += 0.01f;
	}
}