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
#include "DX12Engine/Asset/ModelAsset.h"
#include "DX12Engine/Entity/ColliderComponent.h"

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
		sunLight->SetIntensity(10.0f);
		sunLight->SetColor({ 1.0f, 0.85f, 0.8f });
		m_LightBuffer->AddLight(sunLight);
		std::shared_ptr<DX12Engine::Light> pointLight = std::make_shared<DX12Engine::Light>();
		pointLight->SetType((int)DX12Engine::LightType::Point);
		pointLight->SetPosition({ 0.0f, 1.5f, -0.8f });
		pointLight->SetIntensity(20.0f);
		pointLight->SetRange(200.0f);
		pointLight->SetColor({ 1.0f, 1.0f, 1.0f });
		//m_LightBuffer->AddLight(pointLight);
		std::shared_ptr<DX12Engine::Light> spotLight = std::make_shared<DX12Engine::Light>();
		spotLight->SetType((int)DX12Engine::LightType::Spot);
		spotLight->SetPosition({ 1.0f, 6.0f, -1.0f });
		spotLight->SetDirection({ -0.1f, -0.8f, 0.1f });
		spotLight->SetColor({ 0.9f, 0.5f, 0.0f });
		spotLight->SetIntensity(20.0f);
		spotLight->SetSpotAngle(45.0f);
		//m_LightBuffer->AddLight(spotLight);

		DX12Engine::TextureLoader textureLoader;
		m_SkyboxCubemap = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_cubemap.dds"));
		m_SkyboxIrradiance = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_irradiance.dds"));

		auto brickTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("dark-worn-stone-ue"));
		auto goldTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("hammered-gold-ue"));
		auto wornMetalTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("worn-shiny-metal-ue"));

		std::shared_ptr<DX12Engine::PBRMaterial> pbrBrick = std::make_shared<DX12Engine::PBRMaterial>();
		pbrBrick->SetAllTextures(brickTextures);
		std::shared_ptr<DX12Engine::PBRMaterial> pbrGold = std::make_shared<DX12Engine::PBRMaterial>();
		pbrGold->SetAllTextures(goldTextures);
		std::shared_ptr<DX12Engine::PBRMaterial> pbrWornMetal = std::make_shared<DX12Engine::PBRMaterial>();
		pbrWornMetal->SetAllTextures(wornMetalTextures);

		DX12Engine::ModelLoader modelLoader;
		auto cubeMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("cube.obj")));
		auto sphereMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("sphere.obj")));
		auto floorMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("floor.obj")));

		auto cubeModel = std::make_shared<DX12Engine::ModelAsset>("Cube");
		size_t cubeMeshIdx = cubeModel->AddMesh(cubeMesh);
		cubeModel->AddMaterial(std::make_shared<DX12Engine::MaterialAsset>("Material", pbrBrick));
		DX12Engine::ModelNode cubeRoot; cubeRoot.Name = "Root"; cubeRoot.ParentIndex = -1; cubeRoot.MeshIndex = -1;
		size_t cubeRootIdx = cubeModel->AddNode(cubeRoot);
		DX12Engine::ModelNode cubeNode; cubeNode.Name = "Cube"; cubeNode.ParentIndex = (int)cubeRootIdx; cubeNode.MeshIndex = cubeMeshIdx;
		size_t cubeNodeIdx = cubeModel->AddNode(cubeNode);

		auto sphereModel = std::make_shared<DX12Engine::ModelAsset>("Sphere");
		size_t sphereMeshIdx = sphereModel->AddMesh(sphereMesh);
		sphereModel->AddMaterial(std::make_shared<DX12Engine::MaterialAsset>("Material", pbrGold));
		DX12Engine::ModelNode sphereRoot; sphereRoot.Name = "Root"; sphereRoot.ParentIndex = -1; sphereRoot.MeshIndex = -1;
		size_t sphereRootIdx = sphereModel->AddNode(sphereRoot);
		DX12Engine::ModelNode sphereNode; sphereNode.Name = "Sphere"; sphereNode.ParentIndex = (int)sphereRootIdx; sphereNode.MeshIndex = sphereMeshIdx;
		size_t sphereNodeIdx = sphereModel->AddNode(sphereNode);

		auto floorModel = std::make_shared<DX12Engine::ModelAsset>("Floor");
		size_t floorMeshIdx = floorModel->AddMesh(floorMesh);
		floorModel->AddMaterial(std::make_shared<DX12Engine::MaterialAsset>("Material", pbrWornMetal));
		DX12Engine::ModelNode floorRoot; floorRoot.Name = "Root"; floorRoot.ParentIndex = -1; floorRoot.MeshIndex = -1;
		size_t floorRootIdx = floorModel->AddNode(floorRoot);
		DX12Engine::ModelNode floorNode; floorNode.Name = "Floor"; floorNode.ParentIndex = (int)floorRootIdx; floorNode.MeshIndex = floorMeshIdx;
		size_t floorNodeIdx = floorModel->AddNode(floorNode);

		m_Cube = std::make_shared<DX12Engine::GameObject>();
		m_Cube->Move({ -1.5f, 1.0f, -0.0f });
		m_Cube->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(cubeModel));
		DX12Engine::ColliderComponent* cubeColliderComp = m_Cube->CreateComponent<DX12Engine::ColliderComponent>(DX12Engine::CollisionMeshType::Box);
		cubeColliderComp->SetUseRenderModelForCollision(true);
		DX12Engine::PhysicsComponent* cubePhysicsComp = m_Cube->CreateComponent<DX12Engine::PhysicsComponent>();
		cubePhysicsComp->SetMass(6.0f);
		cubePhysicsComp->SetRestitution(0.2f);
		cubePhysicsComp->SetStaticFriction(0.6f);
		cubePhysicsComp->SetKineticFriction(0.5f);
		m_SceneObjects.Add("Cube", m_Cube);

		m_Ball = std::make_shared<DX12Engine::GameObject>();
		m_Ball->Move({ 1.5f, 1.0f, 0.0f });
		m_Ball->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(sphereModel));
		DX12Engine::ColliderComponent* ballColliderComp = m_Ball->CreateComponent<DX12Engine::ColliderComponent>(DX12Engine::CollisionMeshType::Sphere);
		ballColliderComp->SetUseRenderModelForCollision(true);
		DX12Engine::PhysicsComponent* ballPhysicsComp = m_Ball->CreateComponent<DX12Engine::PhysicsComponent>();
		ballPhysicsComp->SetMass(4.0f);
		ballPhysicsComp->SetRestitution(0.3f);
		ballPhysicsComp->SetStaticFriction(0.4f);
		ballPhysicsComp->SetKineticFriction(0.3f);
		m_SceneObjects.Add("Ball", m_Ball);

		std::shared_ptr<DX12Engine::GameObject> floor = std::make_shared<DX12Engine::GameObject>();
		floor->Move({ 0.0f, -1.0f, 0.0f });
		floor->Scale({ 2.0f, 1.0f, 2.0f });
		DX12Engine::RenderComponent* floorRenderComp = floor->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(floorModel));
		DX12Engine::ColliderComponent* floorColliderComp = floor->CreateComponent<DX12Engine::ColliderComponent>(DX12Engine::CollisionMeshType::Plane);
		floorColliderComp->SetUseRenderModelForCollision(true);
		DX12Engine::PhysicsComponent* floorPhysicsComp = floor->CreateComponent<DX12Engine::PhysicsComponent>();
		floorPhysicsComp->SetIsStatic(true);
		floorPhysicsComp->SetRestitution(0.4f);
		floorPhysicsComp->SetStaticFriction(0.3f);
		floorPhysicsComp->SetKineticFriction(0.2f);
		m_SceneObjects.Add("Floor", floor);

		// Asphalt floor � high friction, low bounce
		/*std::shared_ptr<DX12Engine::GameObject> asphaltFloor = std::make_shared<DX12Engine::GameObject>();
		asphaltFloor->SetMesh(floorMesh);
		asphaltFloor->Move({ 0.0f, -1.0f, 0.0f });
		asphaltFloor->Scale({ 2.0f, 1.0f, 2.0f });
		DX12Engine::RenderComponent* asphaltRenderComp = asphaltFloor->CreateComponent<DX12Engine::RenderComponent>();
		asphaltRenderComp->SetMaterial(pbrWornMetal);
		DX12Engine::PhysicsComponent* asphaltPhysicsComp = asphaltFloor->CreateComponent<DX12Engine::PhysicsComponent>();
		asphaltPhysicsComp->SetIsStatic(true);
		asphaltPhysicsComp->SetRestitution(0.1f);
		asphaltPhysicsComp->SetStaticFriction(0.85f);
		asphaltPhysicsComp->SetKineticFriction(0.7f);
		asphaltPhysicsComp->SetCollisionMeshType(DX12Engine::CollisionMeshType::Plane);
		m_SceneObjects.Add("AsphaltFloor", asphaltFloor);*/

		m_PhysicsEngine->SetComponents(m_SceneObjects.GetAllComponents<DX12Engine::PhysicsComponent>());
		//m_SceneObjects.Get("Cube")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { 300.0f, 700.0f, 0.0f }, 0.05f, { -0.2f, 0.2f, 0.1f }, true, true });
		//m_SceneObjects.Get("Ball")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { -200.0, 300.0f, 0.0f }, 0.05f, { 0.5f, 0.7f, -0.2f }, true, true });
		m_SceneObjects.Get("Cube")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { 500.0f, 700.0f, 0.0f }, 0.05f });
		m_SceneObjects.Get("Ball")->GetComponent<DX12Engine::PhysicsComponent>()->ApplyForce(DX12Engine::Force{ { -200.0, 300.0f, 0.0f }, 0.05f });

		m_SceneObjects.Init();
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
