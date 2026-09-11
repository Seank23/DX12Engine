#include "ClusteredLightingScene.h"

#include "DX12Engine/Asset/ModelAsset.h"
#include "DX12Engine/Entity/RenderComponent.h"
#include "DX12Engine/IO/ModelLoader.h"
#include "DX12Engine/IO/TextureLoader.h"
#include "DX12Engine/Resources/Light.h"
#include "DX12Engine/Resources/Materials/PBRMaterial.h"
#include "DX12Engine/Resources/ResourceManager.h"

#include <algorithm>
#include <cmath>

namespace DX12EngineDemo
{
	namespace
	{
		// The hall runs along +Z from the camera. Depth slices are spaced exponentially
		// between the near and far planes, so a scene has to be genuinely long before the
		// outer slices see anything at all.
		constexpr float HALL_START_Z = -20.0f;
		constexpr float HALL_END_Z = 160.0f;
		constexpr float HALL_HALF_WIDTH = 14.0f;
		constexpr float WALL_HEIGHT = 9.0f;
		constexpr float PILLAR_HEIGHT = 5.0f;
		constexpr float PILLAR_OFFSET_X = 8.0f;
		constexpr int PILLAR_ROWS = 16;
		constexpr int ACCENT_SPHERE_COUNT = 8;

		// far/near is the only thing that sets the slice distribution: 0.1 to 300 spends
		// slices 0-5 inside the first metre and the remaining 18 on the length of the hall.
		constexpr float CAMERA_NEAR = 0.1f;
		constexpr float CAMERA_FAR = 300.0f;

		// The only lights in the scene that get a shadow map. Everything else opts out -
		// that asymmetry is the point: hundreds of lights, a handful of maps.
		constexpr int SHADOWED_SPOT_COUNT = 3;
		constexpr int SHADOWED_POINT_COUNT = 2;

		// The stress dial. Cost scales with the froxel *volume* the lights cover, not with
		// their count: the same light near the camera lands in far more clusters than it does
		// at the end of the hall, because near tiles and near slices are small. At these
		// values the grid sits at ~30% of MAX_LIGHT_INDICES with peaks of 70-odd lights in the
		// worst cluster; RANGE back up to 6 is enough on its own to exhaust the index list and
		// trip the drop warning, because demand goes with the cube of it.
		constexpr int LIGHT_GRID_COLUMNS = 10;
		constexpr int LIGHT_GRID_ROWS = 24;
		constexpr float GRID_LIGHT_RANGE = 6.0f;
		constexpr float GRID_LIGHT_INTENSITY = 18.0f;
		constexpr float GRID_LIGHT_HEIGHT = 3.5f;
		constexpr float GRID_LIGHT_BOB = 1.5f;

		// Packed into one small ball so they all land in the same few froxels, then orbited
		// through the field. MAX_LIGHT_INDICES budgets an average of 16 lights per cluster
		// across the whole grid and nothing caps an individual one, so this is what drives
		// peak occupancy far past that average without costing much total budget.
		constexpr int OVERLAP_LIGHT_COUNT = 24;
		constexpr float OVERLAP_LIGHT_RANGE = 9.0f;
		constexpr float OVERLAP_LIGHT_INTENSITY = 10.0f;
		constexpr float OVERLAP_LIGHT_RADIUS = 1.5f;
		constexpr float OVERLAP_CENTRE_Y = 4.0f;
		constexpr float OVERLAP_CENTRE_Z = 34.0f;
		constexpr float OVERLAP_ORBIT_RADIUS = 6.0f;

		DirectX::XMFLOAT3 LightHue(float hue)
		{
			hue -= floorf(hue);
			const float r = std::clamp(fabsf(hue * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
			const float g = std::clamp(2.0f - fabsf(hue * 6.0f - 2.0f), 0.0f, 1.0f);
			const float b = std::clamp(2.0f - fabsf(hue * 6.0f - 4.0f), 0.0f, 1.0f);
			// Pulled off full saturation deliberately: a few hundred fully saturated lights
			// read as coloured noise, and the thing worth seeing is where each pool ends.
			constexpr float wash = 0.3f;
			return { r + (1.0f - r) * wash, g + (1.0f - g) * wash, b + (1.0f - b) * wash };
		}

		std::shared_ptr<DX12Engine::ModelAsset> MakeSingleMeshModel(const std::string& name, std::shared_ptr<DX12Engine::MeshAsset> mesh, std::shared_ptr<DX12Engine::PBRMaterial> material)
		{
			auto model = std::make_shared<DX12Engine::ModelAsset>(name);
			const std::size_t meshIndex = model->AddMesh(std::move(mesh));
			model->AddMaterial(std::make_shared<DX12Engine::MaterialAsset>("Material", std::move(material)));

			DX12Engine::ModelNode root;
			root.Name = "Root";
			root.ParentIndex = -1;
			root.MeshIndex = -1;
			const std::size_t rootIndex = model->AddNode(root);

			DX12Engine::ModelNode node;
			node.Name = name;
			node.ParentIndex = static_cast<int>(rootIndex);
			node.MeshIndex = static_cast<int>(meshIndex);
			model->AddNode(node);
			return model;
		}
	}

	ClusteredLightingScene::ClusteredLightingScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize)
		: m_RenderContext(renderContext), m_PhysicsEngine(physicsEngine), m_WindowSize(windowSize)
	{
	}

