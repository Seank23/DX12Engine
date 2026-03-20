#include "DemoScene.h"

#include "DX12Engine/Entity/RenderComponent.h"
#include "DX12Engine/IO/ModelLoader.h"
#include "DX12Engine/IO/TextureLoader.h"
#include "DX12Engine/Rendering/GPUUploader.h"
#include "DX12Engine/Resources/Light.h"
#include "DX12Engine/Resources/Materials/PBRMaterial.h"
#include "DX12Engine/Resources/ResourceManager.h"
#include "DX12Engine/Entity/PhysicsComponent.h"
#include "DX12Engine/Physics/PhysicsEngine.h"

namespace DX12EngineDemo
{
	DemoScene::DemoScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize)
		: m_RenderContext(renderContext), m_PhysicsEngine(physicsEngine), m_WindowSize(windowSize)
	{
	}

	void DemoScene::Init()
	{
		m_Camera = std::make_unique<DX12Engine::Camera>(m_WindowSize.x / m_WindowSize.y, 1.0f, 100.0f);
		m_Camera->SetPosition({ 5.0f, 1.0f, -10.0f });
		m_Camera->SetRotation(5.0f, 115.0f);
		m_Camera->SetSpeed(5.0f);

		m_LightBuffer = std::make_unique<DX12Engine::LightBuffer>();
		std::shared_ptr<DX12Engine::Light> sunLight = std::make_shared<DX12Engine::Light>();
		sunLight->SetType((int)DX12Engine::LightType::Directional);
		sunLight->SetDirection({ 0.45f, -0.577f, 0.577f });
		sunLight->SetIntensity(5.0f);
		sunLight->SetColor({ 1.0f, 0.85f, 0.8f });
		m_LightBuffer->AddLight(sunLight);

		DX12Engine::TextureLoader textureLoader;
		DX12Engine::GPUUploader uploader = m_RenderContext->GetUploader();

		m_SkyboxCubemap = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox2_cubemap.dds"));
		m_SkyboxIrradiance = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox2_irradiance.dds"));
		uploader.UploadTextureBatch({ m_SkyboxCubemap.get(), m_SkyboxIrradiance.get() });

		auto brickTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("dark-worn-stone-ue"));
		uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(brickTextures));
		auto goldTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("hammered-gold-ue"));
		uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(goldTextures));
		auto wornMetalTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("worn-shiny-metal-ue"));
		uploader.UploadTextureBatch(DX12Engine::TextureLoader::GetTextureArray(wornMetalTextures));

		std::shared_ptr<DX12Engine::PBRMaterial> pbrBrick = std::make_shared<DX12Engine::PBRMaterial>();
		pbrBrick->SetAllTextures(brickTextures);
		std::shared_ptr<DX12Engine::PBRMaterial> pbrGold = std::make_shared<DX12Engine::PBRMaterial>();
		pbrGold->SetAllTextures(goldTextures);
		std::shared_ptr<DX12Engine::PBRMaterial> pbrWornMetal = std::make_shared<DX12Engine::PBRMaterial>();
		pbrWornMetal->SetAllTextures(wornMetalTextures);

		DX12Engine::ModelLoader modelLoader;
		auto cubeMesh = std::make_shared<DX12Engine::Mesh>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("cube.obj")));
		auto sphereMesh = std::make_shared<DX12Engine::Mesh>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("sphere.obj")));
		auto floorMesh = std::make_shared<DX12Engine::Mesh>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("floor.obj")));

		m_Cube = std::make_shared<DX12Engine::GameObject>();
		m_Cube->SetMesh(cubeMesh);
		m_Cube->Move({ -1.5f, 1.0f, 0.0f });
		DX12Engine::RenderComponent* cubeRenderComp = m_Cube->CreateComponent<DX12Engine::RenderComponent>();
		cubeRenderComp->SetMaterial(pbrBrick);
		DX12Engine::PhysicsComponent* cubePhysicsComp = m_Cube->CreateComponent<DX12Engine::PhysicsComponent>();
		cubePhysicsComp->SetMass(6.0f);
		cubePhysicsComp->SetCollisionMeshType(DX12Engine::CollisionMeshType::Box);
		m_SceneObjects.Add("Cube", m_Cube);

		m_Ball = std::make_shared<DX12Engine::GameObject>();
		m_Ball->SetMesh(sphereMesh);
		m_Ball->Move({ 1.5f, 1.0f, 0.0f });
		DX12Engine::RenderComponent* ballRenderComp = m_Ball->CreateComponent<DX12Engine::RenderComponent>();
		ballRenderComp->SetMaterial(pbrGold);
		DX12Engine::PhysicsComponent* ballPhysicsComp = m_Ball->CreateComponent<DX12Engine::PhysicsComponent>();
		ballPhysicsComp->SetMass(4.0f);
		ballPhysicsComp->SetCollisionMeshType(DX12Engine::CollisionMeshType::Sphere);
		m_SceneObjects.Add("Ball", m_Ball);

		std::shared_ptr<DX12Engine::GameObject> floor = std::make_shared<DX12Engine::GameObject>();
		floor->SetMesh(floorMesh);
		floor->Move({ 0.0f, -1.0f, 0.0f });
		DX12Engine::RenderComponent* floorRenderComp = floor->CreateComponent<DX12Engine::RenderComponent>();
		floorRenderComp->SetMaterial(pbrWornMetal);
		DX12Engine::PhysicsComponent* floorPhysicsComp = floor->CreateComponent<DX12Engine::PhysicsComponent>();
		floorPhysicsComp->SetIsStatic(true);
		floorPhysicsComp->SetCollisionMeshType(DX12Engine::CollisionMeshType::Plane);
		m_SceneObjects.Add("Floor", floor);

		m_PhysicsEngine->SetComponents(m_SceneObjects.GetAllComponents<DX12Engine::PhysicsComponent>());
		m_SceneObjects.Get("Cube")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { 200.0f, 700.0f, 0.0f }, 0.05f });
		m_SceneObjects.Get("Ball")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { -200.0, 300.0f, 0.0f }, 0.05f });
	}

	void DemoScene::Update(float ts, float elapsed)
	{
		m_Camera->Update(ts);
		m_SceneObjects.Update(ts, elapsed);
	}

	void DemoScene::OnResize(DirectX::XMFLOAT2 newSize)
	{
		m_WindowSize = newSize;
		if (m_Camera)
			m_Camera->SetAspectRatio(newSize.x / newSize.y);
	}
}
