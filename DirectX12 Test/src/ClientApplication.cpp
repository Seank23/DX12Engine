#include "ClientApplication.h"
#include <windowsx.h>

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
#include "DX12Engine/Resources/Materials/SkyboxMaterial.h"
#include "DX12Engine/Rendering/PipelineStateBuilder.h"
#include "DX12Engine/Rendering/RootSignatureBuilder.h"
#include "DX12Engine/Buffers/LightBuffer.h"
#include "DX12Engine/Resources/Light.h"
#include "DX12Engine/Resources/Skybox.h"
//#include "DX12Engine/Rendering/ProceduralRenderer.h"
#include "DX12Engine/Resources/DepthMap.h"

ClientApplication::ClientApplication()
{
	std::shared_ptr<DX12Engine::RenderContext> context = std::make_shared<DX12Engine::RenderContext>(this, 1920, 1080);

	DX12Engine::Renderer renderer(context);

	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\cube2.obj");
	DX12Engine::Mesh mesh2 = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\sphere.obj");
	DX12Engine::Mesh floorMesh = modelLoader.LoadObj("E:\\Projects\\source\\repos\\DirectX12 Test\\floor.obj");

	DX12Engine::TextureLoader textureLoader;
	DX12Engine::GPUUploader uploader = context->GetUploader();
	std::vector<DX12Engine::Texture*> textures;

	std::shared_ptr<DX12Engine::Texture> skyboxCube = textureLoader.LoadCubemapDDS(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\skybox\\skybox_cubemap.dds");
	std::shared_ptr<DX12Engine::Texture> skyboxIrradiance = textureLoader.LoadCubemapDDS(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\skybox\\skybox_irradiance.dds");
	textures = { skyboxCube.get(), skyboxIrradiance.get() };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::Texture> stoneAlbedo = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_albedo.png");
	std::shared_ptr<DX12Engine::Texture> stoneNormal = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_normal-dx.png");
	std::shared_ptr<DX12Engine::Texture> stoneMetallic = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_metallic.png");
	std::shared_ptr<DX12Engine::Texture> stoneRoughness = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_roughness.png");
	std::shared_ptr<DX12Engine::Texture> stoneAO = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\dark-worn-stone-ue\\dark-worn-stonework_ao.png");
	textures = { stoneAlbedo.get(), stoneNormal.get(), stoneMetallic.get(), stoneRoughness.get(), stoneAO.get() };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::Texture> goldAlbedo = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\hammered-gold-ue\\hammered-gold_albedo.png");
	std::shared_ptr<DX12Engine::Texture> goldNormal = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\hammered-gold-ue\\hammered-gold_normal-dx.png");
	std::shared_ptr<DX12Engine::Texture> goldMetallic = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\hammered-gold-ue\\hammered-gold_metallic.png");
	std::shared_ptr<DX12Engine::Texture> goldRoughness = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\hammered-gold-ue\\hammered-gold_roughness.png");
	std::shared_ptr<DX12Engine::Texture> goldAO = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\hammered-gold-ue\\hammered-gold_ao.png");
	textures = { goldAlbedo.get(), goldNormal.get(), goldMetallic.get(), goldRoughness.get(), goldAO.get(), };
	uploader.UploadTextureBatch(textures);

	std::shared_ptr<DX12Engine::Texture> concreteAlbedo = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\clean-concrete-ue\\clean-concrete_albedo.png");
	std::shared_ptr<DX12Engine::Texture> concreteNormal = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\clean-concrete-ue\\clean-concrete_normal-dx.png");
	std::shared_ptr<DX12Engine::Texture> concreteMetallic = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\clean-concrete-ue\\clean-concrete_metallic.png");
	std::shared_ptr<DX12Engine::Texture> concreteRoughness = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\clean-concrete-ue\\clean-concrete_roughness.png");
	std::shared_ptr<DX12Engine::Texture> concreteAO = textureLoader.LoadWIC(L"E:\\Projects\\source\\repos\\DirectX12 Test\\DirectX12 Test\\res\\clean-concrete-ue\\clean-concrete_ao.png");
	textures = { concreteAlbedo.get(), concreteNormal.get(), concreteMetallic.get(), concreteRoughness.get(), concreteAO.get(), };
	uploader.UploadTextureBatch(textures);

	DX12Engine::Skybox skybox(skyboxCube);
	renderer.SetSkybox(&skybox);

	std::shared_ptr<DX12Engine::BasicMaterial> basicMaterial = std::make_shared<DX12Engine::BasicMaterial>();
	basicMaterial->SetTexture(goldAlbedo);

	std::shared_ptr<DX12Engine::PBRMaterial> pbrStone = std::make_shared<DX12Engine::PBRMaterial>();
	pbrStone->SetAlbedoMap(stoneAlbedo);
	pbrStone->SetNormalMap(stoneNormal);
	pbrStone->SetMetallicMap(stoneMetallic);
	pbrStone->SetRoughnessMap(stoneRoughness);
	pbrStone->SetAOMap(stoneAO);

	std::shared_ptr<DX12Engine::PBRMaterial> pbrGold = std::make_shared<DX12Engine::PBRMaterial>();
	pbrGold->SetAlbedoMap(goldAlbedo);
	pbrGold->SetNormalMap(goldNormal);
	pbrGold->SetMetallicMap(goldMetallic);
	pbrGold->SetRoughnessMap(goldRoughness);
	pbrGold->SetAOMap(goldAO);

	std::shared_ptr<DX12Engine::PBRMaterial> pbrConcrete = std::make_shared<DX12Engine::PBRMaterial>();
	pbrConcrete->SetAlbedoMap(concreteAlbedo);
	pbrConcrete->SetNormalMap(concreteNormal);
	pbrConcrete->SetMetallicMap(concreteMetallic);
	pbrConcrete->SetRoughnessMap(concreteRoughness);
	pbrConcrete->SetAOMap(concreteAO);

	DX12Engine::RenderObject object1(mesh);
	DX12Engine::RenderObject object2(mesh2);
	DX12Engine::RenderObject floor(floorMesh);
	object1.SetMaterial(pbrStone);
	object2.SetMaterial(pbrGold);
	floor.SetMaterial(pbrConcrete);
	object1.SetModelMatrix(DirectX::XMMatrixTranslation(-1.5f, 0.0f, 0.0f));
	object2.SetModelMatrix(DirectX::XMMatrixTranslation(1.5f, 0.0f, 0.0f));
	floor.SetModelMatrix(DirectX::XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	float count = 0.0f;

	DX12Engine::LightBuffer lightBuffer;
	DX12Engine::Light sunLight;
	sunLight.SetType((int)DX12Engine::LightType::Directional);
	sunLight.SetDirection({ 0.45f, -0.577f, 0.577f });
	sunLight.SetIntensity(3.0f);
	sunLight.SetColor({ 1.0f, 0.85f, 0.8f });
	lightBuffer.AddLight(&sunLight);
	DX12Engine::Light pointLight;
	pointLight.SetType((int)DX12Engine::LightType::Point);
	pointLight.SetPosition({ 0.0f, 2.0f, 1.0f });
	pointLight.SetIntensity(10.0f);
	pointLight.SetRange(3.0f);
	pointLight.SetColor({ 1.0f, 1.0f, 1.0f });
	//lightBuffer.AddLight(&pointLight);
	DX12Engine::Light spotLight;
	spotLight.SetType((int)DX12Engine::LightType::Spot);
	spotLight.SetPosition({ 1.0f, 6.0f, -1.0f });
	spotLight.SetDirection({ -0.1f, -0.8f, 0.1f });
	spotLight.SetColor({ 0.9f, 0.5f, 0.0f });
	spotLight.SetIntensity(4.0f);
	spotLight.SetSpotAngle(45.0f);
	lightBuffer.AddLight(&spotLight);
	renderer.SetLightBuffer(&lightBuffer);

	m_Camera = std::make_unique<DX12Engine::Camera>(1600.0f / 900.0f, 0.001f, 100.0f);
	m_Camera->SetPosition({ 4.0f, 2.0f, -5.0f });
	m_Camera->SetRotation(-20.0, 125.0);
	renderer.SetCamera(m_Camera.get());

	DX12Engine::ProceduralRenderer proceduralRenderer = context->GetProceduralRenderer();
	std::unique_ptr<DX12Engine::DepthMap> shadowMap = proceduralRenderer.CreateShadowMapResource(2);
	std::unique_ptr<DX12Engine::DepthMap> shadowCubeMap = proceduralRenderer.CreateShadowCubeMapResource(1);

	std::vector<DX12Engine::RenderObject*> shadowCastingObjects{ &object1, &object2, &floor };

	while (renderer.PollWindow())
	{
		m_Camera->ProcessKeyboardInput(0.01f);

		proceduralRenderer.RenderShadowMaps(shadowMap.get(), lightBuffer.GetLightsByType({ DX12Engine::LightType::Directional, DX12Engine::LightType::Spot }), shadowCastingObjects);
		proceduralRenderer.RenderShadowCubeMaps(shadowCubeMap.get(), lightBuffer.GetLightsByType({ DX12Engine::LightType::Point }), shadowCastingObjects);
		renderer.InitFrame(renderer.GetDefaultViewport(), renderer.GetDefaultScissorRect());
		//lightBuffer.GetLight(0)->SetPosition({ -5.0f + count, 5.5f, -5.0f });
		lightBuffer.Update();
		renderer.SetShadowMap(shadowMap.get());
		renderer.SetShadowCubeMap(shadowCubeMap.get());
		//object1.SetModelMatrix(DirectX::XMMatrixTranslation(2.0f * DirectX::XMScalarCos(count), 1.0f + DirectX::XMScalarSin(count), 1.0f));
		//object2.SetModelMatrix(DirectX::XMMatrixTranslation(2.0f * DirectX::XMScalarSin(count), 1.0f + DirectX::XMScalarCos(count), -1.0f));
		renderer.Render(&object1);
		renderer.Render(&object2);
		renderer.Render(&floor);
		renderer.PresentFrame();
		count += 0.01f;
	}
}

ClientApplication::~ClientApplication()
{
}

void ClientApplication::HandleMouseMovement(HWND hwnd, LPARAM lParam)
{
	int mouseX = GET_X_LPARAM(lParam);
	int mouseY = GET_Y_LPARAM(lParam);

	if (m_FirstMouse) 
	{
		m_LastMouseX = (float)mouseX;
		m_LastMouseY = (float)mouseY;
		m_FirstMouse = false;
	}

	float deltaX = (float)(mouseX - m_LastMouseX);
	float deltaY = (float)(mouseY - m_LastMouseY);

	m_LastMouseX = (float)mouseX;
	m_LastMouseY = (float)mouseY;

	m_Camera->ProcessMouseInput(deltaX, deltaY);
}
