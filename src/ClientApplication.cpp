#include "ClientApplication.h"
#include <windowsx.h>

#include "DX12Engine/Resources/Shader.h"
#include "DX12Engine/IO/ModelLoader.h"
#include "DX12Engine/Resources/Mesh.h"
#include "DX12Engine/Entity/RenderComponent.h"
#include "DX12Engine/IO/TextureLoader.h"
#include "DX12Engine/Resources/Texture.h"
#include "DX12Engine/Rendering/GPUUploader.h"
#include "DX12Engine/Resources/Materials/BasicMaterial.h"
#include "DX12Engine/Resources/Materials/PBRMaterial.h"
#include "DX12Engine/Rendering/PipelineStateBuilder.h"
#include "DX12Engine/Rendering/RootSignatureBuilder.h"
#include "DX12Engine/Resources/Light.h"
#include "DX12Engine/Resources/RenderTexture.h"
#include "DX12Engine/Resources/ResourceManager.h"
#include "DX12Engine/Rendering/RenderPass/ShadowMapRenderPass.h"
#include "DX12Engine/Rendering/RenderPass/GeometryRenderPass.h"
#include "DX12Engine/Rendering/RenderPass/LightingRenderPass.h"
#include "DX12Engine/Rendering/RenderPipelineConfig.h"

ClientApplication::ClientApplication()
	: Application()
{
}

ClientApplication::~ClientApplication()
{
}

