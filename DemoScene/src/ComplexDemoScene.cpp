#include "ComplexDemoScene.h"

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
#include "DX12Engine/Entity/AnimationComponent.h"

namespace DX12EngineDemo
{
	ComplexDemoScene::ComplexDemoScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize)
		: m_RenderContext(renderContext), m_PhysicsEngine(physicsEngine), m_WindowSize(windowSize)
	{
	}

	void ComplexDemoScene::Init()
	{
		m_Camera = std::make_unique<DX12Engine::Camera>(m_WindowSize.x / m_WindowSize.y, 0.1f, 1000.0f);
		m_Camera->SetPosition({ -10.0f, 5.0f, -20.0f });
		m_Camera->SetRotation(-3.0f, 60.0f);
		m_Camera->SetSpeed(5.0f);

		m_LightBuffer = std::make_unique<DX12Engine::LightBuffer>();
		std::shared_ptr<DX12Engine::Light> sunLight = std::make_shared<DX12Engine::Light>();
		sunLight->SetDirection({ 0.45f, -0.577f, 0.577f });
		sunLight->SetIntensity(5.0f);
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

		auto wornMetalTextures = textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath("worn-shiny-metal-ue"));

		std::shared_ptr<DX12Engine::PBRMaterial> pbrWornMetal = std::make_shared<DX12Engine::PBRMaterial>();
		pbrWornMetal->SetAllTextures(wornMetalTextures);

		DX12Engine::ModelLoader modelLoader;
		auto mazdaModel = modelLoader.LoadCookedModel("Mazda3_Anim");
		auto floorMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("floor.obj")));

		auto floorModel = std::make_shared<DX12Engine::ModelAsset>("Floor");
		size_t floorMeshIdx = floorModel->AddMesh(floorMesh);
		floorModel->AddMaterial(std::make_shared<DX12Engine::MaterialAsset>("Material", pbrWornMetal));
		DX12Engine::ModelNode floorRoot; floorRoot.Name = "Root"; floorRoot.ParentIndex = -1; floorRoot.MeshIndex = -1;
		size_t floorRootIdx = floorModel->AddNode(floorRoot);
		DX12Engine::ModelNode floorNode; floorNode.Name = "Floor"; floorNode.ParentIndex = (int)floorRootIdx; floorNode.MeshIndex = floorMeshIdx;
		size_t floorNodeIdx = floorModel->AddNode(floorNode);

		std::shared_ptr<DX12Engine::GameObject> floor = std::make_shared<DX12Engine::GameObject>();
		floor->Move({ 0.0f, -0.45f, 0.0f });
		floor->Scale({ 4.0f, 1.0f, 4.0f });
		floor->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(floorModel));
		DX12Engine::ColliderComponent* floorColliderComp = floor->CreateComponent<DX12Engine::ColliderComponent>(DX12Engine::CollisionMeshType::Plane);
		floorColliderComp->SetUseRenderModelForCollision(true);
		DX12Engine::PhysicsComponent* floorPhysicsComp = floor->CreateComponent<DX12Engine::PhysicsComponent>();
		floorPhysicsComp->SetIsStatic(true);
		floorPhysicsComp->SetRestitution(0.4f);
		floorPhysicsComp->SetStaticFriction(0.3f);
		floorPhysicsComp->SetKineticFriction(0.2f);
		m_SceneObjects.Add("Floor", floor);

		std::shared_ptr<DX12Engine::GameObject> mazda = std::make_shared<DX12Engine::GameObject>();
		mazda->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(mazdaModel));
		mazda->Scale({ 5.0f, 5.0f, 5.0f });
		mazda->CreateComponent<DX12Engine::AnimationComponent>();
		DX12Engine::ColliderComponent* mazdaColliderComp = mazda->CreateComponent<DX12Engine::ColliderComponent>(DX12Engine::CollisionMeshType::Box);
		mazdaColliderComp->SetUseRenderModelForCollision(true);
		DX12Engine::PhysicsComponent* mazdaPhysicsComp = mazda->CreateComponent<DX12Engine::PhysicsComponent>();
		mazdaPhysicsComp->SetMass(1300.0f);
		mazdaPhysicsComp->SetRestitution(0.2f);
		mazdaPhysicsComp->SetStaticFriction(0.6f);
		mazdaPhysicsComp->SetKineticFriction(0.5f);
		m_SceneObjects.Add("Mazda", mazda);

		m_SceneObjects.Init();
	}

	void ComplexDemoScene::Update(float ts, float elapsed)
	{
		m_Camera->Update(ts);
		m_SceneObjects.Get("Mazda")->Rotate({ 0.0f, ts * 10.0f, 0.0f });
		m_SceneObjects.Update(ts, elapsed);
	}

	void ComplexDemoScene::OnResize(DirectX::XMFLOAT2 newSize)
	{
		m_WindowSize = newSize;
		if (m_Camera)
			m_Camera->SetAspectRatio(newSize.x / newSize.y);
	}

	DX12Engine::GameObject* ComplexDemoScene::GetInteractiveObject()
	{
		return m_SceneObjects.Get("Mazda").get();
	}
}
