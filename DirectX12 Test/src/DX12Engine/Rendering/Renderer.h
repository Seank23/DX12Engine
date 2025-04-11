#pragma once
#include "RenderContext.h"
#include "RenderObject.h"
#include "../Resources/Texture.h"
#include "../Heaps/RenderPassDescriptorHeap.h"
#include "../Buffers/LightBuffer.h"
#include "../Input/Camera.h"
#include "../Resources/Skybox.h"
#include "../Resources/DepthMap.h"

namespace DX12Engine
{
	class Renderer
	{
	public:
		Renderer(std::shared_ptr<RenderContext> context);
		~Renderer();

		void InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
		void Render(RenderObject* renderObject);
		void RenderObjectList(std::vector<RenderObject*> objects);
		void PresentFrame();
		bool PollWindow();

		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }
		void SetCamera(Camera* camera) { m_Camera = camera; }
		void SetSkybox(Skybox* skybox) { m_Skybox = skybox; }
		void SetShadowMap(DepthMap* shadowMap) { m_ShadowMap = shadowMap; }
		void SetShadowCubeMap(DepthMap* shadowCubeMap) { m_ShadowCubeMap = shadowCubeMap; }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		void RenderSkybox();

		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList* m_CommandList;

		RenderPassDescriptorHeap& m_RenderHeap;
		LightBuffer* m_LightBuffer;

		Camera* m_Camera;
		Skybox* m_Skybox;
		DepthMap* m_ShadowMap;
		DepthMap* m_ShadowCubeMap;
	};
}

