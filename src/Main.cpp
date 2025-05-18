#include "./ClientApplication.h"
#include "DX12Engine/Launcher.h"

int main() 
{
	int windowSize[2] = { 1920, 1080 };
	ClientApplication app;
	DX12Engine::Launcher::Launch(&app, windowSize);
}