	void ClusteredLightingScene::Init()
	{
		m_Camera = std::make_unique<DX12Engine::Camera>(m_WindowSize.x / m_WindowSize.y, CAMERA_NEAR, CAMERA_FAR);
		m_Camera->SetPosition({ 0.0f, 5.0f, HALL_START_Z + 4.0f });
		m_Camera->SetRotation(-4.0f, 90.0f); // yaw 90 faces +Z, straight down the hall
		m_Camera->SetSpeed(12.0f);

		BuildLights();
		BuildGeometry();
	}

	void ClusteredLightingScene::BuildLights()
	{
		m_LightBuffer = std::make_unique<DX12Engine::LightBuffer>();

		// Deliberately dim. A sun bright enough to light the hall on its own would wash out
		// every local pool and there would be nothing left to look at.
		std::shared_ptr<DX12Engine::Light> sun = std::make_shared<DX12Engine::Light>();
		sun->SetDirection({ 0.35f, -0.85f, 0.4f });
		sun->SetIntensity(0.2f);
		sun->SetColor({ 0.55f, 0.6f, 0.8f });
		m_LightBuffer->AddLight(sun);

		// Added before the rest so they claim the shadow slots, which are handed out in
		// insertion order.
		for (int i = 0; i < SHADOWED_SPOT_COUNT; i++)
		{
			std::shared_ptr<DX12Engine::Light> spot = std::make_shared<DX12Engine::Light>();
			spot->SetType((int)DX12Engine::LightType::Spot);
			spot->SetPosition({ 0.0f, WALL_HEIGHT - 1.0f, HALL_START_Z + 25.0f + (float)i * 45.0f });
			spot->SetDirection({ 0.0f, -1.0f, 0.25f });
			spot->SetRange(35.0f);
			spot->SetSpotAngle(35.0f);
			spot->SetIntensity(9.0f);
			spot->SetColor({ 1.0f, 0.95f, 0.85f });
			m_LightBuffer->AddLight(spot);
			m_SweepSpots.push_back(spot);
		}

		for (int i = 0; i < SHADOWED_POINT_COUNT; i++)
		{
			std::shared_ptr<DX12Engine::Light> point = std::make_shared<DX12Engine::Light>();
			point->SetType((int)DX12Engine::LightType::Point);
			// Deliberately off the pillar line: a point light sitting on it ends up inside the
			// 0.5-radius cylinder of whichever row it lands next to, and its cube map then
			// reports the pillar's inner wall as the nearest occluder in nearly every
			// direction - the light renders six shadow faces and lights nothing.
			point->SetPosition({ (i % 2 == 0) ? -PILLAR_OFFSET_X * 0.55f : PILLAR_OFFSET_X * 0.55f, 4.0f, HALL_START_Z + 20.0f + (float)i * 24.0f });
			point->SetRange(18.0f);
			point->SetIntensity(30.0f);
			point->SetColor({ 1.0f, 0.72f, 0.4f });
			m_LightBuffer->AddLight(point);
		}

		// The bread and butter: a regular field of small lights spanning the hall. The pools
		// overlap by design, so cluster occupancy climbs into the range the index budget
		// assumes instead of sitting at one or two everywhere.
		m_GridLights.reserve(LIGHT_GRID_COLUMNS * LIGHT_GRID_ROWS);
		for (int row = 0; row < LIGHT_GRID_ROWS; row++)
		{
			for (int column = 0; column < LIGHT_GRID_COLUMNS; column++)
			{
				const float u = (LIGHT_GRID_COLUMNS == 1) ? 0.5f : (float)column / (float)(LIGHT_GRID_COLUMNS - 1);
				const float v = (LIGHT_GRID_ROWS == 1) ? 0.5f : (float)row / (float)(LIGHT_GRID_ROWS - 1);
				const DirectX::XMFLOAT3 anchor{
					(u * 2.0f - 1.0f) * (HALL_HALF_WIDTH - 2.0f),
					GRID_LIGHT_HEIGHT,
					HALL_START_Z + 8.0f + v * ((HALL_END_Z - HALL_START_Z) - 16.0f)
				};

				std::shared_ptr<DX12Engine::Light> light = std::make_shared<DX12Engine::Light>();
				light->SetType((int)DX12Engine::LightType::Point);
				light->SetCastsShadows(false);
				light->SetPosition(anchor);
				light->SetRange(GRID_LIGHT_RANGE);
				light->SetIntensity(GRID_LIGHT_INTENSITY);
				light->SetColor(LightHue(u * 0.35f + v * 0.65f));
				m_LightBuffer->AddLight(light);

				m_GridLights.push_back({ light, anchor, (float)(row * LIGHT_GRID_COLUMNS + column) * 0.37f });
			}
		}

		// Spread over a golden-angle spiral rather than a ring so they do not line up into
		// bands that happen to straddle cluster edges evenly.
		m_OverlapLights.reserve(OVERLAP_LIGHT_COUNT);
		for (int i = 0; i < OVERLAP_LIGHT_COUNT; i++)
		{
			const float angle = (float)i * 2.39996f;
			const float radius = OVERLAP_LIGHT_RADIUS * sqrtf((float)i / (float)OVERLAP_LIGHT_COUNT);
			const DirectX::XMFLOAT3 anchor{
				cosf(angle) * radius,
				OVERLAP_CENTRE_Y + sinf(angle) * radius * 0.5f,
				OVERLAP_CENTRE_Z + sinf(angle) * radius
			};

			std::shared_ptr<DX12Engine::Light> light = std::make_shared<DX12Engine::Light>();
			light->SetType((int)DX12Engine::LightType::Point);
			light->SetCastsShadows(false);
			light->SetPosition(anchor);
			light->SetRange(OVERLAP_LIGHT_RANGE);
			light->SetIntensity(OVERLAP_LIGHT_INTENSITY);
			light->SetColor(LightHue((float)i / (float)OVERLAP_LIGHT_COUNT));
			m_LightBuffer->AddLight(light);

			m_OverlapLights.push_back({ light, anchor, angle });
		}
	}