void ClientApplication::Init(std::shared_ptr<DX12Engine::RenderContext> renderContext, DirectX::XMFLOAT2 windowSize)
{
	m_RenderContext = renderContext;
	m_Renderer = std::make_unique<DX12Engine::Renderer>(m_RenderContext);

	DX12Engine::ModelLoader modelLoader;
	DX12Engine::Mesh mesh = modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("cube.obj"));
	DX12Engine::Mesh mesh2 = modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("sphere.obj"));
	DX12Engine::Mesh floorMesh = modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("floor.obj"));

	DX12Engine::TextureLoader textureLoader;
	DX12Engine::GPUUploader uploader = m_RenderContext->GetUploader();
	std::vector<DX12Engine::Texture*> textures;

	std::shared_ptr<DX12Engine::Texture> skyboxCube = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_cubemap.dds"));
	std::shared_ptr<DX12Engine::Texture> skyboxIrradiance = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_irradiance.dds"));
	textures = { skyboxCube.get(), skyboxIrradiance.get() };
	uploader.UploadTextureBatch(textures);

	auto brickTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("dark-worn-stone-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(brickTextures));
	auto goldTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("hammered-gold-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(goldTextures));
	auto concreteTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("clean-concrete-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(concreteTextures));
	auto wornMetalTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("worn-shiny-metal-ue"));
	uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(wornMetalTextures));

	std::shared_ptr<DX12Engine::PBRMaterial> pbrBrick = std::make_shared<DX12Engine::PBRMaterial>();
	pbrBrick->SetAllTextures(brickTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrGold = std::make_shared<DX12Engine::PBRMaterial>();
	pbrGold->SetAllTextures(goldTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrConcrete = std::make_shared<DX12Engine::PBRMaterial>();
	pbrConcrete->SetAllTextures(concreteTextures);
	std::shared_ptr<DX12Engine::PBRMaterial> pbrWornMetal = std::make_shared<DX12Engine::PBRMaterial>();
	pbrWornMetal->SetAllTextures(wornMetalTextures);

	std::shared_ptr<DX12Engine::GameObject> cube = std::make_shared<DX12Engine::GameObject>();
	DX12Engine::RenderComponent* cubeRenderComp = cube->CreateComponent<DX12Engine::RenderComponent>();
	cubeRenderComp->SetMesh(mesh);
	cubeRenderComp->SetMaterial(pbrBrick);
	cubeRenderComp->Move({ -1.5f, 0.0f, 0.0f });
	m_SceneObjects.Add(cube);
	std::shared_ptr<DX12Engine::GameObject> ball = std::make_shared<DX12Engine::GameObject>();
	DX12Engine::RenderComponent* ballRenderComp = ball->CreateComponent<DX12Engine::RenderComponent>();
	ballRenderComp->SetMesh(mesh2);
	ballRenderComp->SetMaterial(pbrGold);
	ballRenderComp->Move({ 1.5f, 0.0f, 0.0f });
	m_SceneObjects.Add(ball);
	std::shared_ptr<DX12Engine::GameObject> floor = std::make_shared<DX12Engine::GameObject>();
	DX12Engine::RenderComponent* floorRenderComp = floor->CreateComponent<DX12Engine::RenderComponent>();
	floorRenderComp->SetMesh(floorMesh);
	floorRenderComp->SetMaterial(pbrWornMetal);
	floorRenderComp->Move({ 0.0f, -1.0f, 0.0f });
	m_SceneObjects.Add(floor);

	m_LightBuffer = std::make_unique<DX12Engine::LightBuffer>();
	std::shared_ptr<DX12Engine::Light> sunLight = std::make_shared<DX12Engine::Light>();
	sunLight->SetType((int)DX12Engine::LightType::Directional);
	sunLight->SetDirection({ 0.45f, -0.577f, 0.577f });
	sunLight->SetIntensity(5.0f);
	sunLight->SetColor({ 1.0f, 0.85f, 0.8f });
	m_LightBuffer->AddLight(sunLight);
	std::shared_ptr<DX12Engine::Light> pointLight = std::make_shared<DX12Engine::Light>();
	pointLight->SetType((int)DX12Engine::LightType::Point);
	pointLight->SetPosition({ 0.0f, 1.0f, 1.0f });
	pointLight->SetIntensity(3.0f);
	pointLight->SetRange(10.0f);
	pointLight->SetColor({ 1.0f, 1.0f, 1.0f });
	//m_LightBuffer->AddLight(pointLight);
	std::shared_ptr<DX12Engine::Light> spotLight = std::make_shared<DX12Engine::Light>();
	spotLight->SetType((int)DX12Engine::LightType::Spot);
	spotLight->SetPosition({ 1.0f, 6.0f, -1.0f });
	spotLight->SetDirection({ -0.1f, -0.8f, 0.1f });
	spotLight->SetColor({ 0.9f, 0.5f, 0.0f });
	spotLight->SetIntensity(1.0f);
	spotLight->SetSpotAngle(45.0f);
	//m_LightBuffer->AddLight(spotLight);
	m_Renderer->SetLightBuffer(m_LightBuffer.get());

	m_Camera = std::make_unique<DX12Engine::Camera>(windowSize.x / windowSize.y, 1.0f, 100.0f);
	m_Camera->SetPosition({ 0.0f, 1.0f, -2.0f });
	m_Camera->SetRotation(-20.0f, 125.0f);
	m_Renderer->SetCamera(m_Camera.get());

	auto shadowCastingLights = m_LightBuffer->GetLightsByType({ DX12Engine::LightType::Directional, DX12Engine::LightType::Spot });
	auto cubeShadowCastingLights = m_LightBuffer->GetLightsByType({ DX12Engine::LightType::Point });

	std::vector<DX12Engine::RenderComponent*> renderComponents = m_SceneObjects.GetAllComponents<DX12Engine::RenderComponent>();

	DX12Engine::RenderPipelineConfig pipelineConfig;
	DX12Engine::RenderPassConfig shadowMapConfig;
	shadowMapConfig.Type = DX12Engine::RenderPassType::ShadowMap;
	shadowMapConfig.Count = 2;
	shadowMapConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &renderComponents;
	shadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &shadowCastingLights;
	std::vector<DX12Engine::RenderTargetType> shadowBufferTypes{
		DX12Engine::RenderTargetType::Depth
	};

	DX12Engine::RenderPassConfig cubeShadowMapConfig;
	cubeShadowMapConfig.Type = DX12Engine::RenderPassType::CubeShadowMap;
	cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &renderComponents;
	cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &cubeShadowCastingLights;
	std::vector<DX12Engine::RenderTargetType> cubeShadowBufferTypes{
		DX12Engine::RenderTargetType::Depth
	};

	DX12Engine::RenderPassConfig geometryConfig;
	geometryConfig.Type = DX12Engine::RenderPassType::Geometry;
	geometryConfig.InputResources[DX12Engine::InputResourceType::SceneObjects] = &renderComponents;
	std::vector<DX12Engine::RenderTargetType> gBufferTypes{
		DX12Engine::RenderTargetType::Albedo,
		DX12Engine::RenderTargetType::WorldNormal,
		DX12Engine::RenderTargetType::ObjectNormal,
		DX12Engine::RenderTargetType::Material,
		DX12Engine::RenderTargetType::Position,
		DX12Engine::RenderTargetType::Depth
	};

	std::vector<std::shared_ptr<DX12Engine::Texture>> lightingExternalTextures{ skyboxCube, skyboxIrradiance };
	DX12Engine::RenderPassConfig lightingConfig;
	lightingConfig.Type = DX12Engine::RenderPassType::Lighting;
	lightingConfig.InputResources[DX12Engine::InputResourceType::LightBuffer] = m_LightBuffer.get();
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
	m_RenderPipeline = m_Renderer->CreateRenderPipeline(pipelineConfig);
}

void ClientApplication::Update(float ts, float elapsed)
{
	m_Camera->ProcessKeyboardInput(0.01f);

	m_SceneObjects.Objects[0]->GetComponent<DX12Engine::RenderComponent>()->Rotate({0.0f, 1.0f, 0.0f});
	m_SceneObjects.Objects[1]->GetComponent<DX12Engine::RenderComponent>()->Rotate({ 1.0f, 0.0f, 0.0f });
	m_SceneObjects.Objects[1]->GetComponent<DX12Engine::RenderComponent>()->Move({ 0.0f, sin(elapsed) * ts, 0.0f });

	m_LightBuffer->Update();
	m_Renderer->UpdateObjectList(m_SceneObjects.Objects);

	m_Renderer->ExecutePipeline(m_RenderPipeline);
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
