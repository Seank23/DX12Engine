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
#include "DX12Engine/Rendering/PipelineStateBuilder.h"
#include "DX12Engine/Rendering/RootSignatureBuilder.h"
#include "DX12Engine/Buffers/LightBuffer.h"
#include "DX12Engine/Resources/Light.h"
#include "DX12Engine/Resources/RenderTexture.h"
#include "DX12Engine/Resources/ResourceManager.h"
#include "DX12Engine/Rendering/RenderPass/ShadowMapRenderPass.h"
#include "DX12Engine/Rendering/RenderPass/GeometryRenderPass.h"
#include "DX12Engine/Rendering/RenderPass/LightingRenderPass.h"
#include "DX12Engine/Rendering/RenderPipelineConfig.h"

ClientApplication::ClientApplication()
{
	DirectX::XMFLOAT2 windowSize = { 1920, 1080 };
	std::shared_ptr<DX12Engine::RenderContext> context = std::make_shared<DX12Engine::RenderContext>(this, windowSize.x, windowSize.y);

	DX12Engine::Renderer renderer(context);

	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj("../../cube2.obj");
	DX12Engine::Mesh mesh2 = modelLoader.LoadObj("../../sphere.obj");
	DX12Engine::Mesh floorMesh = modelLoader.LoadObj("../../floor.obj");

	DX12Engine::TextureLoader textureLoader;
	DX12Engine::GPUUploader uploader = context->GetUploader();
	std::vector<DX12Engine::Texture*> textures;

	std::shared_ptr<DX12Engine::Texture> skyboxCube = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetResourcePath("skybox/skybox_cubemap.dds"));
	std::shared_ptr<DX12Engine::Texture> skyboxIrradiance = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetResourcePath("skybox/skybox_irradiance.dds"));
	textures = { skyboxCube.get(), skyboxIrradiance.get() };
	uploader.UploadTextureBatch(textures);

	auto brickTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetResourcePath("dark-worn-stone-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(brickTextures));
	auto goldTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetResourcePath("hammered-gold-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(goldTextures));
	auto concreteTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetResourcePath("clean-concrete-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(concreteTextures));
	auto wornMetalTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetResourcePath("worn-shiny-metal-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(wornMetalTextures));

	std::shared_ptr<DX12Engine::PBRMaterial> pbrBrick = std::make_shared<DX12Engine::PBRMaterial>();
	pbrBrick->SetAllTextures(brickTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrGold = std::make_shared<DX12Engine::PBRMaterial>();
	pbrGold->SetAllTextures(goldTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrConcrete = std::make_shared<DX12Engine::PBRMaterial>();
	pbrConcrete->SetAllTextures(concreteTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrWornMetal = std::make_shared<DX12Engine::PBRMaterial>();
	pbrWornMetal->SetAllTextures(wornMetalTextures);

	DX12Engine::RenderObject object1(mesh);
	DX12Engine::RenderObject object2(mesh2);
	DX12Engine::RenderObject floor(floorMesh);
	DX12Engine::RenderObject wallBack(floorMesh);
	object1.SetMaterial(pbrBrick);
	object2.SetMaterial(pbrGold);
	floor.SetMaterial(pbrWornMetal);
	wallBack.SetMaterial(pbrWornMetal);
	object1.Move({ -1.5f, 0.0f, 0.0f });
	object2.Move({ 1.5f, 0.0f, 0.0f });
	floor.Move({ 0.0f, -1.0f, 0.0f });
	wallBack.Move({ 0.0f, 4.0f, 5.0f });
	wallBack.Rotate({ -90.0f, 0.0f, 0.0f });
	float count = 0.0f;

	DX12Engine::LightBuffer lightBuffer;
	DX12Engine::Light sunLight;
	sunLight.SetType((int)DX12Engine::LightType::Directional);
	sunLight.SetDirection({ 0.45f, -0.577f, 0.577f });
	sunLight.SetIntensity(5.0f);
	sunLight.SetColor({ 1.0f, 0.85f, 0.8f });
	lightBuffer.AddLight(&sunLight);
	DX12Engine::Light pointLight;
	pointLight.SetType((int)DX12Engine::LightType::Point);
	pointLight.SetPosition({ 0.0f, 1.0f, 1.0f });
	pointLight.SetIntensity(3.0f);
	pointLight.SetRange(10.0f);
	pointLight.SetColor({ 1.0f, 1.0f, 1.0f });
	//lightBuffer.AddLight(&pointLight);
	DX12Engine::Light spotLight;
	spotLight.SetType((int)DX12Engine::LightType::Spot);
	spotLight.SetPosition({ 1.0f, 6.0f, -1.0f });
	spotLight.SetDirection({ -0.1f, -0.8f, 0.1f });
	spotLight.SetColor({ 0.9f, 0.5f, 0.0f });
	spotLight.SetIntensity(1.0f);
	spotLight.SetSpotAngle(45.0f);
	//lightBuffer.AddLight(&spotLight);
	renderer.SetLightBuffer(&lightBuffer);

	m_Camera = std::make_unique<DX12Engine::Camera>(windowSize.x / windowSize.y, 1.0f, 100.0f);
	m_Camera->SetPosition({ 0.0f, 1.0f, -2.0f });
	m_Camera->SetRotation(-20.0f, 125.0f);
	renderer.SetCamera(m_Camera.get());

	std::vector<DX12Engine::RenderObject*> sceneObjects{ &object1, &object2, &floor };

	auto shadowCastingLights = lightBuffer.GetLightsByType({ DX12Engine::LightType::Directional, DX12Engine::LightType::Spot });
	auto cubeShadowCastingLights = lightBuffer.GetLightsByType({ DX12Engine::LightType::Point });

	DX12Engine::RenderPipelineConfig pipelineConfig;
	DX12Engine::RenderPassConfig shadowMapConfig;
	shadowMapConfig.Type = DX12Engine::RenderPassType::ShadowMap;
	shadowMapConfig.Count = 2;
	shadowMapConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &sceneObjects;
	shadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &shadowCastingLights;
	std::vector<DX12Engine::RenderTargetType> shadowBufferTypes{
		DX12Engine::RenderTargetType::Depth
	};

	DX12Engine::RenderPassConfig cubeShadowMapConfig;
	cubeShadowMapConfig.Type = DX12Engine::RenderPassType::CubeShadowMap;
	cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &sceneObjects;
	cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &cubeShadowCastingLights;
	std::vector<DX12Engine::RenderTargetType> cubeShadowBufferTypes{
		DX12Engine::RenderTargetType::Depth
	};

	DX12Engine::RenderPassConfig geometryConfig;
	geometryConfig.Type = DX12Engine::RenderPassType::Geometry;
	geometryConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &sceneObjects;
	std::vector<DX12Engine::RenderTargetType> gBufferTypes{
		DX12Engine::RenderTargetType::Albedo,
		DX12Engine::RenderTargetType::WorldNormal,
		DX12Engine::RenderTargetType::ObjectNormal,
		DX12Engine::RenderTargetType::Material,
		DX12Engine::RenderTargetType::Position,
		DX12Engine::RenderTargetType::Depth
	};

	std::vector<DX12Engine::Texture*> lightingExternalTextures{ skyboxCube.get(), skyboxIrradiance.get() };
	DX12Engine::RenderPassConfig lightingConfig;
	lightingConfig.Type = DX12Engine::RenderPassType::Lighting;
	lightingConfig.InputResources[DX12Engine::InputResourceType::LightBuffer] = &lightBuffer;
	lightingConfig.InputResources[DX12Engine::InputResourceType::Camera] = m_Camera.get();
	lightingConfig.InputResources[DX12Engine::InputResourceType::ExternalTextures] = &lightingExternalTextures;
	lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Geometry] = &gBufferTypes;
	lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_ShadowMap] = &shadowBufferTypes;
	lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_CubeShadowMap] = &cubeShadowBufferTypes;

	std::vector<DX12Engine::RenderTargetType> ssrGBufferTypes{ DX12Engine::RenderTargetType::Albedo, DX12Engine::RenderTargetType::WorldNormal, DX12Engine::RenderTargetType::Material, DX12Engine::RenderTargetType::Position, DX12Engine::RenderTargetType::Depth };
	std::vector<DX12Engine::RenderTargetType> ssrLightingTypes{ DX12Engine::RenderTargetType::Composite };
	DX12Engine::RenderPassConfig ssrConfig;
	ssrConfig.Type = DX12Engine::RenderPassType::ScreenSpaceReflection;
	ssrConfig.InputResources[DX12Engine::InputResourceType::Camera] = m_Camera.get();
	ssrConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Geometry] = &ssrGBufferTypes;
	ssrConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Lighting] = &ssrLightingTypes;

	pipelineConfig.Passes.push_back(shadowMapConfig);
	pipelineConfig.Passes.push_back(cubeShadowMapConfig);
	pipelineConfig.Passes.push_back(geometryConfig);
	pipelineConfig.Passes.push_back(lightingConfig);
	pipelineConfig.Passes.push_back(ssrConfig);
	DX12Engine::RenderPipeline pipeline = renderer.CreateRenderPipeline(pipelineConfig);

	while (renderer.PollWindow())
	{
		m_Camera->ProcessKeyboardInput(0.01f);

		object1.Rotate({ 0.0f, 1.0f, 0.0f });
		object2.Rotate({ 1.0f, 0.0f, 0.0f });
		object2.Move({ 0.0f, sin(count) / 200, 0.0f});

		lightBuffer.Update();
		renderer.UpdateObjectList(sceneObjects);

		renderer.ExecutePipeline(pipeline);

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
