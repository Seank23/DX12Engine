#pragma once
#include <memory>

#include "./DX12Engine/Application.h"
#include "./DX12Engine/Input/Camera.h"

class ClientApplication : public DX12Engine::Application
{

public:
	ClientApplication();
	~ClientApplication();

	virtual void HandleMouseMovement(HWND hwnd, LPARAM lParam) override;

private:
	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
	bool m_FirstMouse = true;

	std::unique_ptr<DX12Engine::Camera> m_Camera;
};

