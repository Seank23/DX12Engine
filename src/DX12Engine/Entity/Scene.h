#pragma once
#include "GameObject.h"
#include "../Rendering/Buffers/LightBuffer.h"
#include "../Input/Camera.h"

namespace DX12Engine
{
	class Texture;

	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		virtual void Init() = 0;
		virtual void Update(float ts, float elapsed) = 0;
		virtual void OnResize(DirectX::XMFLOAT2 newSize) = 0;
		virtual GameObject* GetInteractiveObject() = 0;

		GameObjectContainer& GetSceneObjects() { return m_SceneObjects; }
		LightBuffer* GetLightBuffer() { return m_LightBuffer.get(); }
		Camera* GetCamera() { return m_Camera.get(); }
		Texture* GetSkyboxCubemap() { return m_SkyboxCubemap.get(); }
		Texture* GetSkyboxIrradiance() { return m_SkyboxIrradiance.get(); }

	protected:
		GameObjectContainer m_SceneObjects;
		std::unique_ptr<LightBuffer> m_LightBuffer;
		std::unique_ptr<Camera> m_Camera;

		std::shared_ptr<Texture> m_SkyboxCubemap;
		std::shared_ptr<Texture> m_SkyboxIrradiance;
	};
}