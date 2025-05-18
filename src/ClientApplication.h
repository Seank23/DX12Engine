#pragma once
#include <memory>

#include "./DX12Engine/Application.h"
#include "./DX12Engine/Input/Camera.h"
#include "DX12Engine/Rendering/Buffers/LightBuffer.h"
#include "DX12Engine/Rendering/Renderer.h"
#include "DX12Engine/Rendering/RenderContext.h"

class ClientApplication : public DX12Engine::Application
{

public:
	ClientApplication();
	~ClientApplication();

	virtual void Init(std::shared_ptr<DX12Engine::RenderContext> renderContext, DirectX::XMFLOAT2 windowSize) override;
	virtual void Update(float ts, float elapsed) override;

	virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam) override;

private:
	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
	bool m_FirstMouse = true;

	std::unique_ptr<DX12Engine::Camera> m_Camera;
	std::unique_ptr<DX12Engine::Renderer> m_Renderer;
	std::unique_ptr<DX12Engine::LightBuffer> m_LightBuffer;
	DX12Engine::RenderPipeline m_RenderPipeline;

	std::vector<std::shared_ptr<DX12Engine::RenderObject>> m_SceneObjects;
};