	void ClusteredLightingScene::BuildGeometry()
	{
		DX12Engine::TextureLoader textureLoader;
		m_SkyboxCubemap = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_cubemap.dds"));
		m_SkyboxIrradiance = textureLoader.LoadCubemapDDS(DX12Engine::ResourceManager::GetMaterialPath("skybox/skybox_irradiance.dds"));

		auto loadMaterial = [&textureLoader](const char* folder)
		{
			std::shared_ptr<DX12Engine::PBRMaterial> material = std::make_shared<DX12Engine::PBRMaterial>();
			material->SetAllTextures(textureLoader.LoadMaterial(DX12Engine::ResourceManager::GetMaterialPath(folder)));
			return material;
		};

		// Metal, so the hall reflects. On a metal floor kD = (1 - F)(1 - metallic) is ~0 and a
		// local light contributes only its specular lobe, which a full-strength sky reflection
		// drowns - hence RendererOptions::AmbientIntensity below 1 in the demo app.
		std::shared_ptr<DX12Engine::PBRMaterial> metal = loadMaterial("worn-shiny-metal-ue");
		std::shared_ptr<DX12Engine::PBRMaterial> concrete = loadMaterial("clean-concrete-ue");
		std::shared_ptr<DX12Engine::PBRMaterial> gold = loadMaterial("hammered-gold-ue");

		DX12Engine::ModelLoader modelLoader;
		auto slabMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("floor.obj")));
		auto cubeMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("cube.obj")));
		auto cylinderMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("cylinder.obj")));
		auto sphereMesh = std::make_shared<DX12Engine::MeshAsset>(modelLoader.LoadObj(DX12Engine::ResourceManager::GetModelPath("sphere.obj")));

		// One ModelAsset per shape, shared by every instance of it: the mesh uploads once and
		// each GameObject resolves its own primitive bindings and constant buffer.
		auto floorModel = MakeSingleMeshModel("Floor", slabMesh, metal);
		auto wallModel = MakeSingleMeshModel("Wall", cubeMesh, concrete);
		auto pillarModel = MakeSingleMeshModel("Pillar", cylinderMesh, concrete);
		auto sphereModel = MakeSingleMeshModel("Sphere", sphereMesh, gold);

		const float hallLength = HALL_END_Z - HALL_START_Z;
		const float hallCentreZ = (HALL_START_Z + HALL_END_Z) * 0.5f;

		// floor.obj is a 10x10 slab; cube, cylinder and sphere are all 2 units across.
		SpawnObject("Floor", floorModel, { 0.0f, 0.0f, hallCentreZ }, { HALL_HALF_WIDTH / 5.0f, 1.0f, hallLength / 10.0f });
		SpawnObject("WallLeft", wallModel, { -HALL_HALF_WIDTH, WALL_HEIGHT * 0.5f, hallCentreZ }, { 0.4f, WALL_HEIGHT * 0.5f, hallLength * 0.5f });
		SpawnObject("WallRight", wallModel, { HALL_HALF_WIDTH, WALL_HEIGHT * 0.5f, hallCentreZ }, { 0.4f, WALL_HEIGHT * 0.5f, hallLength * 0.5f });
		SpawnObject("WallBack", wallModel, { 0.0f, WALL_HEIGHT * 0.5f, HALL_END_Z }, { HALL_HALF_WIDTH, WALL_HEIGHT * 0.5f, 0.4f });

		for (int row = 0; row < PILLAR_ROWS; row++)
		{
			const float z = HALL_START_Z + 10.0f + (float)row * ((hallLength - 20.0f) / (float)(PILLAR_ROWS - 1));
			for (int side = 0; side < 2; side++)
			{
				const float x = (side == 0) ? -PILLAR_OFFSET_X : PILLAR_OFFSET_X;
				SpawnObject("Pillar" + std::to_string(row) + "_" + std::to_string(side), pillarModel,
							{ x, PILLAR_HEIGHT * 0.5f, z }, { 0.5f, PILLAR_HEIGHT * 0.5f, 0.5f });
			}
		}

		// Glossy, so a light's pool reads as a moving specular hit and not only as diffuse
		// falloff - it makes the boundary between two clusters far easier to see.
		for (int i = 0; i < ACCENT_SPHERE_COUNT; i++)
		{
			const float t = (float)i / (float)(ACCENT_SPHERE_COUNT - 1);
			const float z = HALL_START_Z + 15.0f + t * (hallLength - 30.0f);
			const float x = ((i % 2) == 0) ? -4.0f : 4.0f;
			auto sphere = SpawnObject("Sphere" + std::to_string(i), sphereModel, { x, 2.5f, z }, { 1.2f, 1.2f, 1.2f });
			if (!m_InteractiveObject)
				m_InteractiveObject = sphere;
		}

		m_SceneObjects.Init();
	}

	std::shared_ptr<DX12Engine::GameObject> ClusteredLightingScene::SpawnObject(const std::string& name, std::shared_ptr<DX12Engine::ModelAsset> model, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 scale)
	{
		std::shared_ptr<DX12Engine::GameObject> object = std::make_shared<DX12Engine::GameObject>();
		object->CreateComponent<DX12Engine::RenderComponent>(std::make_shared<DX12Engine::ModelInstance>(std::move(model)));
		object->Scale({ scale.x, scale.y, scale.z });
		object->Move({ position.x, position.y, position.z });
		m_SceneObjects.Add(name, object);
		return object;
	}

	void ClusteredLightingScene::Update(float ts, float elapsed)
	{
		m_Time += ts;
		m_Camera->Update(ts);

		// Every light is positioned from its anchor and the clock, so the field keeps moving
		// without drifting. Motion matters here: a static light field would let a stale or
		// wrongly cached froxel assignment still look correct.
		for (const AnimatedLight& entry : m_GridLights)
		{
			const float bob = sinf(m_Time * 0.9f + entry.Phase) * GRID_LIGHT_BOB;
			entry.Source->SetPosition({ entry.Anchor.x, entry.Anchor.y + bob, entry.Anchor.z });
		}

		const float orbit = m_Time * 0.45f;
		for (const AnimatedLight& entry : m_OverlapLights)
		{
			entry.Source->SetPosition({
				entry.Anchor.x + cosf(orbit) * OVERLAP_ORBIT_RADIUS,
				entry.Anchor.y,
				entry.Anchor.z + sinf(orbit) * OVERLAP_ORBIT_RADIUS });
		}

		for (std::size_t i = 0; i < m_SweepSpots.size(); i++)
		{
			const float sweep = sinf(m_Time * 0.35f + (float)i * 1.7f) * 0.55f;
			m_SweepSpots[i]->SetDirection({ sweep, -1.0f, 0.25f });
		}

		m_SceneObjects.Update(ts, elapsed);
	}

	void ClusteredLightingScene::OnResize(DirectX::XMFLOAT2 newSize)
	{
		m_WindowSize = newSize;
		if (m_Camera)
			m_Camera->SetAspectRatio(newSize.x / newSize.y);
	}

	DX12Engine::GameObject* ClusteredLightingScene::GetInteractiveObject()
	{
		return m_InteractiveObject.get();
	}
}
