#include "DX12EngineDemoApp.h"

#include "DX12Engine/Launcher.h"

int main()
{
	int windowSize[2] = { 1920, 1080 };
	DX12EngineDemo::DX12EngineDemoApp app;
	DX12Engine::Launcher::Launch(&app, windowSize, "DX12Engine Demo");
}
